

#ifndef LOGLOGFILTER_SPREAD_H
#define LOGLOGFILTER_SPREAD_H


#include "BaseSketchType.h"
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
    std::unordered_set<uint32_t> passed_;

public:
    LogLogFilter_Spread(float memory_kb, float f_ratio, BaseSketchType base_sketch_type = BaseSketchType::FreeRS);

    ~LogLogFilter_Spread() override {
        delete sketch;
    }

    void setPerFlowMode(bool enabled);

    int get_leftmost(uint32_t random_val);

    void update(const uint32_t key, const uint32_t element);

    uint32_t query(const uint32_t key);

    std::unordered_map<uint32_t, uint32_t> detect(uint32_t threshold);

    uint32_t que(const uint32_t key);
    std::unordered_map<uint32_t, uint32_t> candidates();

};



#endif
