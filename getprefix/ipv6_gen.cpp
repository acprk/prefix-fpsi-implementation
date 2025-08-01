#include <iostream>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <filesystem>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <future>
#include <set>
#include <cstdint>

// 定义128位整数别名
using uint128_t = __uint128_t;

// 为uint128_t定义ostream输出操作符
std::ostream& operator<<(std::ostream& os, uint128_t value) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(32) << value;
    return os << oss.str();
}

class ImprovedFuzzyPSI {
private:
    // 前缀到IP的映射
    std::unordered_map<std::string, std::vector<uint128_t>> sender_prefix_to_ips;
    std::unordered_map<std::string, std::vector<uint128_t>> receiver_prefix_to_ips;
    
    // 改进的编码系统：使用区间表示
    struct PrefixInterval {
        uint128_t start;
        uint128_t end;
        std::string original_prefix;
        
        bool overlaps(const PrefixInterval& other) const {
            return !(end < other.start || other.end < start);
        }
    };
    
    std::vector<PrefixInterval> sender_intervals;
    std::vector<PrefixInterval> receiver_intervals;
    
    // 原始数据集
    std::vector<uint128_t> original_sender_ips;
    std::vector<uint128_t> original_receiver_ips;
    
    std::string volepsi_path;
    std::string sender_prefix_path;
    std::string receiver_prefix_path;
    std::string sender_ip_path;
    std::string receiver_ip_path;
    int delta;
    
    // 分桶参数
    const uint128_t BUCKET_SIZE = (uint128_t)1 << 7; // 128, covers 2 * δ = 100
    
    // 统计信息
    struct Statistics {
        int total_sender_ips = 0;
        int total_receiver_ips = 0;
        int total_sender_prefixes = 0;
        int total_receiver_prefixes = 0;
        int psi_intersection_size = 0;
        int final_matches = 0;
        int unique_yj_values = 0;
        int ground_truth_matches = 0;
        int true_positives = 0;
        int false_positives = 0;
        int false_negatives = 0;
        std::chrono::milliseconds psi_execution_time{0};
    } stats;
    
    // 工具函数
    std::string uint128_to_ip(uint128_t ip) {
        std::ostringstream oss;
        for (int i = 7; i >= 0; i--) {
            uint16_t segment = (ip >> (i * 16)) & 0xFFFF;
            oss << std::hex << segment;
            if (i > 0) oss << ":";
        }
        return oss.str();
    }
    
    std::string trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t\r\n");
        return str.substr(start, end - start + 1);
    }
    
    // 将前缀转换为区间
    PrefixInterval prefix_to_interval(const std::string& prefix) {
        uint128_t start = 0, end = 0;
        int bit_pos = 127;
        
        for (char c : prefix) {
            if (c == '0') {
                bit_pos--;
            } else if (c == '1') {
                start |= ((uint128_t)1 << bit_pos);
                end |= ((uint128_t)1 << bit_pos);
                bit_pos--;
            } else if (c == '*') {
                end |= ((uint128_t)1 << bit_pos);
                bit_pos--;
            }
        }
        
        return {start, end, prefix};
    }
    
public:
    ImprovedFuzzyPSI(const std::string& psi_path, 
                     const std::string& s_prefix_path,
                     const std::string& r_prefix_path,
                     const std::string& s_ip_path,
                     const std::string& r_ip_path,
                     int d) 
        : volepsi_path(psi_path), 
          sender_prefix_path(s_prefix_path),
          receiver_prefix_path(r_prefix_path),
          sender_ip_path(s_ip_path),
          receiver_ip_path(r_ip_path),
          delta(d) {}
    
    bool load_data() {
        std::cout << "=== 加载数据 ===" << std::endl;
        
        if (!load_ip_data()) {
            return false;
        }
        
        if (!load_prefix_data()) {
            return false;
        }
        
        compute_ground_truth();
        
        std::cout << "\n📊 数据加载统计:" << std::endl;
        std::cout << "  原始Sender IPs: " << stats.total_sender_ips << " 个" << std::endl;
        std::cout << "  原始Receiver IPs: " << stats.total_receiver_ips << " 个" << std::endl;
        std::cout << "  Sender前缀: " << stats.total_sender_prefixes << " 个" << std::endl;
        std::cout << "  Receiver前缀: " << stats.total_receiver_prefixes << " 个" << std::endl;
        std::cout << "  真实匹配对数: " << stats.ground_truth_matches << " 对" << std::endl;
        
        return true;
    }
    
