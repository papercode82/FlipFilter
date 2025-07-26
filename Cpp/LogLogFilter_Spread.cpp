
#include <fstream>
#include "header/LogLogFilter_Spread.h"

LogLogFilter_Spread::LogLogFilter_Spread(float memory_kb, Sketch* skt){
    m = int(memory_kb * 1024 * 8 / 5);
    r = 3;
    f = 0;
    phi = 0.77351;
    R.resize(m, 0);
    std::random_device rd;
    std::mt19937 gen(rd());
    type = 0;
    std::uniform_int_distribution<> dis(0, 1127483647);
    for (int i = 0; i < r; ++i) {
        seeds.push_back(dis(gen));
    }
    sketch = skt;
}

int LogLogFilter_Spread::get_leftmost(uint32_t random_val){
    int left_most = 0;
    while (random_val) {
        left_most += 1;
        random_val >>= 1;
    }
    return 32 - left_most;

}

void LogLogFilter_Spread::update(const uint32_t key, const uint32_t element){
    uint32_t hash_value = 0;
    uint32_t gamma = 0xffffffff;
    int del_ = (type > 0)? 10 : 5;
    for (int i = 0; i < r; ++i) {
        MurmurHash3_x86_32(&key, sizeof(key), seeds[i], &hash_value);
        int idx = hash_value % m;
        gamma = std::min(static_cast<uint32_t>(R[idx]), gamma);
    }
    if (gamma < del_) {
        uint64_t pair_ = (static_cast<uint64_t>(key) << 32) | element;
        bool is_new = (seen_pairs.find(pair_) == seen_pairs.end());
        if (is_new) {
            seen_pairs.insert(pair_);
            f += 1;
        }
        for (int i = 0; i < r; ++i) {
            MurmurHash3_x86_32(&key, sizeof(key), seeds[i], &hash_value);
            int idx = hash_value % m;
            uint32_t element_hash = 0;
            MurmurHash3_x86_32(&element, sizeof(element), seeds[i], &element_hash);
            uint32_t random_val = element_hash;
            int leftmost = get_leftmost(random_val);
            R[idx] = std::max(std::min(leftmost, del_), static_cast<int>(R[idx]));
        }
    } else {
        sketch->update(key, element);
    }
}


uint32_t LogLogFilter_Spread::query(const uint32_t key){
    uint32_t hash_value = 0;
    double filter_est = 0.0;
    double reg_sum = 0.0;
    uint32_t gamma = 0xffffffff;
    int del_ = (type > 0)? 10 : 5;
    for (int i = 0; i < r; ++i) {
        MurmurHash3_x86_32(&key, sizeof(key), seeds[i], &hash_value);
        int idx = hash_value % m;
        gamma = std::min(gamma, static_cast<uint32_t>(R[idx]));
        reg_sum += R[idx];
    }
    reg_sum = std::pow(2, reg_sum / r);
    filter_est = (m * r) / (m - r) * (1 / (r * phi) * reg_sum - f / m);
    uint32_t sketch_est = 0;
    if (gamma >= del_) {
        sketch_est = sketch->query(key);
    }
    uint32_t rounded = (filter_est > 0.0) ? static_cast<uint32_t>(filter_est) : -filter_est;
    uint32_t value_es = rounded + sketch_est;
    return value_es;
}




void LogLogFilter_Spread::spreadEstimation(
        const std::vector<std::pair<uint32_t, uint32_t>>& dataset,
        const std::unordered_map<uint32_t, std::unordered_set<uint32_t>>& true_cardi) {
    type = 1;
    for (const auto& [key, element] : dataset) {
        update(key, element);
    }
    float total_are = 0.0f;
    uint32_t count = 0;
    for (const auto& entry : true_cardi) {
        uint32_t flow_label = entry.first;
        uint32_t true_value = entry.second.size();
        uint32_t estimated_value = query(flow_label);
        if (true_value > 0) {
            float are = std::abs(static_cast<float>(estimated_value) - static_cast<float>(true_value)) / static_cast<float>(true_value);
            total_are += are;
            ++count;
        }
    }
    if (count > 0) {
        float avg_are = total_are / count;
        std::cout << "ARE: " << avg_are << std::endl;
    } else {
        std::cout << "No data to calculate ARE." << std::endl;
    }
}



