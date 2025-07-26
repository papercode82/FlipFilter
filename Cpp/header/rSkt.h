#ifndef RSKT_H
#define RSKT_H
#include "Sketch.h"
// #include "MurmurHash3.h"
#include <cstring>
#include <ctime>
#include <cmath>
#define MAX(a,b) ((a)<(b))?(b):(a)


// An implementation for Paper: "Randomized Error Removal for Online Spread Estimation in High-Speed Networks"
// available at https://doi.org/10.1109/TNET.2022.3197968
// The version implemented is the more accurate rSkt2 version

class rSkt : public Sketch{
private:
    uint32_t w, m, num_leading_zeros;   // w is the length of C and C', m is the number of registers in C[i]
    uint32_t ** C, ** C1;
    uint32_t keyseed, eleseed;
    double alpha;
    uint32_t total_query_count;
    uint32_t query_none_0_count;

public:
    rSkt(uint32_t);
    ~rSkt() {
        for (int i = 0; i < w; ++i) {
            delete[] C[i];
            delete[] C1[i];
        }
    }

    void update(uint32_t, uint32_t);
    int queryReg(uint32_t*, uint32_t*);
    uint32_t query(uint32_t);

    void spreadEstimation(
            const std::vector<std::pair<uint32_t, uint32_t>>& dataset,
            const std::unordered_map<uint32_t, std::unordered_set<uint32_t>>& true_cardi);


    std::unordered_map<uint32_t, uint32_t> detect(uint32_t threshold);
    std::unordered_map<uint32_t, uint32_t> candidates();
};
#endif //RSKT_H