private:
    bool load_ip_data() {
        std::cout << "  🔄 加载原始IPv6数据..." << std::endl;
        
        std::ifstream sender_file(sender_ip_path);
        if (!sender_file.is_open()) {
            std::cerr << "❌ 无法打开Sender IP文件: " << sender_ip_path << std::endl;
            return false;
        }
        
        std::string line;
        while (std::getline(sender_file, line)) {
            if (line.empty() || line[0] == '#') continue;
            
            size_t pos = line.find(',');
            if (pos != std::string::npos) {
                pos = line.find(',', pos + 1);
                if (pos != std::string::npos) {
                    std::string ip_str = line.substr(pos + 1);
                    pos = ip_str.find(',');
                    if (pos != std::string::npos) {
                        ip_str = ip_str.substr(0, pos);
                        ip_str = trim(ip_str);
                        try {
                            uint128_t ip;
                            std::istringstream iss(ip_str);
                            iss >> ip;
                            original_sender_ips.push_back(ip);
                        } catch (...) {}
                    }
                }
            }
        }
        sender_file.close();
        stats.total_sender_ips = original_sender_ips.size();
        
        std::ifstream receiver_file(receiver_ip_path);
        if (!receiver_file.is_open()) {
            std::cerr << "❌ 无法打开Receiver IP文件: " << receiver_ip_path << std::endl;
            return false;
        }
        
        while (std::getline(receiver_file, line)) {
            if (line.empty() || line[0] == '#') continue;
            
            size_t pos = line.find(',');
            if (pos != std::string::npos) {
                pos = line.find(',', pos + 1);
                if (pos != std::string::npos) {
                    std::string ip_str = line.substr(pos + 1);
                    pos = ip_str.find(',');
                    if (pos != std::string::npos) {
                        ip_str = ip_str.substr(0, pos);
                        ip_str = trim(ip_str);
                        try {
                            uint128_t ip;
                            std::istringstream iss(ip_str);
                            iss >> ip;
                            original_receiver_ips.push_back(ip);
                        } catch (...) {}
                    }
                }
            }
        }
        receiver_file.close();
        stats.total_receiver_ips = original_receiver_ips.size();
        
        std::cout << "    ✅ 加载Sender IPs: " << stats.total_sender_ips << " 个" << std::endl;
        std::cout << "    ✅ 加载Receiver IPs: " << stats.total_receiver_ips << " 个" << std::endl;
        
        return true;
    }
    
    bool load_prefix_data() {
        std::cout << "  🔄 加载前缀数据..." << std::endl;
        
        if (!load_prefix_file(sender_prefix_path, sender_prefix_to_ips, sender_intervals, "Sender")) {
            return false;
        }
        stats.total_sender_prefixes = sender_prefix_to_ips.size();
        
        if (!load_prefix_file(receiver_prefix_path, receiver_prefix_to_ips, receiver_intervals, "Receiver")) {
            return false;
        }
        stats.total_receiver_prefixes = receiver_prefix_to_ips.size();
        
        return true;
    }
    
    bool load_prefix_file(const std::string& filename,
                         std::unordered_map<std::string, std::vector<uint128_t>>& prefix_map,
                         std::vector<PrefixInterval>& intervals,
                         const std::string& type) {
        
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "❌ 无法打开" << type << "前缀文件: " << filename << std::endl;
            return false;
        }
        
        std::string line;
        uint128_t current_ip = 0;
        bool in_prefix_section = false;
        
        while (std::getline(file, line)) {
            line = trim(line);
            if (line.empty() || line[0] == '#') continue;
            
            size_t arrow_pos = line.find(" -> ");
            if (arrow_pos != std::string::npos) {
                size_t paren_start = line.find('(');
                size_t paren_end = line.find(')');
                if (paren_start != std::string::npos && paren_end != std::string::npos) {
                    std::string ip_str = line.substr(paren_start + 1, paren_end - paren_start - 1);
                    try {
                        std::istringstream iss(ip_str);
                        iss >> current_ip;
                        in_prefix_section = true;
                        continue;
                    } catch (...) {}
                }
            }
            
            if (in_prefix_section && line.find('.') != std::string::npos) {
                size_t dot_pos = line.find('.');
                if (dot_pos != std::string::npos && dot_pos < line.length() - 1) {
                    std::string prefix = line.substr(dot_pos + 1);
                    prefix = trim(prefix);
                    
                    if (!prefix.empty() && prefix.find("邻域区间") == std::string::npos) {
                        prefix_map[prefix].push_back(current_ip);
                        PrefixInterval interval = prefix_to_interval(prefix);
                        intervals.push_back(interval);
                    }
                }
            }
            
            if (line.empty()) {
                in_prefix_section = false;
            }
        }
        file.close();
        
        std::cout << "    ✅ 加载" << type << "前缀: " << prefix_map.size() << " 个唯一前缀" << std::endl;
        std::cout << "    ✅ 生成" << type << "区间: " << intervals.size() << " 个" << std::endl;
        
        return true;
    }
    
    void compute_ground_truth() {
        std::cout << "  🔄 计算真实匹配对..." << std::endl;
        
        stats.ground_truth_matches = 0;
        for (uint128_t s : original_sender_ips) {
            for (uint128_t r : original_receiver_ips) {
                __int128_t diff = (__int128_t)s - (__int128_t)r;
                if (diff < 0) diff = -diff;
                if (diff <= delta) {
                    stats.ground_truth_matches++;
                }
            }
        }
        
        std::cout << "    ✅ 真实匹配对数: " << stats.ground_truth_matches << " 对" << std::endl;
    }
    
