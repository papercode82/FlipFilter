#ifndef KJSKT_H
#define KJSKT_H

#include "Sketch.h"

#define MAX(a,b) ((a)<(b))?(b):(a)
#define MIN(a,b) ((a)<(b))?(a):(b)

//
// An implementation for paper From CountMin to Super kJoin Sketches for Flow Spread Estimation
// available at https://doi.org/10.1109/TNSE.2023.3279665
// kJoin version
//

class KjSkt : public Sketch{
private:
    uint32_t d, w, l;
    uint8_t ** C;
    uint32_t eleseed;
    uint32_t *keyseeds;
    uint32_t num_of_leading_bits;
    double alpha;
public:
    KjSkt(uint32_t);

    void update(uint32_t, uint32_t);
    uint32_t query(uint32_t);
    void spreadEstimation(
            const std::vector<std::pair<uint32_t, uint32_t>>& dataset,
            const std::unordered_map<uint32_t, std::unordered_set<uint32_t>>& true_cardi);

    ~KjSkt() {
        delete[] keyseeds;
        for (int i = 0; i < d; ++i) {
            delete[] C[i];
        }
    }

    std::unordered_map<uint32_t, uint32_t> detect(uint32_t threshold);
    std::unordered_map<uint32_t, uint32_t> candidates();

};
#endif
