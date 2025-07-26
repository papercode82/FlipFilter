
#include "header/SuperKjSkt.h"
#include <unordered_map>
#include <unordered_set>
#include <string.h>

SuperKjSkt::SuperKjSkt(uint32_t memory_kb) {
    k = 3;
    m = (memory_kb * 1024 * 8) / 3 / (5);
    l = 8;
    z = 4;
    s_l = l / z;
    s_m = m / z;
    C = new unsigned char *[k];
    keyseeds = new unsigned *[k];
    num_of_leading_bits = floor(log10(double(z)) / log10(2.0)); // used to locate
    for (int i = 0; i < k; ++i) {
        C[i] = new uint8_t[m]{0};
        keyseeds[i] = new unsigned[s_l]{0};
    }
    std::unordered_set<uint32_t> mset;
    srand(time(NULL));
    while (mset.size() != s_l * k)
        mset.insert(rand() % 10000);
    int temp_i = 0;
    for (auto iter = mset.begin(); iter != mset.end(); iter ++, temp_i ++) {
        int row = temp_i / s_l;
        int col = temp_i % s_l;
        keyseeds[row][col] = *iter;
    }
    eleseed = uint32_t(rand());
    if (l == 16) {
        alpha = 0.673;
    } else if (l == 32) {
        alpha = 0.697;
    } else if (l == 64) {
        alpha = 0.709;
    } else {
        alpha = (0.7213 / (1 + (1.079 / l)));
    }
}

void SuperKjSkt::update(uint32_t key, uint32_t ele) {
    uint32_t hashValue, keysegIndex, elehashIndex, segIndex;
    char hash_input_key[5] = {0};
    char hash_input_ele[5] = {0};
    memcpy(hash_input_ele, &ele, sizeof(uint32_t));
    memcpy(hash_input_key, &key, sizeof(uint32_t));
    MurmurHash3_x86_32(hash_input_ele, 4, eleseed, &hashValue);
    segIndex = (hashValue / z) % s_l;
    elehashIndex = hashValue >> (sizeof(uint32_t) * 8 - num_of_leading_bits); // hashed register
    uint32_t q_part = hashValue - (elehashIndex << (sizeof(uint32_t) * 8 - num_of_leading_bits));
    uint32_t left_most = 0;
    while(q_part) {
        left_most += 1;
        q_part = q_part >> 1;
    }
    left_most = sizeof(uint32_t) * 8 - num_of_leading_bits - left_most + 1;
    for (int i = 0; i < k; ++i) {
        uint32_t seed = keyseeds[i][segIndex];
        MurmurHash3_x86_32(hash_input_key, 4, seed, &hashValue);
        keysegIndex = hashValue % s_m;
        C[i][keysegIndex * z + elehashIndex] = MAX(C[i][keysegIndex * z + elehashIndex], left_most);
    }
}

uint32_t SuperKjSkt::query(uint32_t key) {
    uint8_t estMap[l];
    uint32_t hashValue, keyhashIndex;
    char hash_input_key[5] = {0};
    memcpy(hash_input_key, &key, sizeof(uint32_t));
    for (int i = 0; i < k; ++i) {
        for (int s = 0; s < s_l; ++s) {
            uint32_t tmp_seed = keyseeds[i][s];
            MurmurHash3_x86_32(hash_input_key, 4, tmp_seed, &hashValue);
            keyhashIndex = hashValue % s_m;
            for (int ii = 0; ii < z; ++ii) {
                if (i == 0) {
                    estMap[s * z + ii] = C[i][keyhashIndex * z + ii];
                } else {
                    estMap[s * z + ii] = MIN(estMap[s * z + ii], C[i][keyhashIndex * z + ii]);
                }
            }
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




void SuperKjSkt::spreadEstimation(
        const std::vector<std::pair<uint32_t, uint32_t>>& dataset,
        const std::unordered_map<uint32_t, std::unordered_set<uint32_t>>& true_cardi) {
    for (const auto& [key, element] : dataset) {
        update(key, element);
    }
    float total_are = 0.0f;
    float total_aae = 0.0f;
    uint32_t count = 0;

    auto start_query = std::chrono::high_resolution_clock::now();
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



std::unordered_map<uint32_t, uint32_t> SuperKjSkt::detect(uint32_t threshold){
    // empty implementation
    std::unordered_map<uint32_t, uint32_t> res;
    return res;
}

std::unordered_map<uint32_t, uint32_t> SuperKjSkt::candidates(){
    // empty implementation
    std::unordered_map<uint32_t, uint32_t> res;
    return res;
}