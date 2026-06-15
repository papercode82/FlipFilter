#include "header/FlipFilter.h"
#include "header/Couper.h"
#include "header/CouponFilter.h"
#include "header/LogLogFilter_Spread.h"
#include "header/vHLL.h"
#include "header/KjSkt.h"
#include "header/SuperKjSkt.h"
#include "header/rSkt.h"
#include "header/FreeRS.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using Dataset = std::vector<std::pair<uint32_t, uint32_t>>;
using TrueCardinality = std::unordered_map<uint32_t, std::unordered_set<uint32_t>>;

enum class DatasetChoice {
    CAIDA,
    StackOverflow
};

enum class Method {
    FlipFilter,
    Couper,
    CouponFilter,
    LogLogFilterSpread,
    SketchOnly
};

struct PerFlowResult {
    Method method;
    BaseSketchType base_sketch;
    uint32_t memory_kb;
    float filter_ratio;
    double are = 0.0;
    double update_mpps = 0.0;
    double query_mfps = 0.0;
};

struct SSResult {
    Method method;
    BaseSketchType base_sketch;
    uint32_t memory_kb;
    float filter_ratio;
    uint32_t threshold;
    double f1 = 0.0;
    double are = 0.0;
};

const DatasetChoice DATASET_CHOICE = DatasetChoice::CAIDA;
const std::vector<uint32_t> MEMORY_VALUES = {100, 200, 300, 400};
const std::vector<float> FILTER_RATIOS = {0.20f};
const bool RUN_PERFLOW_EXPERIMENT = true;
const bool RUN_SS_EXPERIMENT = true;
const bool RUN_SKETCH_ONLY_BASELINE = true;

const std::vector<BaseSketchType> PERFLOW_BASE_SKETCHES = {
        BaseSketchType::VHLL,
        BaseSketchType::KjSkt,
        BaseSketchType::SuperKjSkt,
        BaseSketchType::RSkt
};

const std::vector<BaseSketchType> SS_BASE_SKETCHES = {
        BaseSketchType::FreeRS
};

const std::vector<Method> METHODS = {
        Method::FlipFilter,
        Method::Couper,
        Method::CouponFilter,
        Method::LogLogFilterSpread
};

const int FLIP_LAYERS = 2;
const std::vector<int> FLIP_BITMAP_SIZES = {4, 8};
const float FLIP_RATIO_THRESHOLD = 0.70f;
const std::vector<float> FLIP_LAYER_MEMORY_RATIOS = {0.50f, 0.50f};
const uint32_t FLIP_DISTRIBUTE_NUM = 3;
const std::vector<uint32_t> SS_THRESHOLDS = {1000};

std::string datasetName(DatasetChoice choice) {
    switch (choice) {
        case DatasetChoice::CAIDA:
            return "CAIDA";
        case DatasetChoice::StackOverflow:
            return "StackOverflow";
    }
    return "Unknown";
}

std::string methodName(Method method) {
    switch (method) {
        case Method::FlipFilter:
            return "FlipFilter";
        case Method::Couper:
            return "Couper";
        case Method::CouponFilter:
            return "CouponFilter";
        case Method::LogLogFilterSpread:
            return "LogLogFilter_Spread";
        case Method::SketchOnly:
            return "SketchOnly";
    }
    return "Unknown";
}

std::string sketchName(BaseSketchType sketch) {
    switch (sketch) {
        case BaseSketchType::VHLL:
            return "vHLL";
        case BaseSketchType::KjSkt:
            return "KjSkt";
        case BaseSketchType::SuperKjSkt:
            return "SuperKjSkt";
        case BaseSketchType::RSkt:
            return "rSkt";
        case BaseSketchType::FreeRS:
            return "FreeRS";
    }
    return "Unknown";
}

std::string formatFloat(float value, int precision = 2) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << value;
    return oss.str();
}

std::string formatIntVector(const std::vector<int>& values) {
    std::ostringstream oss;
    oss << "{";
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) oss << ":";
        oss << values[i];
    }
    oss << "}";
    return oss.str();
}

std::string formatFloatVector(const std::vector<float>& values) {
    std::ostringstream oss;
    oss << "{";
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) oss << ":";
        oss << formatFloat(values[i]);
    }
    oss << "}";
    return oss.str();
}

