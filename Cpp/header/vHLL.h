#ifndef VHLL_H
#define VHLL_H

#include "Sketch.h"
#include <set>
#include <ctime>
#include <cmath>
#include <cstring>
#define MAX(a,b) ((a)<(b))?(b):(a)

class vHLL : public Sketch {
private:
    uint32_t m, v, num_leading_zeros;
    uint32_t* R, *S;
    uint32_t seed, cardi_all_flow;
    double alpha;
public:
    vHLL(uint32_t memory_kb);
    ~vHLL() {
        delete R;
        delete S;
    }
    void update(uint32_t, uint32_t);
    uint32_t query(uint32_t);
    void update_param();
    void spreadEstimation(
            const std::vector<std::pair<uint32_t, uint32_t>>& dataset,
            const std::unordered_map<uint32_t, std::unordered_set<uint32_t>>& true_cardi);

    std::unordered_map<uint32_t, uint32_t> detect(uint32_t threshold);
    std::unordered_map<uint32_t, uint32_t> candidates();

};

#endif //VHLL_H
