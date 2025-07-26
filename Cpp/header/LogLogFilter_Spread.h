

#ifndef LOGLOGFILTER_SPREAD_H
#define LOGLOGFILTER_SPREAD_H


#include "MurmurHash3.h"
#include "Sketch.h"

class LogLogFilter_Spread : public Sketch{
private:
    int m;
    int r;
    uint32_t f;
    double phi;
    Sketch* sketch;
    int type;
    std::vector<int8_t> R;
    std::vector<int> seeds;
    std::unordered_set<uint64_t> seen_pairs;


public:
    LogLogFilter_Spread(float memory_kb, Sketch* skt);

    int get_leftmost(uint32_t random_val);

    void update(const uint32_t key, const uint32_t element);

    uint32_t query(const uint32_t key);

    void spreadEstimation(
            const std::vector<std::pair<uint32_t, uint32_t>>& dataset,
            const std::unordered_map<uint32_t, std::unordered_set<uint32_t>>& true_cardi);


    void SSDetection(
            const std::vector<std::pair<uint32_t, uint32_t>>& dataset,
            const std::unordered_map<uint32_t, std::unordered_set<uint32_t>>& true_cardi,
            const std::vector<uint32_t> thresholds);

    std::unordered_map<uint32_t, uint32_t> detect(uint32_t threshold);

    void throughput(const std::vector<std::pair<uint32_t, uint32_t>>& dataset,
                    const std::unordered_map<uint32_t, std::unordered_set<uint32_t>>& true_cardi);

    uint32_t que(const uint32_t key);
    std::unordered_map<uint32_t, uint32_t> candidates();

};



#endif