std::filesystem::path resolveResultPath(const std::string& file_name) {
    std::filesystem::path result_dir = "results";
    std::filesystem::create_directories(result_dir);
    return result_dir / file_name;
}

std::string resolveDatasetPath(const std::string& file_name) {
    const std::vector<std::filesystem::path> candidates = {
            std::filesystem::path("../dataset") / file_name,
            std::filesystem::path("../../dataset") / file_name,
            std::filesystem::path("dataset") / file_name
    };

    for (const auto& path: candidates) {
        if (std::filesystem::exists(path)) {
            return path.string();
        }
    }

    return candidates.front().string();
}

std::optional<uint32_t> parseIPv4(const std::string& ip_str) {
    std::stringstream ss(ip_str);
    uint32_t result = 0;
    int part = 0;

    for (int i = 0; i < 4; ++i) {
        if (!(ss >> part)) return std::nullopt;
        if (part < 0 || part > 255) return std::nullopt;
        result = (result << 8) | static_cast<uint32_t>(part);
        if (i < 3) {
            if (ss.peek() != '.') return std::nullopt;
            ss.ignore();
        }
    }

    if (ss.rdbuf()->in_avail() != 0) return std::nullopt;
    return result;
}

std::pair<Dataset, TrueCardinality> loadDataSetCAIDA() {
    std::vector<std::string> file_paths = {
            resolveDatasetPath("CAIDA_demo.txt")
    };

    Dataset data;
    TrueCardinality true_cardinality;

    for (const auto& file_path: file_paths) {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: " + file_path);
        }

        std::string line;
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            std::string source_ip;
            std::string dest_ip;
            iss >> source_ip >> dest_ip;
            if (source_ip.empty() || dest_ip.empty()) continue;

            auto src = parseIPv4(source_ip);
            auto dst = parseIPv4(dest_ip);
            if (!src || !dst) continue;

            data.emplace_back(*src, *dst);
            true_cardinality[*src].insert(*dst);
        }
    }

    return {data, true_cardinality};
}

std::pair<Dataset, TrueCardinality> loadDataSetStackOverflow() {
    std::vector<std::string> file_paths = {
            resolveDatasetPath("StackOverflow_demo.txt")
    };

    Dataset data;
    TrueCardinality true_cardinality;

    for (const auto& file_path: file_paths) {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: " + file_path);
        }

        std::string line;
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            std::string key_str;
            std::string ele_str;
            if (!(iss >> key_str >> ele_str)) continue;

            uint32_t key = static_cast<uint32_t>(std::stoul(key_str));
            uint32_t ele = static_cast<uint32_t>(std::stoul(ele_str));
            data.emplace_back(key, ele);
            true_cardinality[key].insert(ele);
        }
    }

    return {data, true_cardinality};
}

std::pair<Dataset, TrueCardinality> loadDataset(DatasetChoice choice) {
    switch (choice) {
        case DatasetChoice::CAIDA:
            return loadDataSetCAIDA();
        case DatasetChoice::StackOverflow:
            return loadDataSetStackOverflow();
    }
    throw std::runtime_error("Unknown dataset choice.");
}

std::unique_ptr<Sketch> createSketchOnlyBaseSketch(uint32_t memory_kb, BaseSketchType base_sketch) {
    switch (base_sketch) {
        case BaseSketchType::VHLL:
            return std::make_unique<vHLL>(memory_kb);
        case BaseSketchType::KjSkt:
            return std::make_unique<KjSkt>(memory_kb);
        case BaseSketchType::SuperKjSkt:
            return std::make_unique<SuperKjSkt>(memory_kb);
        case BaseSketchType::RSkt:
            return std::make_unique<rSkt>(memory_kb);
        case BaseSketchType::FreeRS:
            return std::make_unique<FreeRS>(memory_kb);
    }
    throw std::runtime_error("Unknown base sketch type.");
}

void prepareQueryIfNeeded(Sketch& sketch) {
    sketch.prepareQuery();
}

