#ifndef _COUPONFILTER_H
#define _COUPONFILTER_H

#include <cstdint>
#include <iostream>
#include <ctime>
#include <cmath>
#include <cstring>
// #include "./MurmurHash3.h"
#include "./Xorshift.h"
#include "Sketch.h"

#define MAX_VALUE 0xFFFFFFFF
#define NUM_BITS 8 // size of each item

struct Cpf_Param {
    uint32_t m;
    uint32_t c;
    double p;
    char mea_tag;
};

// implementation of CouponFilter, derived from:  https://github.com/duyang92/coupon-filter-paper

class CouponFilter : public Sketch {
private:
    // @params: m - num of filter units
    //          c - size of each filter unit (in bits, in fact, the num of coupon should be collected)
    //          prefix_p - prefix value for the probability of drawing a coupon, p = prefix_p * 0.5 ** 32
    //          mea_tag - f - (frequency-based); c - (cardinality-based); p - (persistence-based)
    double p;
    uint32_t m;
    uint32_t c;
    char mea_tag;
    double tau;

    uint32_t seed;       // seed for murmurhash
    uint8_t *bitmap;
    Sketch *sketch;
    xorshift::xorshift32 rng;

    uint32_t get_unit_index(uint32_t flow_id);
    int get_coupon_index(uint32_t flow_id, uint32_t ele_id);

public:

    CouponFilter(uint32_t memory_kb, Sketch *skt);

    ~CouponFilter() {
        if (bitmap) {
            delete[] bitmap;
            bitmap = nullptr;
        }
    }

    void update(uint32_t flow_id, uint32_t ele_id);
    uint32_t query(uint32_t flow_id);

    void spreadEstimation(
            const std::vector<std::pair<uint32_t, uint32_t>>& dataset,
            const std::unordered_map<uint32_t, std::unordered_set<uint32_t>>& true_cardi);


    // for super spreader detection
    std::unordered_map<uint32_t, uint32_t> detect(uint32_t threshold);

    void SSDetection(
            const std::vector<std::pair<uint32_t, uint32_t>>& dataset,
            const std::unordered_map<uint32_t, std::unordered_set<uint32_t>>& true_cardi,
            const std::vector<uint32_t> thresholds);

    void throughput(const std::vector<std::pair<uint32_t, uint32_t>>& dataset,
                    const std::unordered_map<uint32_t, std::unordered_set<uint32_t>>& true_cardi);

    std::unordered_map<uint32_t, uint32_t> candidates();

};

#endif // _COUPONFILTER_H
