// encode_data.cpp
// 对IP数据进行前缀编码：Receiver前缀展开，Sender通配符填充
// 支持多个delta值：10, 50, 250

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <iomanip>
#include <cmath>
#include <bitset>
#include <sstream>
#include <filesystem>

struct IPData {
    uint32_t ip;
    std::string organization;
    std::string dataset_type;
};

class MultiDeltaPrefixEncoder {
private:
    static constexpr int BIT_LENGTH = 32;
    
    // 不同delta值的配置
    struct DeltaConfig {
        int delta;
        int wildcard_bits;
        
        DeltaConfig(int d) : delta(d) {
            // 计算需要填充的通配符位数 = log2(2*δ-1) 向下取整 + 1
            wildcard_bits = static_cast<int>(std::floor(std::log2(2 * delta - 1))) + 1;
        }
    };
    
    std::vector<DeltaConfig> delta_configs = {
        DeltaConfig(10),   // δ=10, 通配符位数=5
        DeltaConfig(50),   // δ=50, 通配符位数=7  
        DeltaConfig(250)   // δ=250, 通配符位数=9
    };
    
    // 将IPv4字符串转换为uint32_t
    uint32_t ipv4_to_uint32(const std::string& ip_str) {
        std::istringstream iss(ip_str);
        std::string token;
        uint32_t ip = 0;
        int shift = 24;
        
        while (std::getline(iss, token, '.') && shift >= 0) {
            ip |= (std::stoi(token) << shift);
            shift -= 8;
        }
        return ip;
    }
    
    // 将整数转换为二进制字符串
    std::string to_binary_string(uint32_t value, int length) const {
        std::string result(length, '0');
        for (int i = length - 1; i >= 0 && value > 0; i--) {
            result[i] = '0' + (value & 1);
            value >>= 1;
        }
        return result;
    }
    
    // 计算区间[left, right]的二进制前缀分解
    std::vector<std::string> decompose_interval(uint32_t left, uint32_t right) const {
        std::vector<std::string> prefixes;
        
        while (left <= right) {
            // 找到最大的2^k使得[left, left + 2^k - 1] ⊆ [left, right]
            int k = 0;
            while (left + (1ULL << (k + 1)) - 1 <= right && 
                   (left & ((1ULL << (k + 1)) - 1)) == 0 && 
                   k < 32) {
                k++;
            }
            
            // 生成对应的前缀
            int prefix_length = BIT_LENGTH - k;
            if (prefix_length > 0 && k < 32) {
                std::string prefix = to_binary_string(left >> k, prefix_length);
                prefix += std::string(k, '*'); // '*' 表示通配符
                prefixes.push_back(prefix);
            }
            
            if (k == 0) {
                left++;
            } else {
                left += (1ULL << k);
            }
            
            // 防止无限循环
            if (left == 0) break;
        }
        
        return prefixes;
    }
    
    // 检查两个前缀是否匹配
    bool prefixes_match(const std::string& prefix1, const std::string& prefix2) const {
        if (prefix1.length() != prefix2.length()) return false;
        
        for (size_t i = 0; i < prefix1.length(); i++) {
            char c1 = prefix1[i];
            char c2 = prefix2[i];
            
            // 如果任一字符是通配符，则匹配
            if (c1 == '*' || c2 == '*') continue;
            
            // 否则必须完全相等
            if (c1 != c2) return false;
        }
        
        return true;
    }
    
public:
    // 读取CSV格式的IP数据文件
    std::vector<IPData> read_csv_file(const std::string& filename) {
        std::vector<IPData> ip_data;
        std::ifstream file(filename);
        
        if (!file.is_open()) {
            std::cerr << "错误: 无法打开文件 " << filename << std::endl;
            return ip_data;
        }
        
        std::string line;
        bool first_line = true;
        
        while (std::getline(file, line)) {
            // 跳过CSV头部
            if (first_line) {
                first_line = false;
                continue;
            }
            
            // 跳过注释行和空行
            if (line.empty() || line[0] == '#') continue;
            
            // 解析CSV行: ip_address,organization,dataset_type
            std::istringstream iss(line);
            std::string ip_str, organization, dataset_type;
            
            if (std::getline(iss, ip_str, ',') &&
                std::getline(iss, organization, ',') &&
                std::getline(iss, dataset_type)) {
                
                try {
                    IPData data;
                    data.ip = ipv4_to_uint32(ip_str);
                    data.organization = organization;
                    data.dataset_type = dataset_type;
                    ip_data.push_back(data);
                } catch (const std::exception& e) {
                    std::cerr << "警告: 无法解析行: " << line << std::endl;
                }
            }
        }
        
        file.close();
        return ip_data;
    }
    
