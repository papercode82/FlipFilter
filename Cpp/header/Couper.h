#ifndef COUPER_H
#define COUPER_H

#include "Sketch.h"
#include "BaseSketchType.h"
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
    std::unordered_set<uint32_t> passed_;

public:

    Couper(float memory_kb, float f_ratio, BaseSketchType base_sketch_type = BaseSketchType::FreeRS);

    ~Couper() override {
        delete sketch;
    }

    void update(const uint32_t key, const uint32_t element);
    uint32_t query(const uint32_t key);
    std::unordered_map<uint32_t, uint32_t> detect(uint32_t threshold);
    uint32_t que(const uint32_t key);
    std::unordered_map<uint32_t, uint32_t> candidates();

};
#endif //COUPER_H