template<typename SketchType>
PerFlowResult evaluatePerFlowSketch(
        SketchType& sketch,
        Method method,
        BaseSketchType base_sketch,
        uint32_t memory_kb,
        float filter_ratio,
        const Dataset& dataset,
        const TrueCardinality& true_cardinality) {
    auto start_update = std::chrono::high_resolution_clock::now();
    for (const auto& [key, element]: dataset) {
        sketch.update(key, element);
    }
    auto end_update = std::chrono::high_resolution_clock::now();

    prepareQueryIfNeeded(sketch);

    double total_are = 0.0;
    uint32_t count = 0;

    auto start_query = std::chrono::high_resolution_clock::now();
    for (const auto& [flow_label, elements]: true_cardinality) {
        const uint32_t true_value = static_cast<uint32_t>(elements.size());
        const uint32_t estimated_value = sketch.query(flow_label);
        const double error = std::abs(static_cast<double>(estimated_value) - static_cast<double>(true_value));
        total_are += error / static_cast<double>(true_value);
        ++count;
    }
    auto end_query = std::chrono::high_resolution_clock::now();

    const std::chrono::duration<double> update_duration = end_update - start_update;
    const std::chrono::duration<double> query_duration = end_query - start_query;

    return {
            method,
            base_sketch,
            memory_kb,
            filter_ratio,
            count > 0 ? total_are / count : 0.0,
            update_duration.count() > 0 ? (dataset.size() / 1e6) / update_duration.count() : 0.0,
            query_duration.count() > 0 ? (true_cardinality.size() / 1e6) / query_duration.count() : 0.0
    };
}

PerFlowResult runPerFlowCase(
        Method method,
        BaseSketchType base_sketch,
        uint32_t memory_kb,
        float filter_ratio,
        const Dataset& dataset,
        const TrueCardinality& true_cardinality) {
    switch (method) {
        case Method::FlipFilter: {
            FlipFilter filter(memory_kb,
                              filter_ratio,
                              FLIP_LAYERS,
                              FLIP_BITMAP_SIZES,
                              FLIP_RATIO_THRESHOLD,
                              FLIP_LAYER_MEMORY_RATIOS,
                              FLIP_DISTRIBUTE_NUM,
                              base_sketch);
            return evaluatePerFlowSketch(filter, method, base_sketch, memory_kb, filter_ratio, dataset, true_cardinality);
        }
        case Method::Couper: {
            Couper filter(memory_kb, filter_ratio, base_sketch);
            return evaluatePerFlowSketch(filter, method, base_sketch, memory_kb, filter_ratio, dataset, true_cardinality);
        }
        case Method::CouponFilter: {
            CouponFilter filter(memory_kb, filter_ratio, base_sketch);
            return evaluatePerFlowSketch(filter, method, base_sketch, memory_kb, filter_ratio, dataset, true_cardinality);
        }
        case Method::LogLogFilterSpread: {
            LogLogFilter_Spread filter(memory_kb, filter_ratio, base_sketch);
            filter.setPerFlowMode(true);
            return evaluatePerFlowSketch(filter, method, base_sketch, memory_kb, filter_ratio, dataset, true_cardinality);
        }
        case Method::SketchOnly:
            break;
    }
    throw std::runtime_error("Unknown per-flow method.");
}

template<typename SketchType>
std::vector<SSResult> evaluateSuperSpreaderSketch(
        SketchType& sketch,
        Method method,
        BaseSketchType base_sketch,
        uint32_t memory_kb,
        float filter_ratio,
        const Dataset& dataset,
        const TrueCardinality& true_cardinality) {
    for (const auto& [key, element]: dataset) {
        sketch.update(key, element);
    }

    prepareQueryIfNeeded(sketch);

    std::vector<SSResult> results;
    results.reserve(SS_THRESHOLDS.size());

    for (uint32_t threshold: SS_THRESHOLDS) {
        std::unordered_map<uint32_t, uint32_t> ground_truth;
        for (const auto& [key, elements]: true_cardinality) {
            if (elements.size() > threshold) {
                ground_truth[key] = static_cast<uint32_t>(elements.size());
            }
        }

        auto detected = sketch.detect(threshold);
        uint32_t true_positive = 0;
        double total_are = 0.0;

        for (const auto& [key, estimated_value]: detected) {
            auto gt_iter = ground_truth.find(key);
            if (gt_iter == ground_truth.end()) continue;

            ++true_positive;
            total_are += std::abs(static_cast<double>(estimated_value) - static_cast<double>(gt_iter->second))
                         / static_cast<double>(gt_iter->second);
        }

        const double precision = detected.empty() ? 0.0 : static_cast<double>(true_positive) / detected.size();
        const double recall = ground_truth.empty() ? 0.0 : static_cast<double>(true_positive) / ground_truth.size();
        const double f1 = (precision + recall > 0.0)
                          ? 2.0 * precision * recall / (precision + recall)
                          : 0.0;
        const double are = true_positive > 0 ? total_are / true_positive : 0.0;

        results.push_back({method, base_sketch, memory_kb, filter_ratio, threshold, f1, are});
    }

    return results;
}