    // Receiver编码：为每个IP生成其邻域区间的前缀分解
    std::vector<std::string> encode_receiver_element(uint32_t ip, int delta) {
        // 计算邻域区间 [ip - δ, ip + δ]
        int64_t left_64 = std::max((int64_t)0, (int64_t)ip - (int64_t)delta);
        int64_t right_64 = std::min((int64_t)UINT32_MAX, (int64_t)ip + (int64_t)delta);
        
        uint32_t left = (uint32_t)left_64;
        uint32_t right = (uint32_t)right_64;
        
        return decompose_interval(left, right);
    }
    
    // Sender编码：生成通配符填充的前缀
    std::vector<std::string> encode_sender_element(uint32_t ip, int wildcard_bits) {
        std::vector<std::string> prefixes;
        std::string binary = to_binary_string(ip, BIT_LENGTH);
        
        // 生成从最具体到最通用的前缀
        // 例如：111000 -> 111000, 11100*, 1110**, 111***
        for (int wildcards = 0; wildcards <= wildcard_bits; wildcards++) {
            if (BIT_LENGTH - wildcards <= 0) break;
            
            std::string prefix = binary.substr(0, BIT_LENGTH - wildcards);
            prefix += std::string(wildcards, '*');
            prefixes.push_back(prefix);
        }
        
        return prefixes;
    }
    
    // 编码所有Receiver数据
    std::unordered_map<uint32_t, std::vector<std::string>> encode_receiver_data(
        const std::vector<IPData>& receiver_data, int delta) {
        
        std::cout << "\n=== 编码Receiver数据 (Delta=" << delta << ") ===" << std::endl;
        std::cout << "邻域半径δ: " << delta << std::endl;
        std::cout << "编码模式: 邻域区间前缀分解" << std::endl;
        
        std::unordered_map<uint32_t, std::vector<std::string>> encoded_data;
        int total_prefixes = 0;
        
        for (size_t i = 0; i < receiver_data.size(); i++) {
            uint32_t ip = receiver_data[i].ip;
            auto prefixes = encode_receiver_element(ip, delta);
            encoded_data[ip] = prefixes;
            total_prefixes += prefixes.size();
            
            if (i < 5) {  // 显示前5个示例
                std::cout << "IP " << ip << " (" << receiver_data[i].organization << ") -> " 
                          << prefixes.size() << " 个前缀:" << std::endl;
                for (size_t j = 0; j < std::min((size_t)3, prefixes.size()); j++) {
                    std::cout << "  " << prefixes[j] << std::endl;
                }
                if (prefixes.size() > 3) {
                    std::cout << "  ... (共" << prefixes.size() << "个)" << std::endl;
                }
            }
        }
        
        std::cout << "✓ 编码完成: " << receiver_data.size() << " 个IP -> " 
                  << total_prefixes << " 个前缀" << std::endl;
        std::cout << "✓ 平均每IP前缀数: " << (double)total_prefixes / receiver_data.size() << std::endl;
        
        return encoded_data;
    }
    