void LogLogFilter_Spread::throughput(const std::vector<std::pair<uint32_t, uint32_t>>& dataset,
                                 const std::unordered_map<uint32_t, std::unordered_set<uint32_t>>& true_cardi){

    // time for starting update
    auto start_update = std::chrono::high_resolution_clock::now();
    // the process of traffic
    for (const auto& [key, element] : dataset) {
        update(key, element);
    }
    // time for end
    auto end_update = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> update_duration = end_update - start_update;
    double update_throughput_Mpps = (dataset.size() / 1e6) / update_duration.count();

    // time for starting query
    auto start_query = std::chrono::high_resolution_clock::now();
    for (const auto& [flow_label, ele_set] : true_cardi) {
        query(flow_label);
    }
    auto end_query = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> query_duration = end_query - start_query;
    double query_throughput_Mfps = (true_cardi.size() / 1e6) / query_duration.count();

    std::cout << "update Throughput: " << update_throughput_Mpps << " Mips" << std::endl;
    std::cout << "Query Throughput: " << query_throughput_Mfps << " Mfps" << std::endl;
}



std::unordered_map<uint32_t, uint32_t> LogLogFilter_Spread::detect(uint32_t threshold) {
    std::unordered_map<uint32_t, uint32_t> result;
    std::unordered_map<uint32_t, uint32_t> detected_ss;
    detected_ss = sketch->candidates();
    for (const auto& [key, estimated] : detected_ss) {
        uint32_t es = estimated + que(key);
        if ( es > threshold) {
            result[key] = es;
        }
    }
    return result;
}



uint32_t LogLogFilter_Spread::que(const uint32_t key){
    uint32_t hash_value = 0;
    double filter_est = 0.0;
    double reg_sum = 0.0;
    uint32_t gamma = 0xffffffff;
    for (int i = 0; i < r; ++i) {
        MurmurHash3_x86_32(&key, sizeof(key), seeds[i], &hash_value);
        int idx = hash_value % m;
        gamma = std::min(gamma, static_cast<uint32_t>(R[idx]));
        reg_sum += R[idx];
    }
    reg_sum = std::pow(2, reg_sum / r);
    filter_est = (m * r) / (m - r) * (1 / (r * phi) * reg_sum - f / m);
    uint32_t rounded = (filter_est > 0.0) ? static_cast<uint32_t>(filter_est) : -filter_est;
    uint32_t value_es = rounded;
    return value_es;
}




void LogLogFilter_Spread::SSDetection(
        const std::vector<std::pair<uint32_t, uint32_t>>& dataset,
        const std::unordered_map<uint32_t, std::unordered_set<uint32_t>>& true_cardi,
        const std::vector<uint32_t> thresholds) {
    // the process of traffic
    for (const auto& [key, element] : dataset) {
        update(key, element);
    }
    std::cout << "\nSuper Spreader Detection:\n";
    for (uint32_t threshold : thresholds) {
        std::unordered_map<uint32_t, uint32_t> ground_truth;
        for (const auto& [key, elements] : true_cardi) {
            if (elements.size() > threshold) {
                ground_truth[key] = elements.size();
            }
        }
        std::unordered_map<uint32_t, uint32_t> detected_ss;
        detected_ss = detect(threshold);
        int TP = 0;
        for (const auto& [key, estimated] : detected_ss) {
            if (ground_truth.find(key) != ground_truth.end()) {
                TP++;
            }
        }
        double precision = (detected_ss.size() > 0) ? (double)TP / detected_ss.size() : 0.0;
        double recall = (ground_truth.size() > 0) ? (double)TP / ground_truth.size() : 0.0;
        double f1_score = (precision + recall > 0) ? 2 * precision * recall / (precision + recall) : 0.0;
        std::cout << "Th: " << threshold << ", F1: " << f1_score << std::endl;
    }
}



std::unordered_map<uint32_t, uint32_t> LogLogFilter_Spread::candidates() {
    // just an empty implementation, not used for filter, only implemented in sketch for giving super spreader candidates
    std::unordered_map<uint32_t, uint32_t> result;
    return result;
}