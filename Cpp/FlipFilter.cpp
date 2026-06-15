
#include <iostream>
#include <stdexcept>
#include "header/FlipFilter.h"
#include "header/SuperKjSkt.h"
#include "header/KjSkt.h"
#include "header/vHLL.h"
#include "header/rSkt.h"
#include "header/FreeRS.h"

namespace {
inline uint32_t fastMix32(uint32_t value) {
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    value ^= value >> 16;
    return value;
}
}

FlipFilter::FlipFilter(float memory_kb, float f_ratio, int layers,
                       const std::vector<int>& bitmap_sizes,
                       float ratio_threshold,
                       const std::vector<float>& memory_ratios,
                       uint32_t distribute_num,
                       BaseSketchType base_sketch_type)
        : layers_(layers), bitmap_sizes_(bitmap_sizes), ratio_threshold_(ratio_threshold),
        distribution_(0.0f, 1.0f) {

    if (layers_ <= 0) {
        throw std::invalid_argument("FlipFilter requires at least one layer.");
    }
    if (bitmap_sizes_.size() < static_cast<size_t>(layers_)) {
        throw std::invalid_argument("bitmap_sizes must contain one size per layer.");
    }
    if (memory_ratios.size() < static_cast<size_t>(layers_)) {
        throw std::invalid_argument("memory_ratios must contain one ratio per layer.");
    }
    if (f_ratio <= 0.0f || f_ratio >= 1.0f) {
        throw std::invalid_argument("Filter memory ratio must be in (0, 1).");
    }
    if (distribute_num == 0) {
        throw std::invalid_argument("distribute_num must be greater than 0.");
    }

    float filter_m = memory_kb * f_ratio;
    uint32_t skt_memo = static_cast<uint32_t>(std::round(memory_kb - filter_m));
    if (skt_memo == 0) {
        throw std::invalid_argument("Sketch memory is zero. Reduce filter memory ratio or increase total memory.");
    }

    switch (base_sketch_type) {
        case BaseSketchType::VHLL:
            std::cout << "\n" << "FlipFilter + vHLL: " << std::endl;
            sketch = new vHLL(skt_memo);
            break;
        case BaseSketchType::KjSkt:
            std::cout << "\n" << "FlipFilter + KjSkt: " << std::endl;
            sketch = new KjSkt(skt_memo);
            break;
        case BaseSketchType::SuperKjSkt:
            std::cout << "\n" << "FlipFilter + SuperKjSkt: " << std::endl;
            sketch = new SuperKjSkt(skt_memo);
            break;
        case BaseSketchType::RSkt:
            std::cout << "\n" << "FlipFilter + rSkt: " << std::endl;
            sketch = new rSkt(skt_memo);
            break;
        case BaseSketchType::FreeRS:
            std::cout << "\n" << "FlipFilter + FreeRS: " << std::endl;
            sketch = new FreeRS(skt_memo);
            break;
    }

    uint32_t filter_bits = static_cast<uint32_t>(std::round(filter_m * 1024 * 8));
    num_bitmaps_.resize(layers);
    bitmap_index_masks_.resize(layers, 0);
    pass_count_thresholds_.resize(layers);
    filter_.resize(layers);
    for (size_t i = 0; i < layers; ++i) {
        if (bitmap_sizes_[i] <= 0) {
            throw std::invalid_argument("Bitmap size must be greater than 0.");
        }
        if (memory_ratios[i] <= 0.0f) {
            throw std::invalid_argument("Layer memory ratio must be greater than 0.");
        }
        uint32_t memory_bits_layer = static_cast<uint32_t>(std::round(filter_bits * memory_ratios[i]));
        num_bitmaps_[i] = memory_bits_layer / bitmap_sizes_[i];
        if (num_bitmaps_[i] == 0) {
            throw std::runtime_error("A FlipFilter layer has zero bitmaps. Increase memory or reduce bitmap size.");
        }
        if ((bitmap_sizes_[i] & (bitmap_sizes_[i] - 1)) == 0) {
            bitmap_index_masks_[i] = static_cast<uint32_t>(bitmap_sizes_[i] - 1);
        }
        pass_count_thresholds_[i] = static_cast<uint32_t>(std::floor(ratio_threshold_ * bitmap_sizes_[i])) + 1U;
        filter_[i].resize(num_bitmaps_[i], BitmapEncap(bitmap_sizes_[i], distribution_, generator_));
    }
    this->distribute_num = distribute_num;
    hash_seeds = generateSeeds32(distribute_num);
}


