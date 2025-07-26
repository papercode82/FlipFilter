
#ifndef FILTER_H
#define FILTER_H

#include "Sketch.h"
#include <vector>
#include <random>
#include <algorithm>
#include <iostream>


struct BitmapEncap {
    std::vector<bool> bitmap;
    BitmapEncap(int bitmap_size, std::uniform_real_distribution<float>& distribution, std::default_random_engine& generator) {
        float p = 0.5;
        bitmap.resize(bitmap_size, false);
        for (int i = 0; i < bitmap_size; ++i) {
            float r = distribution(generator);
            if ( r < p) {
                bitmap[i] = true;
            } else
                bitmap[i] = false;
        }
    }
};



// header file for FlipFilter
class FlipFilter : public Sketch{

private:
    int layers_;
    std::vector<int> num_bitmaps_;
    std::vector<int> bitmap_sizes_;
    std::vector<std::vector<BitmapEncap>> filter_;

    float ratio_threshold_;
    uint32_t distribute_num;

    std::default_random_engine generator_;
    std::uniform_real_distribution<float> distribution_;

    std::vector<uint32_t> hash_seeds;
    Sketch* sketch;     // the spread measurement algorithm

public:


    FlipFilter(float memory_kb, Sketch* skt);

    bool getOp(uint32_t label, uint32_t bkt_index);
    void update(uint32_t label, uint32_t element);
    uint32_t query(uint32_t label);

    void spreadEstimation(
            std::vector<std::pair<uint32_t, uint32_t>>& dataset,
            const std::unordered_map<uint32_t, std::unordered_set<uint32_t>>& true_cardi);

    void throughput(const std::vector<std::pair<uint32_t, uint32_t>>& dataset,
                            const std::unordered_map<uint32_t, std::unordered_set<uint32_t>>& true_cardi);

    std::unordered_map<uint32_t, uint32_t> detect(uint32_t threshold);

    void SSDetection(
            const std::vector<std::pair<uint32_t, uint32_t>>& dataset,
            const std::unordered_map<uint32_t, std::unordered_set<uint32_t>>& true_cardi,
            const std::vector<uint32_t> thresholds);

    std::vector<uint32_t> generateSeeds32(size_t count);
    uint32_t que(uint32_t label);
    std::unordered_map<uint32_t, uint32_t> candidates();
};

#endif
