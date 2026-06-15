
#ifndef FILTER_H
#define FILTER_H

#include "Sketch.h"
#include "BaseSketchType.h"
#include <cstdint>
#include <vector>
#include <random>
#include <algorithm>

struct BitmapEncap {
    std::vector<bool> bitmap;
    uint64_t bits = 0;
    uint32_t bitmap_size = 0;
    uint32_t ones_count = 0;

    BitmapEncap(int bitmap_size, std::uniform_real_distribution<float>& distribution, std::default_random_engine& generator) {
        this->bitmap_size = static_cast<uint32_t>(bitmap_size);
        float p = 0.5;
        bitmap.resize(bitmap_size, false);
        for (int i = 0; i < bitmap_size; ++i) {
            float r = distribution(generator);
            if ( r < p) {
                bitmap[i] = true;
            } else
                bitmap[i] = false;
        }
        ones_count = static_cast<uint32_t>(std::count(bitmap.begin(), bitmap.end(), true));
        for (uint32_t i = 0; i < this->bitmap_size && i < 64; ++i) {
            if (bitmap[i]) {
                bits |= (1ULL << i);
            }
        }
    }

    void setBit(uint32_t index, bool value) {
        if (index >= 64) {
            const bool old_value = bitmap[index];
            if (old_value == value) return;

            bitmap[index] = value;
            if (value) {
                ++ones_count;
            } else {
                --ones_count;
            }
            return;
        }

        const uint64_t mask = 1ULL << index;
        const bool old_value = (bits & mask) != 0;
        if (old_value == value) return;

        if (value) {
            bits |= mask;
            ++ones_count;
        } else {
            bits &= ~mask;
            --ones_count;
        }
    }

    uint32_t sameDirectionCount(bool op) const {
        return op ? ones_count : bitmap_size - ones_count;
    }
};




class FlipFilter : public Sketch{

private:
    int layers_;
    std::vector<int> num_bitmaps_;
    std::vector<int> bitmap_sizes_;
    std::vector<uint32_t> bitmap_index_masks_;
    std::vector<uint32_t> pass_count_thresholds_;
    std::vector<std::vector<BitmapEncap>> filter_;
    float ratio_threshold_;
    uint32_t distribute_num;
    std::default_random_engine generator_;
    std::uniform_real_distribution<float> distribution_;
    std::vector<uint32_t> hash_seeds;
    std::unordered_set<uint32_t> passed_;
    Sketch* sketch;

public:

    FlipFilter(float memory_kb, float f_ratio, int layers,
               const std::vector<int>& bitmap_sizes,
               float ratio_threshold,
               const std::vector<float>& memory_ratios,
               uint32_t distribute_num = 3,
               BaseSketchType base_sketch_type = BaseSketchType::VHLL);
    ~FlipFilter() override;


    bool getOp(uint32_t label, uint32_t bkt_index);

    void update(uint32_t label, uint32_t element);

    void prepareQuery() override;

    uint32_t query(uint32_t label);

    std::unordered_map<uint32_t, uint32_t> detect(uint32_t threshold);

    std::vector<uint32_t> generateSeeds32(size_t count);

    uint32_t que(uint32_t label);
    std::unordered_map<uint32_t, uint32_t> candidates();
};

#endif