std::vector<SSResult> runSuperSpreaderCase(
        Method method,
        BaseSketchType base_sketch,
        uint32_t memory_kb,
        float filter_ratio,
        const Dataset& dataset,
        const TrueCardinality& true_cardinality) {
    switch (method) {
        case Method::FlipFilter: {
            FlipFilter filter(memory_kb,
                              filter_ratio,
                              FLIP_LAYERS,
                              FLIP_BITMAP_SIZES,
                              FLIP_RATIO_THRESHOLD,
                              FLIP_LAYER_MEMORY_RATIOS,
                              FLIP_DISTRIBUTE_NUM,
                              base_sketch);
            return evaluateSuperSpreaderSketch(filter, method, base_sketch, memory_kb, filter_ratio, dataset, true_cardinality);
        }
        case Method::Couper: {
            Couper filter(memory_kb, filter_ratio, base_sketch);
            return evaluateSuperSpreaderSketch(filter, method, base_sketch, memory_kb, filter_ratio, dataset, true_cardinality);
        }
        case Method::CouponFilter: {
            CouponFilter filter(memory_kb, filter_ratio, base_sketch);
            return evaluateSuperSpreaderSketch(filter, method, base_sketch, memory_kb, filter_ratio, dataset, true_cardinality);
        }
        case Method::LogLogFilterSpread: {
            LogLogFilter_Spread filter(memory_kb, filter_ratio, base_sketch);
            filter.setPerFlowMode(false);
            return evaluateSuperSpreaderSketch(filter, method, base_sketch, memory_kb, filter_ratio, dataset, true_cardinality);
        }
        case Method::SketchOnly:
            break;
    }
    throw std::runtime_error("Unknown super spreader method.");
}

PerFlowResult runSketchOnlyPerFlowCase(
        BaseSketchType base_sketch,
        uint32_t memory_kb,
        const Dataset& dataset,
        const TrueCardinality& true_cardinality) {
    auto sketch = createSketchOnlyBaseSketch(memory_kb, base_sketch);
    return evaluatePerFlowSketch(*sketch,
                                 Method::SketchOnly,
                                 base_sketch,
                                 memory_kb,
                                 0.0f,
                                 dataset,
                                 true_cardinality);
}

std::vector<SSResult> runSketchOnlySuperSpreaderCase(
        BaseSketchType base_sketch,
        uint32_t memory_kb,
        const Dataset& dataset,
        const TrueCardinality& true_cardinality) {
    auto sketch = createSketchOnlyBaseSketch(memory_kb, base_sketch);
    return evaluateSuperSpreaderSketch(*sketch,
                                       Method::SketchOnly,
                                       base_sketch,
                                       memory_kb,
                                       0.0f,
                                       dataset,
                                       true_cardinality);
}

void writePerFlowHeader(std::ofstream& out) {
    out << "dataset,task,method,base_sketch,memory_kb,filter_ratio,"
        << "flip_layers,flip_bitmap_sizes,flip_ratio_threshold,flip_layer_memory_ratio,flip_distribute_num,"
        << "ARE,update_Mpps,query_Mfps\n";
}

