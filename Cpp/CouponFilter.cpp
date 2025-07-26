#include <algorithm>
#include <fstream>
#include "header/CouponFilter.h"


CouponFilter::CouponFilter(uint32_t memory_kb, Sketch* skt): sketch(skt){
    this->c = 4;
    this->tau = 8;
    this->mea_tag = 'c';
    this->p = 0.1;
    uint32_t memory_bits = memory_kb * 1024 * 8;
    this->m = memory_bits / this->c;
    srand(time(NULL));
    seed = uint32_t(rand());
    rng.seed(uint32_t(rand()));
    bitmap = new uint8_t[m];
    memset(bitmap, 0, sizeof(uint8_t) * m);
}


uint32_t CouponFilter::get_unit_index(uint32_t flow_id) {
    uint64_t hash_val[2];
    char hash_input_str[9] = {0};
    memcpy(hash_input_str, &flow_id, sizeof(uint32_t));
    MurmurHash3_x86_128(hash_input_str, sizeof(hash_input_str), seed, hash_val);
    return hash_val[0] % m;
}

int CouponFilter::get_coupon_index(uint32_t flow_id, uint32_t ele_id) {
    uint32_t hash_val = 0;
    char hash_input_str[13] = {0};

    if (mea_tag == 'f') {
        uint32_t rand_val = rng();
        memcpy(hash_input_str, &flow_id, sizeof(uint32_t));
        memcpy(hash_input_str + 4, &rand_val, sizeof(uint32_t));
        MurmurHash3_x86_32(hash_input_str, 8, seed, &hash_val);
    }
    else if (mea_tag == 'c') {
        memcpy(hash_input_str, &flow_id, sizeof(uint32_t));
        memcpy(hash_input_str + 4, &ele_id, sizeof(uint32_t));
        MurmurHash3_x86_32(hash_input_str, 8, seed, &hash_val);
    }
    else if (mea_tag == 'p') {
        memcpy(hash_input_str, &flow_id, sizeof(uint32_t));
        memcpy(hash_input_str + 8, &ele_id, sizeof(uint32_t));
        MurmurHash3_x86_32(hash_input_str, 12, seed, &hash_val);
    }

    for (int i = 0; i < c; i++) {
        if (double(hash_val) < p * (i + 1) * uint32_t(MAX_VALUE)) {
            return i;
        }
    }
    return -1;
}

void CouponFilter::update(uint32_t flow_id, uint32_t ele_id) {
    int coupon_index = get_coupon_index(flow_id, ele_id);
    uint32_t unit_index = get_unit_index(flow_id);

    if (coupon_index >= 0) {
        bitmap[unit_index] |= (1 << coupon_index);
    }

    if (bitmap[unit_index] == (uint8_t(1 << c) - 1)) {
        sketch->update(flow_id, ele_id);
    }
}

uint32_t CouponFilter::query(uint32_t flow_id) {

    uint32_t ans_t = sketch->query(flow_id);
    if (ans_t > 0)
        ans_t += tau;
    else
        ans_t = 1;
    return ans_t;
}



void CouponFilter::spreadEstimation(
        const std::vector<std::pair<uint32_t, uint32_t>>& dataset,
        const std::unordered_map<uint32_t, std::unordered_set<uint32_t>>& true_cardi) {
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



void CouponFilter::throughput(const std::vector<std::pair<uint32_t, uint32_t>>& dataset,
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



std::unordered_map<uint32_t, uint32_t> CouponFilter::detect(uint32_t threshold) {
    std::unordered_map<uint32_t, uint32_t> result;
    std::unordered_map<uint32_t, uint32_t> detected_ss;
    detected_ss = sketch->candidates();
    for (const auto& [key, estimated] : detected_ss) {
        if (estimated + tau > threshold) {
            result[key] = estimated + tau;
        }
    }
    return result;
}



// Super Spreader Detection
void CouponFilter::SSDetection(
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


std::unordered_map<uint32_t, uint32_t> CouponFilter::candidates( ) {
    // just an empty implementation, not used for filter, only implemented in sketch for giving super spreader candidates
    std::unordered_map<uint32_t, uint32_t> result;
    return result;
}