    // 编码所有Sender数据
    std::unordered_map<uint32_t, std::vector<std::string>> encode_sender_data(
        const std::vector<IPData>& sender_data, int delta) {
        
        // 获取对应delta的通配符位数
        int wildcard_bits = 0;
        for (const auto& config : delta_configs) {
            if (config.delta == delta) {
                wildcard_bits = config.wildcard_bits;
                break;
            }
        }
        
        std::cout << "\n=== 编码Sender数据 (Delta=" << delta << ") ===" << std::endl;
        std::cout << "通配符位数: " << wildcard_bits << " (log2(2*" << delta << "-1)+1)" << std::endl;
        std::cout << "编码模式: 通配符填充前缀" << std::endl;
        
        std::unordered_map<uint32_t, std::vector<std::string>> encoded_data;
        int total_prefixes = 0;
        
        for (size_t i = 0; i < sender_data.size(); i++) {
            uint32_t ip = sender_data[i].ip;
            auto prefixes = encode_sender_element(ip, wildcard_bits);
            encoded_data[ip] = prefixes;
            total_prefixes += prefixes.size();
            
            if (i < 5) {  // 显示前5个示例
                std::cout << "IP " << ip << " (" << sender_data[i].organization << ") -> " 
                          << prefixes.size() << " 个前缀:" << std::endl;
                for (const auto& prefix : prefixes) {
                    std::cout << "  " << prefix << std::endl;
                }
            }
        }
        
        std::cout << "✓ 编码完成: " << sender_data.size() << " 个IP -> " 
                  << total_prefixes << " 个前缀" << std::endl;
        std::cout << "✓ 平均每IP前缀数: " << (double)total_prefixes / sender_data.size() << std::endl;
        
        return encoded_data;
    }
    
    // 保存编码后的数据
    void save_encoded_data(
        const std::vector<IPData>& receiver_data,
        const std::vector<IPData>& sender_data,
        const std::unordered_map<uint32_t, std::vector<std::string>>& receiver_encoded,
        const std::unordered_map<uint32_t, std::vector<std::string>>& sender_encoded,
        int delta, const std::string& sender_size_exp) {
        
        std::string output_dir = "/home/luck/xzy/intPSI/APSI_Test/prefixdata";
        std::filesystem::create_directories(output_dir);
        
        // 构造文件名
        std::string receiver_file = output_dir + "/receiver_query_delta_" + std::to_string(delta) + ".txt";
        std::string sender_file = output_dir + "/sender_db_2e" + sender_size_exp + "_delta_" + std::to_string(delta) + ".txt";
        
        // 保存Receiver编码数据
        std::ofstream receiver_out(receiver_file);
        receiver_out << "# Receiver编码数据 (邻域区间前缀分解)\n";
        receiver_out << "# δ = " << delta << ", 邻域模式\n";
        receiver_out << "# 格式: IP -> 前缀列表\n\n";
        
        for (const auto& data : receiver_data) {
            uint32_t ip = data.ip;
            receiver_out << ip << " -> ";
            const auto& prefixes = receiver_encoded.at(ip);
            for (size_t i = 0; i < prefixes.size(); i++) {
                if (i > 0) receiver_out << ", ";
                receiver_out << prefixes[i];
            }
            receiver_out << "\n";
        }
        receiver_out.close();
        
        // 保存Sender编码数据
        std::ofstream sender_out(sender_file);
        sender_out << "# Sender编码数据 (通配符填充前缀)\n";
        sender_out << "# δ = " << delta << ", 通配符模式\n";
        sender_out << "# 格式: IP -> 前缀列表\n\n";
        
        for (const auto& data : sender_data) {
            uint32_t ip = data.ip;
            sender_out << ip << " -> ";
            const auto& prefixes = sender_encoded.at(ip);
            for (size_t i = 0; i < prefixes.size(); i++) {
                if (i > 0) sender_out << ", ";
                sender_out << prefixes[i];
            }
            sender_out << "\n";
        }
        sender_out.close();
        
        // 保存APSI格式的数据（用于实际求交）
        save_apsi_format_data(receiver_encoded, sender_encoded, delta, sender_size_exp);
        
        std::cout << "✓ " << receiver_file << " - Receiver编码数据" << std::endl;
        std::cout << "✓ " << sender_file << " - Sender编码数据" << std::endl;
    }
    
