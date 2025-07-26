#ifndef COUPER_H
#define COUPER_H

#include "Sketch.h"
#include <iostream>
#include <vector>

// An implementation of paper Couper: Memory-Efficient Cardinality Estimation under Unbalanced Distribution
// note that, here is just the implementation about Layer 1, i.e., the filter part
// the estimator (Layer 2) is initialized externally and passed in as a reference

class Couper : public Sketch {
private:
    uint32_t L1;  // Number of bitmaps
    uint32_t b;   // Number of bits per bitmap
    uint32_t tau; // Threshold
    std::vector<std::vector<bool>> filter; // use vector to implement bitmaps with vary size
    Sketch* sketch; // Pointer to an external Sketch (e.g., HyperLogLog)

public:

    Couper(float memory_kb, Sketch* skt);

    ~Couper() {

    }

    // Placeholder for update and query functions
    void update(const uint32_t key, const uint32_t element);

    uint32_t query(const uint32_t key);  // query the spread for given key

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

    uint32_t que(const uint32_t key);
    std::unordered_map<uint32_t, uint32_t> candidates();

};
#endif //COUPER_H
