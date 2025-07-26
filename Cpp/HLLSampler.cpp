#include "header/HLLSampler.h"

#include <cmath>
#include <stdexcept>

HLLSampler::HLLSampler(size_t memory_in_kb) {
    d = 3;
    m = 4;
    // bucket size (bits) in logic
    size_t bucket_size_bits = (5 * m) + 32 + 32 + 32; // registers + id + counter + probability
    size_t total_buckets = memory_in_kb * 1024 * 8 / bucket_size_bits;
    if (total_buckets < d) {
        throw std::invalid_argument("Memory too small to initialize the structure");
    }
    w = total_buckets / d;
    B.resize(d);
    for (size_t i = 0; i < d; ++i) {
        B[i].reserve(w);
        for (size_t j = 0; j < w; ++j) {
            B[i].emplace_back(m);
        }
    }
    rand_seeds = generateSeeds32(d);
}


std::vector<uint32_t> HLLSampler::generateSeeds32(size_t count) {
    std::vector<uint32_t> seeds;
    seeds.reserve(count);
    std::random_device rd;
    std::mt19937 gen(rd() ^ static_cast<unsigned>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count()));
    std::uniform_int_distribution<uint32_t> dist((1U << 24), std::numeric_limits<uint32_t>::max());
    for (size_t i = 0; i < count; ++i) {
        seeds.push_back(dist(gen));
    }
    return seeds;
}



double HLLSampler::urand() {
    static thread_local std::mt19937 rng{std::random_device{}()};
    static thread_local std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(rng);
}


uint32_t HLLSampler::get_rank(uint32_t hash_val){
    int left_most = 0;
    while (hash_val) {
        left_most += 1;
        hash_val >>= 1;
    }
    return (32 - left_most) + 1;
}


void HLLSampler::update(const uint32_t key, const uint32_t element){
    uint32_t element_hash = 0;
    MurmurHash3_x86_32(&element, sizeof(element), HASH_SEED, &element_hash);
    uint32_t rank = get_rank(element_hash);
    for (std::size_t row = 0; row < d; ++row) {
        uint32_t key_hash_val = 0;
        MurmurHash3_x86_32(&key, sizeof(key), rand_seeds[row], &key_hash_val);
        uint32_t column = key_hash_val % w;
        Bucket& bucket = B[row][column];
        uint32_t reg_idx = element_hash % m;
        if (rank <= bucket.registers[reg_idx])
            continue;
        uint32_t reg_val_stale = bucket.registers[reg_idx];
        bucket.registers[reg_idx] = rank;
        const int inc  = static_cast<int>(std::ceil(1.0 / bucket.prob)); // ⌈1/p⌉
        const float hitProb  = (1.0 / bucket.prob) / inc;  // (1/p)/⌈1/p⌉ ≤ 1
        double r1 = urand();
        if (bucket.id == 0) {                 /* Case 1: id = null */
            bucket.id = key;
            if (r1 < hitProb) bucket.counter += inc;
        } else if (bucket.id == key) {        /* Case 2: id = key */
            if (r1 < hitProb) bucket.counter += inc;
        } else {                              /* Case 3: id != key */
            double decayProb = std::pow(1.08, -static_cast<double>(bucket.counter));
            if (urand() < decayProb) {
                if (r1 < hitProb) bucket.counter -= inc;
                if (bucket.counter <= 0) {
                    bucket.id      = key;
                    bucket.counter = -bucket.counter;
                }
            }
        }
        bucket.prob += (-((1.0 / m) * std::pow(2.0, -static_cast<double>(reg_val_stale)))
                        + ((1.0 / m) * std::pow(2.0, -static_cast<double>(rank))));
    }
}


std::unordered_map<uint32_t, uint32_t> HLLSampler::detect(uint32_t threshold){
    std::unordered_map<uint32_t, uint32_t> agg;
    agg.reserve(d * w);
    for (size_t i = 0; i < d; ++i) {
        for (const Bucket& b : B[i]) {
            if (b.id == 0) continue;
            auto [it, inserted] = agg.emplace(b.id, b.counter);
            if (!inserted)
                it->second = std::max(static_cast<uint32_t>(it->second), static_cast<uint32_t>(b.counter));
        }
    }
    std::unordered_map<uint32_t, uint32_t> res;
    res.reserve(agg.size());
    for (const auto& [k, cnt] : agg)
        if (cnt > threshold) res.emplace(k, cnt);
    return res;
}



std::unordered_map<uint32_t, uint32_t> HLLSampler::candidates(){
    std::unordered_map<uint32_t, uint32_t> agg;
    agg.reserve(d * w);
    for (size_t i = 0; i < d; ++i) {
        for (const Bucket& b : B[i]) {
            if (b.id == 0) continue;
            auto [it, inserted] = agg.emplace(b.id, b.counter);
            if (!inserted)
                it->second = std::max(static_cast<uint32_t>(it->second), static_cast<uint32_t>(b.counter));
        }
    }
    return agg;
}



void HLLSampler::SSDetection(
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



uint32_t HLLSampler::query(const uint32_t key){
    // empty implementation for virtual function in Sketch.h
    return 0;
}