    // 保存APSI格式的数据
    void save_apsi_format_data(
        const std::unordered_map<uint32_t, std::vector<std::string>>& receiver_encoded,
        const std::unordered_map<uint32_t, std::vector<std::string>>& sender_encoded,
        int delta, const std::string& sender_size_exp) {
        
        std::string output_dir = "/home/luck/xzy/intPSI/APSI_Test/prefixdata";
        
        // 收集所有唯一的前缀（用于APSI输入）
        std::unordered_set<std::string> all_receiver_prefixes;
        std::unordered_set<std::string> all_sender_prefixes;
        
        for (const auto& pair : receiver_encoded) {
            for (const auto& prefix : pair.second) {
                all_receiver_prefixes.insert(prefix);
            }
        }
        
        for (const auto& pair : sender_encoded) {
            for (const auto& prefix : pair.second) {
                all_sender_prefixes.insert(prefix);
            }
        }
        
        // 保存Receiver items (去重后的前缀集合)
        std::string receiver_items_file = output_dir + "/receiver_items_delta_" + std::to_string(delta) + ".txt";
        std::ofstream receiver_items(receiver_items_file);
        receiver_items << "# APSI格式Receiver数据 (唯一前缀集合)\n";
        receiver_items << "# Delta = " << delta << ", 总计 " << all_receiver_prefixes.size() << " 个唯一前缀\n\n";
        
        for (const auto& prefix : all_receiver_prefixes) {
            receiver_items << prefix << "\n";
        }
        receiver_items.close();
        
        // 保存Sender items (去重后的前缀集合)
        std::string sender_items_file = output_dir + "/sender_items_2e" + sender_size_exp + "_delta_" + std::to_string(delta) + ".txt";
        std::ofstream sender_items(sender_items_file);
        sender_items << "# APSI格式Sender数据 (唯一前缀集合)\n";
        sender_items << "# Delta = " << delta << ", 总计 " << all_sender_prefixes.size() << " 个唯一前缀\n\n";
        
        for (const auto& prefix : all_sender_prefixes) {
            sender_items << prefix << "\n";
        }
        sender_items.close();
        
        std::cout << "✓ " << receiver_items_file << " - APSI格式Receiver数据" << std::endl;
        std::cout << "✓ " << sender_items_file << " - APSI格式Sender数据" << std::endl;
        std::cout << "✓ 去重后Receiver前缀数: " << all_receiver_prefixes.size() << std::endl;
        std::cout << "✓ 去重后Sender前缀数: " << all_sender_prefixes.size() << std::endl;
    }
    
