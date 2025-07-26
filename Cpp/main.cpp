
#include "header/FlipFilter.h"
#include "header/Couper.h"
#include "header/CouponFilter.h"
#include "header/LogLogFilter_Spread.h"
#include "header/SuperKjSkt.h"
#include "header/KjSkt.h"
#include "header/vHLL.h"
#include "header/rSkt.h"
#include "header/FreeRS.h"
#include "header/HLLSampler.h"
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <utility>
#include <iomanip>
#include <map>
#include <set>
#include <cstdint>


#ifdef _WIN32
//#include <winsock2.h>
#include <optional>

#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#endif


// load dataset and get true per-flow spread,
// auto [dataset, true_cardinality] = loadDataSet();
// first is dataset, second is related spread statistics
std::pair<std::vector<std::pair<uint32_t, uint32_t>>,
        std::unordered_map<uint32_t, std::unordered_set<uint32_t>>> loadDataSetCAIDA() {
    std::vector<std::string> file_paths = {
            "file path for your dataset"
    };

    std::vector<std::pair<uint32_t, uint32_t>> data;
    std::unordered_map<uint32_t, std::unordered_set<uint32_t>> true_cardinality;
    uint64_t total_data_num = 0;

    for (const auto &file_path: file_paths) {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << file_path << std::endl;
            continue;
        }

        std::string line;
        while (getline(file, line)) {
            std::istringstream iss(line);
            std::string source_ip, dest_ip;
            iss >> source_ip >> dest_ip;
            if (source_ip.empty() || dest_ip.empty()) continue;

            uint32_t src_ip_int = 0, dst_ip_int = 0;
            int part = 0;
            char dot;
            std::istringstream ss1(source_ip), ss2(dest_ip);
            bool valid = true;

            for (int i = 0; i < 4; i++) {
                if (!(ss1 >> part)) {
                    valid = false;
                    break;
                }
                if (part < 0 || part > 255) {
                    valid = false;
                    break;
                }
                src_ip_int = (src_ip_int << 8) | part;
                if (i < 3 && !(ss1 >> dot && dot == '.')) {
                    valid = false;
                    break;
                }
            }
            if (ss1 >> dot) valid = false;

            for (int i = 0; i < 4 && valid; i++) {
                if (!(ss2 >> part)) {
                    valid = false;
                    break;
                }
                if (part < 0 || part > 255) {
                    valid = false;
                    break;
                }
                dst_ip_int = (dst_ip_int << 8) | part;
                if (i < 3 && !(ss2 >> dot && dot == '.')) {
                    valid = false;
                    break;
                }
            }
            if (ss2 >> dot) valid = false;

            if (!valid) continue;

            data.emplace_back(src_ip_int, dst_ip_int);
            true_cardinality[src_ip_int].insert(dst_ip_int);
            total_data_num++;
        }

        std::cout << file_path << " is loaded." << std::endl;
    }

    std::cout << "\n" << file_paths.size() << " data file loaded.\n"
              << "Totaling packets: " << total_data_num
              << ", Unique flows: " << true_cardinality.size() << std::endl;
    return {data, true_cardinality};
}


std::pair<
        std::vector<std::pair<uint32_t, uint32_t>>,
        std::unordered_map<uint32_t, std::unordered_set<uint32_t>>
> loadDataSetMAWI() {
    std::vector<std::string> file_paths = {
            "file path for your dataset"
    };
    std::vector<std::pair<uint32_t, uint32_t>> data;
    std::unordered_map<uint32_t, std::unordered_set<uint32_t>> true_cardinality;
    uint64_t total_data_num = 0;
    auto parse_ip = [](const std::string &ip_str) -> std::optional<uint32_t> {
        std::stringstream ss(ip_str);
        uint32_t result = 0;
        int part;
        for (int i = 0; i < 4; ++i) {
            if (!(ss >> part)) return std::nullopt;
            if (part < 0 || part > 255) return std::nullopt;
            result = (result << 8) | part;
            if (i < 3) {
                if (ss.peek() != '.') return std::nullopt;
                ss.ignore();
            }
        }
        if (ss.rdbuf()->in_avail() != 0) return std::nullopt;
        return result;
    };

    for (const auto &file_path: file_paths) {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << file_path << std::endl;
            continue;
        }
        std::string line;
        if (!std::getline(file, line)) {
            std::cerr << "Empty file: " << file_path << std::endl;
            continue;
        }
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string src_ip_str, dst_ip_str;

            if (!std::getline(ss, src_ip_str, ',')) continue;
            if (!std::getline(ss, dst_ip_str, ',')) continue;
            auto src_ip_opt = parse_ip(src_ip_str);
            auto dst_ip_opt = parse_ip(dst_ip_str);
            if (!src_ip_opt || !dst_ip_opt) continue;
            uint32_t src_ip = src_ip_opt.value();
            uint32_t dst_ip = dst_ip_opt.value();
            data.emplace_back(src_ip, dst_ip);
            true_cardinality[src_ip].insert(dst_ip);
            total_data_num++;
        }

        std::cout << "Loaded file: " << file_path << std::endl;
    }

    std::cout << "\nFinished loading all files." << std::endl;
    std::cout << "Total packets: " << total_data_num << std::endl;
    std::cout << "Unique flows: " << true_cardinality.size() << std::endl;
    return {data, true_cardinality};
}


