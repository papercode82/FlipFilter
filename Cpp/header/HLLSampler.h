
#ifndef CPP_HLLSAMPLER_H
#define CPP_HLLSAMPLER_H

#include "Sketch.h"
#include <vector>
#include <cstdint>

struct Bucket {
    std::vector<uint32_t> registers;  // HLLSampler, register array, each register is 5 bits in logic
    uint32_t id;                     // ID
    int counter;                // counter
    float prob;                     // probability for HLLSampler

    Bucket(size_t m) : registers(m, 0), id(0), counter(0), prob(1.0) {}

};


class HLLSampler: public Sketch{
private:

    size_t d;                  // d rows
    size_t w;                  // w columns
    size_t m;                  // num of registers in each register array

    std::vector<std::vector<Bucket>> B;  // d*w buckets
    std::vector<uint32_t> rand_seeds; // random seeds

public:

    HLLSampler(size_t memory_in_bytes);

    std::vector<uint32_t> generateSeeds32(size_t count);

    static double urand();   // generate a random value ∈[0,1)

    uint32_t get_rank(uint32_t hash_val);

    void update(const uint32_t key, const uint32_t element);

    void SSDetection(
            const std::vector<std::pair<uint32_t, uint32_t>>& dataset,
            const std::unordered_map<uint32_t, std::unordered_set<uint32_t>>& true_cardi,
            const std::vector<uint32_t> thresholds
    );

    std::unordered_map<uint32_t, uint32_t> detect(uint32_t threshold);

    std::unordered_map<uint32_t, uint32_t> candidates();


    uint32_t query(const uint32_t key);


};



#endif