public:
    void generate_improved_psi_files() {
        std::cout << "\n=== 生成改进的PSI输入文件 ===" << std::endl;
        
        generate_hash_bucketing();
    }
    
private:
    void generate_hash_bucketing() {
        std::cout << "  🔧 使用分桶编码策略..." << std::endl;
        
        std::ofstream sender_file("sender_improved.csv");
        std::ofstream receiver_file("receiver_improved.csv");
        
        std::set<std::string> sender_codes, receiver_codes;
        
        // 处理Sender前缀
        for (const auto& interval : sender_intervals) {
            uint128_t start_bucket = interval.start / BUCKET_SIZE;
            uint128_t end_bucket = interval.end / BUCKET_SIZE;
            for (uint128_t bucket = start_bucket; bucket <= end_bucket; bucket++) {
                uint128_t val = bucket * BUCKET_SIZE;
                std::ostringstream oss;
                oss << std::hex << std::setfill('0') << std::setw(32) << val;
                sender_codes.insert(oss.str());
            }
        }
        
        // 处理Receiver前缀
        for (const auto& interval : receiver_intervals) {
            uint128_t start_bucket = interval.start / BUCKET_SIZE;
            uint128_t end_bucket = interval.end / BUCKET_SIZE;
            for (uint128_t bucket = start_bucket; bucket <= end_bucket; bucket++) {
                uint128_t val = bucket * BUCKET_SIZE;
                std::ostringstream oss;
                oss << std::hex << std::setfill('0') << std::setw(32) << val;
                receiver_codes.insert(oss.str());
            }
        }
        
        for (const auto& code : sender_codes) {
            sender_file << code << std::endl;
        }
        
        for (const auto& code : receiver_codes) {
            receiver_file << code << std::endl;
        }
        
        sender_file.close();
        receiver_file.close();
        
        std::cout << "    ✅ Sender编码数: " << sender_codes.size() << std::endl;
        std::cout << "    ✅ Receiver编码数: " << receiver_codes.size() << std::endl;
        
        save_encoding_info();
    }
    
    void save_encoding_info() {
        std::ofstream info_file("encoding_info.txt");
        info_file << "# 改进的前缀编码信息\n";
        info_file << "# 使用分桶编码策略 (桶大小: " << BUCKET_SIZE << ")\n";
        info_file << "# Sender区间数: " << sender_intervals.size() << "\n";
        info_file << "# Receiver区间数: " << receiver_intervals.size() << "\n";
        info_file.close();
    }
    
