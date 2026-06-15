
#include <fstream>
#include "header/LogLogFilter_Spread.h"
#include "header/SuperKjSkt.h"
#include "header/KjSkt.h"
#include "header/vHLL.h"
#include "header/rSkt.h"
#include "header/FreeRS.h"

namespace {
const char* baseSketchLabel(BaseSketchType base_sketch_type) {
    switch (base_sketch_type) {
        case BaseSketchType::VHLL:
            return "vHLL";
        case BaseSketchType::KjSkt:
            return "KjSkt";
        case BaseSketchType::SuperKjSkt:
            return "SuperKjSkt";
        case BaseSketchType::RSkt:
            return "rSkt";
        case BaseSketchType::FreeRS:
            return "FreeRS";
    }
    return "Unknown";
}

Sketch* createBaseSketch(uint32_t memory_kb, BaseSketchType base_sketch_type) {
    switch (base_sketch_type) {
        case BaseSketchType::VHLL:
            return new vHLL(memory_kb);
        case BaseSketchType::KjSkt:
            return new KjSkt(memory_kb);
        case BaseSketchType::SuperKjSkt:
            return new SuperKjSkt(memory_kb);
        case BaseSketchType::RSkt:
            return new rSkt(memory_kb);
        case BaseSketchType::FreeRS:
            return new FreeRS(memory_kb);
    }
    return nullptr;
}
}

LogLogFilter_Spread::LogLogFilter_Spread(float memory_kb, float f_ratio, BaseSketchType base_sketch_type){

    float filter_m = memory_kb * f_ratio;
    uint32_t skt_memo = static_cast<uint32_t>(memory_kb - filter_m);

    std::cout << "\n" << "LogLog + " << baseSketchLabel(base_sketch_type) << ": " << std::endl;
    sketch = createBaseSketch(skt_memo, base_sketch_type);

    // Init the filter
    m = int(filter_m * 1024 * 8 / 5);
    r = 3;
    f = 0;
    phi = 0.77351;
    R.resize(m, 0);
    std::mt19937 gen(1337);
    type = 0;
    std::uniform_int_distribution<> dis(0, 1127483647);
    for (int i = 0; i < r; ++i) {
        seeds.push_back(dis(gen));
    }
}

void LogLogFilter_Spread::setPerFlowMode(bool enabled) {
    type = enabled ? 1 : 0;
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
        // (key, element) pair
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
        passed_.insert(key);
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




std::unordered_map<uint32_t, uint32_t> LogLogFilter_Spread::detect(uint32_t threshold) {
    std::unordered_map<uint32_t, uint32_t> result;
    std::unordered_map<uint32_t, uint32_t> detected_ss;
    detected_ss = sketch->candidates();
    for (const auto& [key, estimated] : detected_ss) {

//        uint32_t estimate = query(key);
//        if (estimate > threshold) {
//            result[key] = estimate;
//        }
//        if (estimated > threshold) {
//            result[key] = estimated;
//        }
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




std::unordered_map<uint32_t, uint32_t> LogLogFilter_Spread::candidates() {
    std::unordered_map<uint32_t, uint32_t> result;
    return result;
}
