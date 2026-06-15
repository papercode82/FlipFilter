#include <fstream>
#include "header/Couper.h"
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

Couper::Couper(float memory_kb, float f_ratio, BaseSketchType base_sketch_type){

    float filter_m = memory_kb * f_ratio;
    uint32_t skt_memo = static_cast<uint32_t>(memory_kb - filter_m);

    std::cout << "\n" << "Couper + " << baseSketchLabel(base_sketch_type) << ": " << std::endl;
    sketch = createBaseSketch(skt_memo, base_sketch_type);

    // Init the filter
    b = 12;
    tau = 9;
    uint64_t fil_total_bits = static_cast<uint64_t>(filter_m * 1024 * 8);
    L1 = fil_total_bits / b;
    // false is as 0, true is as 1
    filter.resize(L1, std::vector<bool>(b, false));

}


void Couper::update(const uint32_t key, const uint32_t element) {
    uint32_t key_hash_val;
    // Hash the key with the chosen hash function
    MurmurHash3_x86_32(&key, KEY_BYTE_LEN, HASH_SEED, &key_hash_val);
    uint32_t bitmap_idx = key_hash_val % L1;

    // Count the number of 1s in the bitmap
    uint32_t cf = 0;

    for (uint32_t i = 0; i < b; i++) {
        if (filter[bitmap_idx][i]) {
            cf++;
        }
    }

    // Case 1: cf < Tau
    if (cf < tau) {
        uint32_t element_hash_val;
        MurmurHash3_x86_32(&element, KEY_BYTE_LEN, HASH_SEED, &element_hash_val);
        uint32_t bit_pos = element_hash_val % b;
        filter[bitmap_idx][bit_pos] = true;
    }
    else {
        passed_.insert(key);
        sketch->update(key, element);
    }
}


uint32_t Couper::query(const uint32_t key){

    uint32_t key_hash_val;
    // Hash the key with the chosen hash function
    MurmurHash3_x86_32(&key, KEY_BYTE_LEN, HASH_SEED, &key_hash_val);
    uint32_t bitmap_idx = key_hash_val % L1;

    // Count the number of 1s in the bitmap, i.e., check the collected coupons
    uint32_t cf = 0;
    for (uint32_t i = 0; i < b; i++) {
        if (filter[bitmap_idx][i]) {
            cf++;
        }
    }

    // Case 1: cf < Tau
    if (cf < tau) {
        // cf < tau < b
        return static_cast<uint32_t>(b * log(static_cast<double>(b) / (b - cf)));
    }
    else if (cf == tau){
        return sketch->query(key) + static_cast<uint32_t>(b * log(static_cast<double>(b) / (b - cf)));
    }
    else{
        return sketch->query(key) + static_cast<uint32_t>(b * log(static_cast<double>(tau)));
    }

}



std::unordered_map<uint32_t, uint32_t> Couper::detect(uint32_t threshold) {
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



uint32_t Couper::que(const uint32_t key){
    uint32_t key_hash_val;
    // Hash the key with the chosen hash function
    MurmurHash3_x86_32(&key, KEY_BYTE_LEN, HASH_SEED, &key_hash_val);
    uint32_t bitmap_idx = key_hash_val % L1;

    // Count the number of 1s in the bitmap, i.e., check the collected coupons
    uint32_t cf = 0;
    for (uint32_t i = 0; i < b; i++) {
        if (filter[bitmap_idx][i]) {
            cf++;
        }
    }

    if (cf < tau) {
        return static_cast<uint32_t>(b * log(static_cast<double>(b) / (b - cf)));
    }
    else if (cf == tau){
        return static_cast<uint32_t>(b * log(static_cast<double>(b) / (b - cf)));
    }
    else{
        return static_cast<uint32_t>(b * log(static_cast<double>(tau)));
    }
}



std::unordered_map<uint32_t, uint32_t> Couper::candidates() {
    std::unordered_map<uint32_t, uint32_t> result;
    return result;
}