FlipFilter::~FlipFilter() {
    delete sketch;
}


std::vector<uint32_t> FlipFilter::generateSeeds32(size_t count) {
    std::vector<uint32_t> seeds;
    seeds.reserve(count);
    std::mt19937 gen(1337);
    std::uniform_int_distribution<uint32_t> dist((1U << 24), std::numeric_limits<uint32_t>::max());
    for (size_t i = 0; i < count; ++i) {
        seeds.push_back(dist(gen));
    }
    return seeds;
}



bool FlipFilter::getOp(uint32_t label, uint32_t b_index) {
    uint32_t mixed = label ^ (b_index * 0x9e3779b9U) ^ HASH_SEED;
    return (fastMix32(mixed) & 1U) != 0;
}



void FlipFilter::update(uint32_t label, uint32_t element) {
    uint32_t element_hash_val;
    MurmurHash3_x86_32(&element, sizeof(element), HASH_SEED, &element_hash_val);
    uint32_t seed_idx = element_hash_val % distribute_num;

    uint32_t key_hash_val;
    MurmurHash3_x86_32(&label, sizeof(label), hash_seeds[seed_idx], &key_hash_val);

    for (int l = 0; l < layers_; ++l) {
        uint32_t b_idx = key_hash_val % num_bitmaps_[l];
        bool op_ = getOp(label, b_idx);
        uint32_t bit_idx = bitmap_index_masks_[l] != 0
                           ? (element_hash_val & bitmap_index_masks_[l])
                           : (element_hash_val % bitmap_sizes_[l]);
        auto& bucket = filter_[l][b_idx];
        bucket.setBit(bit_idx, op_);
        uint32_t same_direction_count = bucket.sameDirectionCount(op_);
        if (same_direction_count < pass_count_thresholds_[l]) {
            return;
        }
    }
    passed_.insert(label);
    sketch->update(label, element);
}


void FlipFilter::prepareQuery() {
    sketch->prepareQuery();
}



uint32_t FlipFilter::query(uint32_t label) {
    uint32_t spread_es = 0;
    for (int l = 0; l < layers_; ++l) {
        uint32_t valid_b = 0;
        for (int i = 0; i < distribute_num; ++i) {
            uint32_t key_hash_val;
            MurmurHash3_x86_32(&label, sizeof(label), hash_seeds[i], &key_hash_val);
            uint32_t b_idx = key_hash_val % num_bitmaps_[l];
            bool op_ = getOp(label, b_idx);

            uint32_t same_direction_count = filter_[l][b_idx].sameDirectionCount(op_);
            spread_es += same_direction_count;
            if (same_direction_count >= pass_count_thresholds_[l]) {
                valid_b ++;
            }
        }
        if (valid_b * 2 <= distribute_num )
            return spread_es;
    }
    spread_es += sketch->query(label);
    return spread_es;
}





std::unordered_map<uint32_t, uint32_t> FlipFilter::detect(uint32_t threshold) {
    std::unordered_map<uint32_t, uint32_t> result;
    std::unordered_map<uint32_t, uint32_t> detected_ss;
    detected_ss = sketch->candidates();
    for (const auto& [key, estimated] : detected_ss) {
        uint32_t es = estimated + que(key);
        if (es > threshold) {
            result[key] = es;
        }
    }
    return result;
}





uint32_t FlipFilter::que(uint32_t label) {
    uint32_t spread_es = 0;
    for (int l = 0; l < layers_; ++l) {
        uint32_t valid_b = 0;
        for (int i = 0; i < distribute_num; ++i) {
            uint32_t key_hash_val;
            MurmurHash3_x86_32(&label, sizeof(label), hash_seeds[i], &key_hash_val);
            uint32_t b_idx = key_hash_val % num_bitmaps_[l];
            bool op_ = getOp(label, b_idx);

            uint32_t same_direction_count = filter_[l][b_idx].sameDirectionCount(op_);
            spread_es += same_direction_count;
            if (same_direction_count >= pass_count_thresholds_[l]) {
                valid_b ++;
            }
        }

        if (valid_b * 2 <= distribute_num )
            return spread_es;
    }
    return spread_es;
}


std::unordered_map<uint32_t, uint32_t> FlipFilter::candidates( ) {
    std::unordered_map<uint32_t, uint32_t> result;
    return result;
}
