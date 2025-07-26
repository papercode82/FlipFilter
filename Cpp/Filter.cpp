#include "header/Filter.h"
#include <iostream>
#include <fstream>
#include <chrono>



Filter::Filter(float memory_kb, Sketch* skt)
        :sketch(skt), distribution_(0.0f, 1.0f) {
    layers_ = 2;
    bitmap_sizes_ = {4, 8};
    ratio_threshold_ = 0.6;
    std::vector<float> memory_ratios = {0.5, 0.5};
    distribute_num = 3;
    // bytes to bits
    uint32_t memory_bits = static_cast<uint32_t>(std::round(memory_kb * 1024 * 8));
    num_bitmaps_.resize(layers_);
    filter_.resize(layers_);
    for (size_t i = 0; i < layers_; ++i) {
        uint32_t memory_bits_layer = static_cast<uint32_t>(std::round(memory_bits * memory_ratios[i]));
        num_bitmaps_[i] = memory_bits_layer / bitmap_sizes_[i];
        filter_[i].resize(num_bitmaps_[i], BitmapEncap(bitmap_sizes_[i], distribution_, generator_));
    }
    hash_seeds = generateSeeds32(distribute_num);
}




std::vector<uint32_t> Filter::generateSeeds32(size_t count) {
    std::vector<uint32_t> seeds;
    seeds.reserve(count);
    std::random_device rd;
    std::mt19937 gen(rd() ^ static_cast<unsigned>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count()));
    std::uniform_int_distribution<uint32_t> dist((1U << 24), std::numeric_limits<uint32_t>::max());
    for (size_t i = 0; i < count; ++i) {
        seeds.push_back(dist(gen));
    }
    return seeds;
}



bool Filter::getOp(uint32_t label, uint32_t b_index) {
    uint32_t hash_value = 0;
    MurmurHash3_x86_32(&label, sizeof(label), HASH_SEED + b_index, &hash_value);
    bool op_ = (hash_value & 1) != 0;
    return op_;
}



void Filter::update(uint32_t label, uint32_t element) {
    uint32_t element_hash_val;
    MurmurHash3_x86_32(&element, sizeof(element), HASH_SEED, &element_hash_val);
    // chose a hash function by element, the same element (for the same flow) will enter the same bucket
    uint32_t seed_idx = element_hash_val % distribute_num;
    uint32_t key_hash_val;
    MurmurHash3_x86_32(&label, sizeof(label), hash_seeds[seed_idx], &key_hash_val);
    for (int l = 0; l < layers_; ++l) {
        uint32_t b_idx = key_hash_val % num_bitmaps_[l];
        bool op_ = getOp(label, b_idx); // true : 1
        uint32_t bit_idx = element_hash_val % bitmap_sizes_[l];
        filter_[l][b_idx].bitmap[bit_idx] = op_;  // Set this bit
        int same_direction_count = 0;
        for (bool bit: filter_[l][b_idx].bitmap) {
            if (bit == op_) {
                same_direction_count++;
            }
        }
        float ratio = static_cast<float>(same_direction_count) / bitmap_sizes_[l];
        if (ratio <= ratio_threshold_) {
            return; // End the insertion process
        }
    }
    sketch ->update(label, element);
}



uint32_t Filter::query(uint32_t label) {
    uint32_t spread_es = 0;
    for (int l = 0; l < layers_; ++l) {
        uint32_t valid_b = 0;
        for (int i = 0; i < distribute_num; ++i) {
            uint32_t key_hash_val;
            MurmurHash3_x86_32(&label, sizeof(label), hash_seeds[i], &key_hash_val);
            uint32_t b_idx = key_hash_val % num_bitmaps_[l];
            bool op_ = getOp(label, b_idx); // true : 1
            uint32_t same_direction_count = 0;
            for (bool bit: filter_[l][b_idx].bitmap) {
                if (bit == op_) {
                    same_direction_count++;
                }
            }
            spread_es += same_direction_count;
            if (same_direction_count > ratio_threshold_ * bitmap_sizes_[l]) {
                valid_b ++;
            }
        }
        if (valid_b * 2 <= distribute_num )
            return spread_es;
    }
    spread_es += sketch->query(label);
    return spread_es;
}



void Filter::spreadEstimation(
        std::vector<std::pair<uint32_t, uint32_t>>& dataset,
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
    }else {
        std::cout << "No data to calculate ARE." << std::endl;
    }
}




std::unordered_map<uint32_t, uint32_t> Filter::detect(uint32_t threshold) {
    std::unordered_map<uint32_t, uint32_t> result;
    std::unordered_map<uint32_t, uint32_t> detected_ss;
    // get super spreader candidates
    detected_ss = sketch->candidates();
    for (const auto& [key, estimated] : detected_ss) {
        uint32_t es = estimated + que(key);
        if ( es > threshold) {
            result[key] = es;
        }
    }
    return result;
}



void Filter::SSDetection(
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






void Filter::throughput(const std::vector<std::pair<uint32_t, uint32_t>>& dataset,
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





uint32_t Filter::que(uint32_t label) {
    uint32_t spread_es = 0;
    for (int l = 0; l < layers_; ++l) {
        uint32_t valid_b = 0;
        for (int i = 0; i < distribute_num; ++i) {
            uint32_t key_hash_val;
            MurmurHash3_x86_32(&label, sizeof(label), hash_seeds[i], &key_hash_val);
            uint32_t b_idx = key_hash_val % num_bitmaps_[l];
            bool op_ = getOp(label, b_idx); // true : 1
            uint32_t same_direction_count = 0;
            for (bool bit: filter_[l][b_idx].bitmap) {
                if (bit == op_) {
                    same_direction_count++;
                }
            }
            spread_es += same_direction_count;
            if (same_direction_count > ratio_threshold_ * bitmap_sizes_[l]) {
                valid_b ++;
            }
        }
        if (valid_b * 2 <= distribute_num )
            return spread_es;
    }
    return spread_es;
}



std::unordered_map<uint32_t, uint32_t> Filter::candidates( ) {
    // just an empty implementation, not used for filter, only implemented in sketch for giving super spreader candidates
    std::unordered_map<uint32_t, uint32_t> result;
    return result;
}