public:
    bool run_volepsi() {
        std::cout << "\n=== 运行volePSI协议 ===" << std::endl;
        
        // 检查system调用返回值
        if (std::system("rm -f sender_intersection.csv receiver_intersection.csv *.csv.out") != 0) {
            std::cerr << "⚠️ 清理旧文件失败" << std::endl;
        }
        
        int port = 1212;
        std::string server_addr = "localhost:" + std::to_string(port);
        
        std::string receiver_cmd = volepsi_path + 
            " -in receiver_improved.csv" +
            " -r 1" +
            " -ip " + server_addr +
            " -server 0";
            
        std::string sender_cmd = volepsi_path + 
            " -in sender_improved.csv" +
            " -r 0" +
            " -ip " + server_addr +
            " -server 1";
        
        std::cout << "📡 执行PSI..." << std::endl;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        auto receiver_future = std::async(std::launch::async, [&]() {
            return std::system(receiver_cmd.c_str());
        });
        
        std::this_thread::sleep_for(std::chrono::milliseconds(75));
        
        int sender_result = std::system(sender_cmd.c_str());
        
        int receiver_result = receiver_future.get();
        
        auto end_time = std::chrono::high_resolution_clock::now();
        stats.psi_execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        std::cout << "⏱️  PSI执行时间: " << stats.psi_execution_time.count() << " ms" << std::endl;
        
        if (sender_result != 0 || receiver_result != 0) {
            std::cerr << "❌ volePSI执行失败 (sender_result=" << sender_result 
                      << ", receiver_result=" << receiver_result << ")" << std::endl;
            return false;
        }
        
        return check_psi_output();
    }
    
private:
    bool check_psi_output() {
        if (std::filesystem::exists("sender_improved.csv.out")) {
            std::filesystem::copy_file("sender_improved.csv.out", "sender_intersection.csv");
        }
        if (std::filesystem::exists("receiver_improved.csv.out")) {
            std::filesystem::copy_file("receiver_improved.csv.out", "receiver_intersection.csv");
        }
        
        return std::filesystem::exists("receiver_intersection.csv") || 
               std::filesystem::exists("sender_intersection.csv");
    }
    
public:
    std::vector<std::pair<uint128_t, uint128_t>> process_results() {
        std::cout << "\n=== 处理PSI结果 ===" << std::endl;
        
        std::set<uint128_t> psi_values = read_psi_intersection();
        stats.psi_intersection_size = psi_values.size();
        
        std::cout << "🔗 PSI找到 " << stats.psi_intersection_size << " 个交集值" << std::endl;
        
        return map_to_original_ips(psi_values);
    }
    