void writePerFlowRow(std::ofstream& out, const PerFlowResult& result) {
    const bool sketch_only = result.method == Method::SketchOnly;
    out << datasetName(DATASET_CHOICE) << ","
        << "PerFlow,"
        << methodName(result.method) << ","
        << sketchName(result.base_sketch) << ","
        << result.memory_kb << ","
        << formatFloat(result.filter_ratio) << ","
        << (sketch_only ? 0 : FLIP_LAYERS) << ","
        << (sketch_only ? "{}" : formatIntVector(FLIP_BITMAP_SIZES)) << ","
        << (sketch_only ? formatFloat(0.0f) : formatFloat(FLIP_RATIO_THRESHOLD)) << ","
        << (sketch_only ? "{}" : formatFloatVector(FLIP_LAYER_MEMORY_RATIOS)) << ","
        << (sketch_only ? 0 : FLIP_DISTRIBUTE_NUM) << ","
        << result.are << ","
        << result.update_mpps << ","
        << result.query_mfps << "\n";
}

void writeSSDetailHeader(std::ofstream& out) {
    out << "dataset,task,method,base_sketch,memory_kb,filter_ratio,"
        << "flip_layers,flip_bitmap_sizes,flip_ratio_threshold,flip_layer_memory_ratio,flip_distribute_num,"
        << "threshold,F1,ARE\n";
}

void writeSSDetailRow(std::ofstream& out, const SSResult& result) {
    const bool sketch_only = result.method == Method::SketchOnly;
    out << datasetName(DATASET_CHOICE) << ","
        << "SuperSpreader,"
        << methodName(result.method) << ","
        << sketchName(result.base_sketch) << ","
        << result.memory_kb << ","
        << formatFloat(result.filter_ratio) << ","
        << (sketch_only ? 0 : FLIP_LAYERS) << ","
        << (sketch_only ? "{}" : formatIntVector(FLIP_BITMAP_SIZES)) << ","
        << (sketch_only ? formatFloat(0.0f) : formatFloat(FLIP_RATIO_THRESHOLD)) << ","
        << (sketch_only ? "{}" : formatFloatVector(FLIP_LAYER_MEMORY_RATIOS)) << ","
        << (sketch_only ? 0 : FLIP_DISTRIBUTE_NUM) << ","
        << result.threshold << ","
        << result.f1 << ","
        << result.are << "\n";
}

void printExperimentSetting() {
    std::cout << "Dataset: " << datasetName(DATASET_CHOICE) << std::endl;
    std::cout << "Memory values:";
    for (uint32_t memory_kb: MEMORY_VALUES) std::cout << " " << memory_kb;
    std::cout << " KB" << std::endl;
    std::cout << "Filter ratios:";
    for (float filter_ratio: FILTER_RATIOS) std::cout << " " << formatFloat(filter_ratio);
    std::cout << std::endl;
    std::cout << "FlipFilter: layers=" << FLIP_LAYERS
              << ", bitmap_sizes=" << formatIntVector(FLIP_BITMAP_SIZES)
              << ", ratio_threshold=" << formatFloat(FLIP_RATIO_THRESHOLD)
              << ", layer_memory=" << formatFloatVector(FLIP_LAYER_MEMORY_RATIOS)
              << ", d=" << FLIP_DISTRIBUTE_NUM << std::endl;
}

