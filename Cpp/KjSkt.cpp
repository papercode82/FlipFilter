//
// An implementation for paper From CountMin to Super kJoin Sketches for Flow Spread Estimation
// available at https://doi.org/10.1109/TNSE.2023.3279665
//
#include "header/KjSkt.h"
#include <set>
#include <fstream>
#include <string.h>

KjSkt::KjSkt(uint32_t memory_kb) {

    d = 3;
    l = 16;
    w = (memory_kb * 1024 * 8) / 3 / (5 * l);
    C = new uint8_t *[d];
    keyseeds = new unsigned[d];
    num_of_leading_bits = floor(log10(double(l)) / log10(2.0)); // used to locate
    for (int i = 0; i < d; ++i) {
        C[i] = new uint8_t[w * l]{0};
    }
    if (l == 16) {
        alpha = 0.673;
    } else if (l == 32) {
        alpha = 0.697;
    } else if (l == 64) {
        alpha = 0.709;
    } else {
        alpha = (0.7213 / (1 + (1.079 / l)));
    }
    srand(NULL);
    eleseed = uint32_t(rand());
    std::set<uint32_t> mset;
    while (mset.size() != this->d)
        mset.insert(uint32_t(rand()));
    int temp_i = 0;
    for (auto iter = mset.begin(); iter != mset.end(); iter++) {
        keyseeds[temp_i++] = *iter;
    }
}

void KjSkt::update(uint32_t key, uint32_t ele) {
    uint32_t hashValue, keyhashIndex, elehashIndex;
    char hash_input_key[5] = {0};
    char hash_input_ele[5] = {0};
    memcpy(hash_input_ele, &ele, sizeof(uint32_t));
    MurmurHash3_x86_32(hash_input_ele, 4, eleseed, &hashValue);
    elehashIndex = hashValue >> (sizeof(uint32_t) * 8 - num_of_leading_bits); // hashed register
    uint32_t q_part = hashValue - (elehashIndex << (sizeof(uint32_t) * 8 - num_of_leading_bits));
    uint32_t left_most = 0;
    while(q_part) {
        left_most += 1;
        q_part = q_part >> 1;
    }
    left_most = sizeof(uint32_t) * 8 - num_of_leading_bits - left_most + 1;
    for (int i = 0; i < d; ++i) {
        memcpy(hash_input_key, &key, sizeof(uint32_t));
        MurmurHash3_x86_32(hash_input_key, 4, keyseeds[i], &hashValue);
        keyhashIndex = hashValue % this->w;
        C[i][keyhashIndex * l + elehashIndex] = MAX(C[i][keyhashIndex * l + elehashIndex], left_most);
    }
}

uint32_t KjSkt::query(uint32_t key) {
    uint8_t estMap[l];
    uint32_t hashValue, keyhashIndex;
    char hash_input_key[5] = {0};
    for (int i = 0; i < d; ++i) {
        memcpy(hash_input_key, &key, sizeof(uint32_t));
        MurmurHash3_x86_32(hash_input_key, 4, keyseeds[i], &hashValue);
        keyhashIndex = hashValue % this->w;
        if (i == 0) {
            for (int j = 0; j < l; ++j)
                estMap[j] = C[i][keyhashIndex * l + j];
        } else {
            for (int j = 0; j < l; ++j)
                estMap[j] = MIN(estMap[j], C[i][keyhashIndex * l + j]);
        }
    }
    double zero_ratio_reg = 0;
    double sum_reg = 0;
    for (int i = 0; i < l; ++i) {
        sum_reg += pow(2.0, -double(estMap[i]));
        if(estMap[i] == 0)
            zero_ratio_reg += 1;
    }
    zero_ratio_reg = zero_ratio_reg / l;
    double flow_cardi  = alpha * pow(l, 2) / sum_reg;
    if(flow_cardi <= 2.5 * l) {
        if(zero_ratio_reg != 0)
            flow_cardi = - log(zero_ratio_reg) * l;
        else
            flow_cardi = l * log(1.0 * l);
    }
    if (flow_cardi < 0) flow_cardi = 0;
    return flow_cardi;
}




void KjSkt::spreadEstimation(
        const std::vector<std::pair<uint32_t, uint32_t>>& dataset,
        const std::unordered_map<uint32_t, std::unordered_set<uint32_t>>& true_cardi) {
    for (const auto& [key, element] : dataset) {
        update(key, element);
    }
    float total_are = 0.0f;
    uint32_t count = 0;
    for (const auto& entry : true_cardi) {
        uint32_t flow_label = entry.first;
        uint32_t true_value = entry.second.size();
        uint32_t estimated_value = query(flow_label);
        if (true_value > 0) {
            float are = std::abs(static_cast<float>(estimated_value) - static_cast<float>(true_value)) / static_cast<float>(true_value);
            total_are += are;
            ++count;
        }
    }

    if (count > 0) {
        float avg_are = total_are / count;
        std::cout << "ARE: " << avg_are << std::endl;
    } else {
        std::cout << "No data to calculate ARE." << std::endl;
    }
}



std::unordered_map<uint32_t, uint32_t> KjSkt::detect(uint32_t threshold){
    // empty implementation
    std::unordered_map<uint32_t, uint32_t> res;
    return res;
}

std::unordered_map<uint32_t, uint32_t> KjSkt::candidates(){
    // empty implementation
    std::unordered_map<uint32_t, uint32_t> res;
    return res;
}