private:
    std::set<uint128_t> read_psi_intersection() {
        std::set<uint128_t> values;
        
        std::string result_file = "receiver_intersection.csv";
        if (!std::filesystem::exists(result_file)) {
            result_file = "sender_intersection.csv";
        }
        
        std::ifstream file(result_file);
        std::string line;
        
        while (std::getline(file, line)) {
            line = trim(line);
            if (!line.empty()) {
                try {
                    uint128_t val;
                    std::istringstream iss(line);
                    iss >> std::hex >> val;
                    values.insert(val);
                } catch (...) {}
            }
        }
        
        return values;
    }
    
    std::vector<std::pair<uint128_t, uint128_t>> map_to_original_ips(const std::set<uint128_t>& psi_values) {
        std::cout << "  🔄 映射回原始IPv6..." << std::endl;
        
        std::set<uint128_t> sender_candidates, receiver_candidates;
        
        for (uint128_t val : psi_values) {
            uint128_t bucket = val / BUCKET_SIZE;
            uint128_t bucket_start = bucket * BUCKET_SIZE;
            uint128_t bucket_end = (bucket + 1) * BUCKET_SIZE - 1;
            for (size_t i = 0; i < sender_intervals.size(); i++) {
                if (sender_intervals[i].overlaps({bucket_start, bucket_end, ""})) {
                    const std::string& prefix = sender_intervals[i].original_prefix;
                    if (sender_prefix_to_ips.count(prefix)) {
                        for (uint128_t ip : sender_prefix_to_ips[prefix]) {
                            sender_candidates.insert(ip);
                        }
                    }
                }
            }
            
            for (size_t i = 0; i < receiver_intervals.size(); i++) {
                if (receiver_intervals[i].overlaps({bucket_start, bucket_end, ""})) {
                    const std::string& prefix = receiver_intervals[i].original_prefix;
                    if (receiver_prefix_to_ips.count(prefix)) {
                        for (uint128_t ip : receiver_prefix_to_ips[prefix]) {
                            receiver_candidates.insert(ip);
                        }
                    }
                }
            }
        }
        
        std::cout << "    ✅ Sender候选: " << sender_candidates.size() << " 个" << std::endl;
        std::cout << "    ✅ Receiver候选: " << receiver_candidates.size() << " 个" << std::endl;
        
        std::vector<std::pair<uint128_t, uint128_t>> matches;
        for (uint128_t s : sender_candidates) {
            for (uint128_t r : receiver_candidates) {
                __int128_t diff = (__int128_t)s - (__int128_t)r;
                if (diff < 0) diff = -diff;
                if (diff <= delta) {
                    matches.push_back({s, r});
                }
            }
        }
        
        stats.final_matches = matches.size();
        
        std::set<uint128_t> unique_yj;
        for (const auto& m : matches) {
            unique_yj.insert(m.second);
        }
        stats.unique_yj_values = unique_yj.size();
        
        return matches;
    }
    
public:
    void compare_with_ground_truth(const std::vector<std::pair<uint128_t, uint128_t>>& psi_matches) {
        std::cout << "\n=== 与原始数据集对比 ===" << std::endl;
        
        std::set<std::pair<uint128_t, uint128_t>> ground_truth_set;
        for (uint128_t s : original_sender_ips) {
            for (uint128_t r : original_receiver_ips) {
                __int128_t diff = (__int128_t)s - (__int128_t)r;
                if (diff < 0) diff = -diff;
                if (diff <= delta) {
                    ground_truth_set.insert({s, r});
                }
            }
        }
        
        std::set<std::pair<uint128_t, uint128_t>> psi_set(psi_matches.begin(), psi_matches.end());
        
        stats.true_positives = 0;
        stats.false_positives = 0;
        stats.false_negatives = 0;
        
        for (const auto& match : psi_set) {
            if (ground_truth_set.count(match) > 0) {
                stats.true_positives++;
            } else {
                stats.false_positives++;
            }
        }
        
        for (const auto& match : ground_truth_set) {
            if (psi_set.count(match) == 0) {
                stats.false_negatives++;
            }
        }
        
        double precision = (stats.true_positives > 0) ? 
            (double)stats.true_positives / (stats.true_positives + stats.false_positives) : 0.0;
        double recall = (stats.true_positives > 0) ? 
            (double)stats.true_positives / (stats.true_positives + stats.false_negatives) : 0.0;
        double f1_score = (precision + recall > 0) ? 
            2 * precision * recall / (precision + recall) : 0.0;
        
        std::cout << "\n📊 对比结果统计:" << std::endl;
        std::cout << "  真实匹配对数: " << ground_truth_set.size() << std::endl;
        std::cout << "  PSI识别对数: " << psi_set.size() << std::endl;
        std::cout << "  ✅ True Positives: " << stats.true_positives << std::endl;
        std::cout << "  ❌ False Positives: " << stats.false_positives << std::endl;
        std::cout << "  ❌ False Negatives: " << stats.false_negatives << std::endl;
        std::cout << "\n📈 性能指标:" << std::endl;
        std::cout << "  准确率 (Precision): " << std::fixed << std::setprecision(2) << precision * 100 << "%" << std::endl;
        std::cout << "  召回率 (Recall): " << std::fixed << std::setprecision(2) << recall * 100 << "%" << std::endl;
        std::cout << "  F1分数: " << std::fixed << std::setprecision(4) << f1_score << std::endl;
        
        analyze_missed_matches(ground_truth_set, psi_set);
        
        save_comparison_report(ground_truth_set, psi_set, precision, recall, f1_score);
    }
    