int main() {
    const auto start_time = std::chrono::steady_clock::now();
    auto [dataset, true_cardinality] = loadDataset(DATASET_CHOICE);

    std::cout << "Loaded " << dataset.size()
              << " records, " << true_cardinality.size()
              << " flows." << std::endl;
    printExperimentSetting();

    const std::string dataset_name = datasetName(DATASET_CHOICE);
    const std::filesystem::path perflow_path =
            resolveResultPath("main_compare_" + dataset_name + "_perflow.csv");
    const std::filesystem::path ss_detail_path =
            resolveResultPath("main_compare_" + dataset_name + "_ss_threshold_detail.csv");

    std::ofstream perflow_out;
    std::ofstream ss_detail_out;

    if (RUN_PERFLOW_EXPERIMENT) {
        perflow_out.open(perflow_path, std::ios::out);
        if (!perflow_out.is_open()) {
            throw std::runtime_error("Failed to open result file: " + perflow_path.string());
        }
        writePerFlowHeader(perflow_out);
    }

    if (RUN_SS_EXPERIMENT) {
        ss_detail_out.open(ss_detail_path, std::ios::out);
        if (!ss_detail_out.is_open()) {
            throw std::runtime_error("Failed to open result file: " + ss_detail_path.string());
        }
        writeSSDetailHeader(ss_detail_out);
    }

    if (RUN_PERFLOW_EXPERIMENT) {
        std::cout << "\nRunning per-flow spread estimation..." << std::endl;

        if (RUN_SKETCH_ONLY_BASELINE) {
            for (BaseSketchType base_sketch: PERFLOW_BASE_SKETCHES) {
                for (uint32_t memory_kb: MEMORY_VALUES) {
                    const auto result = runSketchOnlyPerFlowCase(base_sketch, memory_kb, dataset, true_cardinality);
                    writePerFlowRow(perflow_out, result);
                    std::cout << "SketchOnly + " << sketchName(base_sketch)
                              << " memory=" << memory_kb
                              << "KB ARE=" << result.are << std::endl;
                }
            }
        }

        for (BaseSketchType base_sketch: PERFLOW_BASE_SKETCHES) {
            for (uint32_t memory_kb: MEMORY_VALUES) {
                for (float filter_ratio: FILTER_RATIOS) {
                    for (Method method: METHODS) {
                        const auto result = runPerFlowCase(method,
                                                           base_sketch,
                                                           memory_kb,
                                                           filter_ratio,
                                                           dataset,
                                                           true_cardinality);
                        writePerFlowRow(perflow_out, result);
                        std::cout << methodName(method) << " + " << sketchName(base_sketch)
                                  << " memory=" << memory_kb
                                  << "KB ARE=" << result.are << std::endl;
                    }
                }
            }
        }
    }

    if (RUN_SS_EXPERIMENT) {
        std::cout << "\nRunning super spreader detection..." << std::endl;

        if (RUN_SKETCH_ONLY_BASELINE) {
            for (BaseSketchType base_sketch: SS_BASE_SKETCHES) {
                for (uint32_t memory_kb: MEMORY_VALUES) {
                    const auto results = runSketchOnlySuperSpreaderCase(base_sketch, memory_kb, dataset, true_cardinality);
                    for (const auto& result: results) {
                        writeSSDetailRow(ss_detail_out, result);
                        std::cout << "SketchOnly + " << sketchName(base_sketch)
                                  << " memory=" << memory_kb
                                  << "KB threshold=" << result.threshold
                                  << " F1=" << result.f1
                                  << " ARE=" << result.are << std::endl;
                    }
                }
            }
        }

        for (BaseSketchType base_sketch: SS_BASE_SKETCHES) {
            for (uint32_t memory_kb: MEMORY_VALUES) {
                for (float filter_ratio: FILTER_RATIOS) {
                    for (Method method: METHODS) {
                        const auto results = runSuperSpreaderCase(method,
                                                                  base_sketch,
                                                                  memory_kb,
                                                                  filter_ratio,
                                                                  dataset,
                                                                  true_cardinality);
                        for (const auto& result: results) {
                            writeSSDetailRow(ss_detail_out, result);
                            std::cout << methodName(method) << " + " << sketchName(base_sketch)
                                      << " memory=" << memory_kb
                                      << "KB threshold=" << result.threshold
                                      << " F1=" << result.f1
                                      << " ARE=" << result.are << std::endl;
                        }
                    }
                }
            }
        }
    }

    if (perflow_out.is_open()) perflow_out.close();
    if (ss_detail_out.is_open()) ss_detail_out.close();

    std::cout << "\nResult files:" << std::endl;
    if (RUN_PERFLOW_EXPERIMENT) {
        std::cout << "  " << perflow_path.string() << std::endl;
    }
    if (RUN_SS_EXPERIMENT) {
        std::cout << "  " << ss_detail_path.string() << std::endl;
    }

    const auto end_time = std::chrono::steady_clock::now();
    const auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time).count();
    std::cout << "Elapsed time: "
              << elapsed_time / 3600 << "h "
              << (elapsed_time % 3600) / 60 << "m "
              << elapsed_time % 60 << "s" << std::endl;

    return 0;
}