    // 验证编码正确性 - 正确的逻辑
    void verify_encoding(
        const std::vector<IPData>& receiver_data,
        const std::vector<IPData>& sender_data,
        const std::unordered_map<uint32_t, std::vector<std::string>>& receiver_encoded,
        const std::unordered_map<uint32_t, std::vector<std::string>>& sender_encoded,
        const std::vector<IPData>& intersection_data,
        int delta) {
        
        std::cout << "\n=== 详细编码验证 (Delta=" << delta << ") ===" << std::endl;
        
        // 构建IP集合
        std::unordered_set<uint32_t> receiver_ips;
        std::unordered_set<uint32_t> sender_ips;
        
        for (const auto& data : receiver_data) {
            receiver_ips.insert(data.ip);
        }
        for (const auto& data : sender_data) {
            sender_ips.insert(data.ip);
        }
        
        std::cout << "原始数据统计:" << std::endl;
        std::cout << "  - Receiver IP数: " << receiver_ips.size() << std::endl;
        std::cout << "  - Sender IP数: " << sender_ips.size() << std::endl;
        
        // 找出所有应该匹配的receiver及其邻域内的sender
        int expected_matching_receivers = 0;
        std::vector<std::pair<uint32_t, std::vector<uint32_t>>> matching_pairs;
        
        for (uint32_t receiver_ip : receiver_ips) {
            std::vector<uint32_t> senders_in_neighborhood;
            
            for (uint32_t sender_ip : sender_ips) {
                if (std::abs((int64_t)receiver_ip - (int64_t)sender_ip) <= delta) {
                    senders_in_neighborhood.push_back(sender_ip);
                }
            }
            
            if (!senders_in_neighborhood.empty()) {
                expected_matching_receivers++;
                matching_pairs.emplace_back(receiver_ip, senders_in_neighborhood);
                
                if (expected_matching_receivers <= 5) {
                    std::cout << "期望匹配 " << expected_matching_receivers << ": "
                              << "R[" << receiver_ip << "] <-> " << senders_in_neighborhood.size() 
                              << "个sender" << std::endl;
                }
            }
        }
        
        std::cout << "期望有匹配的receiver总数: " << expected_matching_receivers << std::endl;
        
        // 详细验证前缀匹配 - 正确的逻辑
        int verified_receivers = 0;
        int total_prefix_matches = 0;
        
        std::cout << "\n验证前缀匹配:" << std::endl;
        
        for (const auto& pair : matching_pairs) {
            uint32_t receiver_ip = pair.first;
            const auto& neighbor_senders = pair.second;
            
            const auto& receiver_prefixes = receiver_encoded.at(receiver_ip);
            bool receiver_has_match = false;
            int receiver_prefix_matches = 0;
            
            // 关键：检查receiver的前缀集合与其邻域内sender的前缀集合是否有交集
            for (uint32_t sender_ip : neighbor_senders) {
                const auto& sender_prefixes = sender_encoded.at(sender_ip);
                
                // 检查前缀集合交集
                for (const auto& r_prefix : receiver_prefixes) {
                    for (const auto& s_prefix : sender_prefixes) {
                        if (prefixes_match(r_prefix, s_prefix)) {
                            receiver_has_match = true;
                            receiver_prefix_matches++;
                            total_prefix_matches++;
                            
                            if (verified_receivers < 3) {
                                std::cout << "  匹配前缀: R[" << receiver_ip << "] '" 
                                          << r_prefix << "' <-> S[" << sender_ip << "] '" 
                                          << s_prefix << "'" << std::endl;
                            }
                        }
                    }
                }
            }
            
            if (receiver_has_match) {
                verified_receivers++;
                
                if (verified_receivers <= 5) {
                    std::cout << "✓ R[" << receiver_ip << "] 有 " << receiver_prefix_matches 
                              << " 个前缀匹配" << std::endl;
                }
            } else {
                if (verified_receivers < 5) {
                    std::cout << "❌ R[" << receiver_ip << "] 无前缀匹配，但应该有匹配" << std::endl;
                    
                    // 详细分析为什么没有匹配
                    std::cout << "    分析: R前缀数=" << receiver_prefixes.size() 
                              << ", 邻域sender数=" << neighbor_senders.size() << std::endl;
                    
                    if (!receiver_prefixes.empty() && !neighbor_senders.empty()) {
                        uint32_t first_sender = neighbor_senders[0];
                        const auto& sender_prefixes = sender_encoded.at(first_sender);
                        
                        std::cout << "    R首个前缀: '" << receiver_prefixes[0] << "'" << std::endl;
                        std::cout << "    S首个前缀: '" << sender_prefixes[0] << "'" << std::endl;
                        std::cout << "    距离: " << std::abs((int64_t)receiver_ip - (int64_t)first_sender) << std::endl;
                        
                        // 检查为什么前缀不匹配
                        std::cout << "    前缀匹配检查:" << std::endl;
                        bool found_any_match = false;
                        for (const auto& r_prefix : receiver_prefixes) {
                            for (const auto& s_prefix : sender_prefixes) {
                                bool match = prefixes_match(r_prefix, s_prefix);
                                if (match) found_any_match = true;
                                std::cout << "      '" << r_prefix << "' vs '" << s_prefix 
                                          << "' = " << (match ? "匹配" : "不匹配") << std::endl;
                                if (found_any_match) break;
                            }
                            if (found_any_match) break;
                        }
                    }
                }
            }
        }
        
        std::cout << "\n=== 验证结果汇总 ===" << std::endl;
        std::cout << "期望有匹配的receiver数: " << expected_matching_receivers << std::endl;
        std::cout << "实际有前缀匹配的receiver数: " << verified_receivers << std::endl;
        std::cout << "总前缀匹配对数: " << total_prefix_matches << std::endl;
        std::cout << "匹配率: " << std::fixed << std::setprecision(2) 
                  << (100.0 * verified_receivers / expected_matching_receivers) << "%" << std::endl;
        
        if (verified_receivers == expected_matching_receivers) {
            std::cout << "✅ 编码验证完全成功！" << std::endl;
        } else if (verified_receivers == 0) {
            std::cout << "❌ 编码验证完全失败！需要检查编码算法" << std::endl;
        } else {
            std::cout << "⚠️ 编码验证部分成功，但有 " 
                      << (expected_matching_receivers - verified_receivers) 
                      << " 个receiver未通过前缀匹配" << std::endl;
        }
    }
    