std::pair<std::vector<std::pair<uint32_t, uint32_t>>,
        std::unordered_map<uint32_t, std::unordered_set<uint32_t>>> loadDataSetStackOverflow() {
    std::vector<std::string> file_paths = {
            "file path for dataset"
    };
    std::vector<std::pair<uint32_t, uint32_t>> data;
    std::unordered_map<uint32_t, std::unordered_set<uint32_t>> true_cardinality;
    uint64_t total_data_num = 0;
    for (const auto &file_path: file_paths) {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << file_path << '\n';
            continue;
        }
        std::string line;
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            std::string key_str, ele_str;
            if (!(iss >> key_str >> ele_str)) continue;
            try {
                uint32_t key_int = static_cast<uint32_t>(std::stoul(key_str));
                uint32_t ele_int = static_cast<uint32_t>(std::stoul(ele_str));
                data.emplace_back(key_int, ele_int);
                true_cardinality[key_int].insert(ele_int);
                ++total_data_num;
            }
            catch (const std::exception &e) {
                std::cerr << "Parse error in file " << file_path
                          << " line: \"" << line << "\" — " << e.what() << '\n';
            }
        }
        std::cout << file_path << " is loaded.\n";
    }
    std::cout << "\n" << file_paths.size() << " files data loaded.\n"
              << "Totaling: " << total_data_num
              << ", Unique flows: " << true_cardinality.size() << std::endl;
    return {data, true_cardinality};
}


int main() {

    std::cout << "Experiment starts ..." << std::endl;
    auto start_time = std::chrono::steady_clock::now();

    auto [dataset, true_cardinality] = loadDataSetCAIDA();
//    auto [dataset, true_cardinality] = loadDataSetMAWI();
//    auto [dataset, true_cardinality] = loadDataSetStackOverflow();

    std::vector<uint32_t> memo_kb_values = {50, 100, 150, 200, 250, 300, 350, 400, 450, 500, 550};
    std::vector<uint32_t> thresholds = {50, 100, 200, 400, 800, 1000, 1500, 2000};

    for (uint32_t memo_kb: memo_kb_values) {

        std::cout << "\nMemory (KB) = " << memo_kb << std::endl;

        float filter_memo = memo_kb * 0.30;
        uint32_t sketch_memo = static_cast<uint32_t>(memo_kb - filter_memo);

        std::cout << "\n" << " vHLL: " << std::endl;
        vHLL *skt_only = new vHLL(memo_kb);
        skt_only->spreadEstimation(dataset, true_cardinality);
        delete skt_only;


        std::cout << "\n" << "FlipFilter + vHLL: " << std::endl;
        vHLL *skt_filter = new vHLL(sketch_memo);
        FlipFilter *filter = new FlipFilter(filter_memo, skt_filter);
        filter->spreadEstimation(dataset, true_cardinality);
        delete filter;
        delete skt_filter;


        std::cout << "\n" << "Couper + vHLL: " << std::endl;
        vHLL *skt_couper = new vHLL(sketch_memo);
        Couper *couper = new Couper(filter_memo, skt_couper);
        couper->spreadEstimation(dataset, true_cardinality);
        delete couper;
        delete skt_couper;


        std::cout << "\n" << "Coupon + vHLL: " << std::endl;
        vHLL *skt_coupon = new vHLL(sketch_memo);
        CouponFilter *coupon = new CouponFilter(filter_memo, skt_coupon);
        coupon->spreadEstimation(dataset, true_cardinality);
        delete coupon;
        delete skt_coupon;


        std::cout << "\n" << "LogLog + vHLL: " << std::endl;
        vHLL *skt_loglogfilter = new vHLL(sketch_memo);
        LogLogFilter_Spread *loglogfilter = new LogLogFilter_Spread(filter_memo, skt_loglogfilter);
        loglogfilter->spreadEstimation(dataset, true_cardinality);
        delete loglogfilter;
        delete skt_loglogfilter;


    }


    auto end_time = std::chrono::steady_clock::now();
    auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time).count();
    std::cout << "\nExperiment ends. Elapsed time: "
              << elapsed_time / 3600 << "h " << (elapsed_time % 3600) / 60 << "m " << elapsed_time % 60 << "s"
              <<
              std::endl;

    return 0;
}

