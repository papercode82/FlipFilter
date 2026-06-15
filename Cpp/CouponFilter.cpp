#include <algorithm>
#include <fstream>
#include "header/CouponFilter.h"
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

CouponFilter::CouponFilter(uint32_t memory_kb, float f_ratio, BaseSketchType base_sketch_type){

    float filter_m = memory_kb * f_ratio;
    uint32_t skt_memo = static_cast<uint32_t>(memory_kb - filter_m);

    std::cout << "\n" << "Coupon + " << baseSketchLabel(base_sketch_type) << ": " << std::endl;
    sketch = createBaseSketch(skt_memo, base_sketch_type);


    this->c = 4;
    this->tau = 8;
    this->mea_tag = 'c'; // cardinality estimation

    this->p = 0.1;
    uint32_t memory_bits = filter_m * 1024 * 8;  // KB -> bits
    this->m = memory_bits / this->c;

    seed = 1337;
    rng.seed(2024);

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
        passed_.insert(flow_id);
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



std::unordered_map<uint32_t, uint32_t> CouponFilter::detect(uint32_t threshold) {
    std::unordered_map<uint32_t, uint32_t> result;
    std::unordered_map<uint32_t, uint32_t> detected_ss;
    detected_ss = sketch->candidates();
    for (const auto& [key, estimated] : detected_ss) {

//        uint32_t estimate = query(key);
//        if (estimate > threshold) {
//            result[key] = estimate;
//        }
        if (estimated + tau > threshold) {
            result[key] = estimated + tau;
        }
    }

    return result;
}





std::unordered_map<uint32_t, uint32_t> CouponFilter::candidates( ) {
    std::unordered_map<uint32_t, uint32_t> result;
    return result;
}