    // 处理所有数据集
    void process_all_datasets() {
        std::string input_dir = "/home/luck/xzy/intPSI/APSI_Test/data";
        
        std::cout << "=== 多Delta IP数据编码器 ===" << std::endl;
        std::cout << "输入目录: " << input_dir << std::endl;
        std::cout << "Delta值: 10, 50, 250" << std::endl;
        std::cout << std::endl;
        
        // 读取receiver数据（所有delta共用）
        std::string receiver_file = input_dir + "/receiver_query.csv";
        auto receiver_data = read_csv_file(receiver_file);
        
        if (receiver_data.empty()) {
            std::cerr << "错误: 无法读取receiver数据！" << std::endl;
            return;
        }
        
        std::cout << "✓ 读取了 " << receiver_data.size() << " 个receiver IP" << std::endl;
        
        // 处理每个delta值
        std::vector<int> deltas = {10, 50, 250};
        std::vector<int> sender_sizes = {12, 14, 16, 18, 20, 22}; // 2^n的指数
        
        for (int delta : deltas) {
            std::cout << "\n=== 处理Delta=" << delta << "的数据集 ===" << std::endl;
            
            // 读取对应delta的交集数据
            std::string intersection_file = input_dir + "/intersection_delta_" + std::to_string(delta) + ".csv";
            auto intersection_data = read_csv_file(intersection_file);
            
            // 编码receiver数据（每个delta都需要重新编码）
            auto receiver_encoded = encode_receiver_data(receiver_data, delta);
            
            // 处理每个sender大小
            for (int size_exp : sender_sizes) {
                std::cout << "\n--- 处理Sender 2^" << size_exp << " ---" << std::endl;
                
                // 读取sender数据
                std::string sender_file = input_dir + "/sender_db_2e" + std::to_string(size_exp) + 
                                        "_delta_" + std::to_string(delta) + ".csv";
                auto sender_data = read_csv_file(sender_file);
                
                if (sender_data.empty()) {
                    std::cerr << "警告: 无法读取sender数据: " << sender_file << std::endl;
                    continue;
                }
                
                // 编码sender数据
                auto sender_encoded = encode_sender_data(sender_data, delta);
                
                // 保存编码数据
                save_encoded_data(receiver_data, sender_data, receiver_encoded, sender_encoded, 
                                delta, std::to_string(size_exp));
                
                // 验证编码
                verify_encoding(receiver_data, sender_data, receiver_encoded, sender_encoded, 
                              intersection_data, delta);
            }
        }
        
        std::cout << "\n=== 编码完成 ===" << std::endl;
        std::cout << "所有编码数据已保存到: /home/luck/xzy/intPSI/APSI_Test/prefixdata/" << std::endl;
    }
};

int main() {
    try {
        MultiDeltaPrefixEncoder encoder;
        encoder.process_all_datasets();
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
}