private:
    void analyze_missed_matches(const std::set<std::pair<uint128_t, uint128_t>>& ground_truth,
                               const std::set<std::pair<uint128_t, uint128_t>>& psi_matches) {
        std::cout << "\n🔍 分析遗漏的匹配..." << std::endl;
        
        std::vector<std::pair<uint128_t, uint128_t>> missed_matches;
        for (const auto& match : ground_truth) {
            if (psi_matches.count(match) == 0) {
                missed_matches.push_back(match);
            }
        }
        
        if (missed_matches.empty()) {
            std::cout << "    ✅ 没有遗漏的匹配！" << std::endl;
            return;
        }
        
        std::cout << "    ⚠️ 发现 " << missed_matches.size() << " 个遗漏的匹配对" << std::endl;
        
        std::ofstream missed_file("missed_matches.txt");
        missed_file << "# 遗漏的匹配对分析\n";
        missed_file << "# 总计遗漏: " << missed_matches.size() << " 对\n";
        missed_file << "# 格式: Sender_IP, Receiver_IP, 距离\n";
        
        for (const auto& match : missed_matches) {
            uint128_t sender_ip = match.first;
            uint128_t receiver_ip = match.second;
            __int128_t diff = (__int128_t)sender_ip - (__int128_t)receiver_ip;
            if (diff < 0) diff = -diff;
            int64_t distance = (int64_t)diff;
            
            missed_file << uint128_to_ip(sender_ip) << " (" << sender_ip << "), "
                       << uint128_to_ip(receiver_ip) << " (" << receiver_ip << "), "
                       << "距离: " << distance << "\n";
            
            bool sender_in_prefix = false, receiver_in_prefix = false;
            for (const auto& interval : sender_intervals) {
                if (sender_ip >= interval.start && sender_ip <= interval.end) {
                    sender_in_prefix = true;
                    break;
                }
            }
            for (const auto& interval : receiver_intervals) {
                if (receiver_ip >= interval.start && receiver_ip <= interval.end) {
                    receiver_in_prefix = true;
                    break;
                }
            }
            
            if (!sender_in_prefix || !receiver_in_prefix) {
                missed_file << "    原因: " << (!sender_in_prefix ? "Sender IP不在任何前缀区间" : "")
                            << (!sender_in_prefix && !receiver_in_prefix ? ", " : "")
                            << (!receiver_in_prefix ? "Receiver IP不在任何前缀区间" : "") << "\n";
            } else if (distance > delta) {
                missed_file << "    原因: 距离 (" << distance << ") 超过阈值 δ (" << delta << ")\n";
            } else {
                missed_file << "    原因: 可能的编码或PSI协议遗漏\n";
            }
        }
        
        missed_file.close();
        std::cout << "    📝 遗漏匹配分析已保存至 missed_matches.txt" << std::endl;
    }
    
    void save_comparison_report(const std::set<std::pair<uint128_t, uint128_t>>& ground_truth,
                               const std::set<std::pair<uint128_t, uint128_t>>& psi_matches,
                               double precision, double recall, double f1_score) {
        std::cout << "  📝 保存对比报告..." << std::endl;
        
        std::ofstream report_file("psi_comparison_report.txt");
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %X");
        
        report_file << "# Fuzzy PSI对比报告 (IPv6)\n";
        report_file << "# 生成时间: " << ss.str() << "\n";
        report_file << "# 距离阈值 δ: " << delta << "\n";
        report_file << "# 编码策略: 分桶编码 (桶大小: " << BUCKET_SIZE << ")\n\n";
        
        report_file << "== 数据统计 ==\n";
        report_file << "原始Sender IPs: " << stats.total_sender_ips << "\n";
        report_file << "原始Receiver IPs: " << stats.total_receiver_ips << "\n";
        report_file << "Sender前缀数: " << stats.total_sender_prefixes << "\n";
        report_file << "Receiver前缀数: " << stats.total_receiver_prefixes << "\n";
        report_file << "PSI交集大小: " << stats.psi_intersection_size << "\n";
        report_file << "最终匹配对数: " << stats.final_matches << "\n";
        report_file << "唯一Receiver IPs (yj): " << stats.unique_yj_values << "\n";
        report_file << "真实匹配对数: " << ground_truth.size() << "\n\n";
        
        report_file << "== 性能指标 ==\n";
        report_file << "True Positives: " << stats.true_positives << "\n";
        report_file << "False Positives: " << stats.false_positives << "\n";
        report_file << "False Negatives: " << stats.false_negatives << "\n";
        report_file << "准确率 (Precision): " << std::fixed << std::setprecision(2) << precision * 100 << "%\n";
        report_file << "召回率 (Recall): " << std::fixed << std::setprecision(2) << recall * 100 << "%\n";
        report_file << "F1分数: " << std::fixed << std::setprecision(4) << f1_score << "\n\n";
        
        report_file << "== PSI匹配结果 ==\n";
        report_file << "# 格式: Sender_IP, Receiver_IP\n";
        for (const auto& match : psi_matches) {
            report_file << uint128_to_ip(match.first) << " (" << std::hex << std::setfill('0') << std::setw(32) << match.first << std::dec << "), "
                        << uint128_to_ip(match.second) << " (" << std::hex << std::setfill('0') << std::setw(32) << match.second << std::dec << ")\n";
        }
        
        report_file.close();
        std::cout << "    ✅ 对比报告已保存至 psi_comparison_report.txt" << std::endl;
    }
    
