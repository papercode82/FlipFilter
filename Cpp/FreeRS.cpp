#include "header/FreeRS.h"

FreeRS::FreeRS(uint32_t memorySize) : ss(memorySize * 0.85) {
    uint32_t totalMemBits = memorySize * 1024 * 8;
    uint32_t ssMemBits = totalMemBits * 0.85;
    uint32_t regMemBits = totalMemBits - ssMemBits;
    SS_MAX_SIZE = ssMemBits / 288;
    REG_NUM = regMemBits / 5;
    regArray = new uint8_t[int(ceil(REG_NUM * 5.0 / 8.0))]();
}

FreeRS::FreeRS(const FreeRS &c) : ss(c.REG_NUM * 5 / 8) {
    uint32_t byteNum = int(ceil(REG_NUM * 5.0 / 8.0));
    regArray = new uint8_t[byteNum];
    memcpy(regArray, c.regArray, byteNum);
}


void FreeRS::insert(const pair<uint32_t, uint32_t> pkt) {
    uint32_t hashValue1 = 0;
    MurmurHash3_x86_32(&pkt, sizeof(pkt), HASH_SEED_RS, &hashValue1);

    uint32_t hashValue2 = 0;
    MurmurHash3_x86_32(&pkt, sizeof(pkt), HASH_SEED_RS + 15417891, &hashValue2);

    uint32_t regIdx = hashValue1 % REG_NUM;
    uint32_t rhoValue = min(31, __builtin_clz(hashValue2) + 1);

    uint32_t oriRhoValue = getRegValue(regIdx);

    if (rhoValue > oriRhoValue) {
        double updateValue = 1.0 / updateProb;
        if (rhoValue != 31) {
            updateProb -= (pow(2, -1.0 * oriRhoValue) - pow(2, -1.0 * rhoValue)) / REG_NUM;
        } else {//when a reg's value is 31, it cannot be updated again
            updateProb -= pow(2, -1.0 * oriRhoValue) / REG_NUM;
        }
        setRegValue(regIdx, rhoValue);

        uint32_t updateValueToInt = floor(updateValue);
        double temp = (double) rand() / RAND_MAX;
        if (temp < (updateValue - updateValueToInt)) {
            updateValueToInt += 1;
        }

        SSKeyNode *keyNode = ss.findKey(pkt.first);
        if (keyNode != nullptr) {
            uint32_t newVal = updateValueToInt + keyNode->parent->counter;
            // std::cout << "check: before SSUpdate" << std::endl;
            ss.SSUpdate(keyNode, newVal);
            // std::cout << "check: after SSUpdate" << std::endl;
        } else {
            if (ss.getSize() < SS_MAX_SIZE) {
                ss.SSPush(pkt.first, updateValueToInt, 0);
            } else {
                double temp = (double) rand() / RAND_MAX;
                uint32_t minVal = ss.getMinVal();
                uint32_t newVal = minVal + updateValueToInt;

                if (temp < (double) updateValueToInt / newVal) {
                    ss.SSPush(pkt.first, newVal, minVal);
                } else {
                    SSKeyNode *minKeyNode = ss.getMinKeyNode();
                    ss.SSUpdate(minKeyNode, newVal);
                }
            }
        }
    }
}

void FreeRS::update(const uint32_t key, const uint32_t element) {
    pair<uint32_t, uint32_t> pkt = make_pair(key, element);
    insert(pkt);
}

void FreeRS::getEstimatedFlowSpreads(unordered_map<uint32_t, uint32_t> &estimatedFlowSpreads) const {
    ss.getKeyVals(estimatedFlowSpreads);
}


std::unordered_map<uint32_t, uint32_t> FreeRS::detect(uint32_t threshold) {
    std::unordered_map<uint32_t, uint32_t> result;
    unordered_map<uint32_t, uint32_t> estFlowSpreads;
    getEstimatedFlowSpreads(estFlowSpreads);
    for (auto iter = estFlowSpreads.begin(); iter != estFlowSpreads.end(); iter++) {
        uint32_t estimate = iter->second;
        if (estimate > threshold) {
            result[iter->first] = estimate;
        }
    }
    return result;
}



std::unordered_map<uint32_t, uint32_t> FreeRS::candidates() {
    std::unordered_map<uint32_t, uint32_t> result;
    unordered_map<uint32_t, uint32_t> estFlowSpreads;
    getEstimatedFlowSpreads(estFlowSpreads);
    for (auto iter = estFlowSpreads.begin(); iter != estFlowSpreads.end(); iter++) {
        result[iter->first] = iter->second;
    }
    return result;
}



void FreeRS::SSDetection(
        const std::vector<std::pair<uint32_t, uint32_t>> &dataset,
        const std::unordered_map<uint32_t, std::unordered_set<uint32_t>> &true_cardi,
        const std::vector<uint32_t> thresholds) {
    // the process of traffic
    for (const auto &[key, element]: dataset) {
        update(key, element);
    }
    std::cout << "\nSuper Spreader Detection:\n";
    for (uint32_t threshold: thresholds) {
        std::unordered_map<uint32_t, uint32_t> ground_truth;
        for (const auto &[key, elements]: true_cardi) {
            if (elements.size() > threshold) {
                ground_truth[key] = elements.size();
            }
        }
        std::unordered_map<uint32_t, uint32_t> detected_ss;
        detected_ss = detect(threshold);
        int TP = 0;
        for (const auto &[key, estimated]: detected_ss) {
            if (ground_truth.find(key) != ground_truth.end()) {
                TP++;
            }
        }
        double precision = (detected_ss.size() > 0) ? (double) TP / detected_ss.size() : 0.0;
        double recall = (ground_truth.size() > 0) ? (double) TP / ground_truth.size() : 0.0;
        double f1_score = (precision + recall > 0) ? 2 * precision * recall / (precision + recall) : 0.0;
        std::cout << "Th: " << threshold << ", F1: " << f1_score << std::endl;
    }
}

FreeRS::~FreeRS() {
    delete[]regArray;
}


uint32_t FreeRS::query(const uint32_t key){
    unordered_map<uint32_t, uint32_t> estFlowSpreads;
    getEstimatedFlowSpreads(estFlowSpreads);
    auto it = estFlowSpreads.find(key);
    if (it != estFlowSpreads.end()) {
        return it->second;
    } else {
        return 1;
    }
}