public:
    void run() {
        if (!load_data()) {
            std::cerr << "❌ 数据加载失败，退出..." << std::endl;
            return;
        }
        
        generate_improved_psi_files();
        
        if (!run_volepsi()) {
            std::cerr << "❌ volePSI执行失败，退出..." << std::endl;
            return;
        }
        
        auto matches = process_results();
        
        compare_with_ground_truth(matches);
        
        std::cout << "\n=== 最终结果 ===" << std::endl;
        std::cout << "🎉 Fuzzy PSI执行完成！" << std::endl;
        std::cout << "  总匹配对数: " << stats.final_matches << std::endl;
        std::cout << "  唯一Receiver IPs (yj): " << stats.unique_yj_values << std::endl;
        std::cout << "  PSI执行时间: " << stats.psi_execution_time.count() << " ms" << std::endl;
        std::cout << "  详细报告见 psi_comparison_report.txt 和 missed_matches.txt" << std::endl;
    }
};

int main(int argc, char* argv[]) {
    std::string volepsi_path = "./frontend";
    std::string sender_prefix_path = "sender_prefix_data_disjoint.txt";
    std::string receiver_prefix_path = "receiver_prefix_data_disjoint.txt";
    std::string sender_ip_path = "sender_ip_data_disjoint.txt";
    std::string receiver_ip_path = "receiver_ip_data_disjoint.txt";
    int delta = 50;
    
    if (argc >= 2) volepsi_path = argv[1];
    if (argc >= 3) sender_prefix_path = argv[2];
    if (argc >= 4) receiver_prefix_path = argv[3];
    if (argc >= 5) sender_ip_path = argv[4];
    if (argc >= 6) receiver_ip_path = argv[5];
    if (argc >= 7) delta = std::stoi(argv[6]);
    
    ImprovedFuzzyPSI psi(volepsi_path, sender_prefix_path, receiver_prefix_path,
                       sender_ip_path, receiver_ip_path, delta);
    
    psi.run();
    
    return 0;
}