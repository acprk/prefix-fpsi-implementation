// encode_data.cpp
// 对IP数据进行前缀编码：Receiver前缀展开，Sender通配符填充

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

class PrefixEncoder {
private:
    static constexpr int DELTA = 50;
    static constexpr int BIT_LENGTH = 32;
    
    // 计算需要填充的通配符位数 = log2(2*δ-1) 向下取整 + 1
    static constexpr int WILDCARD_BITS = static_cast<int>(std::floor(std::log2(2 * DELTA - 1))) + 1;
    
    // 将整数转换为二进制字符串
    std::string to_binary_string(uint32_t value, int length) const {
        std::string result(length, '0');
        for (int i = length - 1; i >= 0 && value > 0; i--) {
            result[i] = '0' + (value & 1);
            value >>= 1;
        }
        return result;
    }
    
    // 将二进制字符串转换为整数（用于测试）
    uint32_t binary_string_to_uint32(const std::string& binary) const {
        uint32_t result = 0;
        for (char c : binary) {
            if (c == '0' || c == '1') {
                result = (result << 1) + (c - '0');
            }
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
    
public:
    // 读取IP数据文件
    std::vector<uint32_t> read_ip_file(const std::string& filename) {
        std::vector<uint32_t> ips;
        std::ifstream file(filename);
        
        if (!file.is_open()) {
            std::cerr << "错误: 无法打开文件 " << filename << std::endl;
            return ips;
        }
        
        std::string line;
        while (std::getline(file, line)) {
            // 跳过注释行和空行
            if (line.empty() || line[0] == '#') continue;
            
            try {
                uint32_t ip = std::stoul(line);
                ips.push_back(ip);
            } catch (const std::exception& e) {
                std::cerr << "警告: 无法解析行: " << line << std::endl;
            }
        }
        
        file.close();
        std::cout << "✓ 从 " << filename << " 读取了 " << ips.size() << " 个IP" << std::endl;
        return ips;
    }
    
    // Receiver编码：为每个IP生成其邻域区间的前缀分解
    std::vector<std::string> encode_receiver_element(uint32_t ip) {
        // 计算邻域区间 [ip - δ, ip + δ]
        int64_t left_64 = std::max((int64_t)0, (int64_t)ip - (int64_t)DELTA);
        int64_t right_64 = std::min((int64_t)UINT32_MAX, (int64_t)ip + (int64_t)DELTA);
        
        uint32_t left = (uint32_t)left_64;
        uint32_t right = (uint32_t)right_64;
        
        return decompose_interval(left, right);
    }
    
    // Sender编码：生成通配符填充的前缀
    std::vector<std::string> encode_sender_element(uint32_t ip) {
        std::vector<std::string> prefixes;
        std::string binary = to_binary_string(ip, BIT_LENGTH);
        
        // 生成从最具体到最通用的前缀
        // 例如：111000 -> 111000, 11100*, 1110**, 111***
        for (int wildcards = 0; wildcards <= WILDCARD_BITS; wildcards++) {
            if (BIT_LENGTH - wildcards <= 0) break;
            
            std::string prefix = binary.substr(0, BIT_LENGTH - wildcards);
            prefix += std::string(wildcards, '*');
            prefixes.push_back(prefix);
        }
        
        return prefixes;
    }
    
    // 编码所有Receiver数据
    std::unordered_map<uint32_t, std::vector<std::string>> encode_receiver_data(
        const std::vector<uint32_t>& receiver_ips) {
        
        std::cout << "\n=== 编码Receiver数据 ===" << std::endl;
        std::cout << "邻域半径δ: " << DELTA << std::endl;
        std::cout << "编码模式: 邻域区间前缀分解" << std::endl;
        
        std::unordered_map<uint32_t, std::vector<std::string>> encoded_data;
        int total_prefixes = 0;
        
        for (size_t i = 0; i < receiver_ips.size(); i++) {
            uint32_t ip = receiver_ips[i];
            auto prefixes = encode_receiver_element(ip);
            encoded_data[ip] = prefixes;
            total_prefixes += prefixes.size();
            
            if (i < 5) {  // 显示前5个示例
                std::cout << "IP " << ip << " (" << to_binary_string(ip, BIT_LENGTH) << ") -> " 
                          << prefixes.size() << " 个前缀:" << std::endl;
                for (size_t j = 0; j < std::min((size_t)3, prefixes.size()); j++) {
                    std::cout << "  " << prefixes[j] << std::endl;
                }
                if (prefixes.size() > 3) {
                    std::cout << "  ... (共" << prefixes.size() << "个)" << std::endl;
                }
            }
        }
        
        std::cout << "✓ 编码完成: " << receiver_ips.size() << " 个IP -> " 
                  << total_prefixes << " 个前缀" << std::endl;
        std::cout << "✓ 平均每IP前缀数: " << (double)total_prefixes / receiver_ips.size() << std::endl;
        
        return encoded_data;
    }
    
    // 编码所有Sender数据
    std::unordered_map<uint32_t, std::vector<std::string>> encode_sender_data(
        const std::vector<uint32_t>& sender_ips) {
        
        std::cout << "\n=== 编码Sender数据 ===" << std::endl;
        std::cout << "通配符位数: " << WILDCARD_BITS << " (log2(2*" << DELTA << "-1)+1)" << std::endl;
        std::cout << "编码模式: 通配符填充前缀" << std::endl;
        
        std::unordered_map<uint32_t, std::vector<std::string>> encoded_data;
        int total_prefixes = 0;
        
        for (size_t i = 0; i < sender_ips.size(); i++) {
            uint32_t ip = sender_ips[i];
            auto prefixes = encode_sender_element(ip);
            encoded_data[ip] = prefixes;
            total_prefixes += prefixes.size();
            
            if (i < 5) {  // 显示前5个示例
                std::cout << "IP " << ip << " (" << to_binary_string(ip, BIT_LENGTH) << ") -> " 
                          << prefixes.size() << " 个前缀:" << std::endl;
                for (const auto& prefix : prefixes) {
                    std::cout << "  " << prefix << std::endl;
                }
            }
        }
        
        std::cout << "✓ 编码完成: " << sender_ips.size() << " 个IP -> " 
                  << total_prefixes << " 个前缀" << std::endl;
        std::cout << "✓ 平均每IP前缀数: " << (double)total_prefixes / sender_ips.size() << std::endl;
        
        return encoded_data;
    }
    
    // 保存编码后的数据
    void save_encoded_data(
        const std::vector<uint32_t>& receiver_ips,
        const std::vector<uint32_t>& sender_ips,
        const std::unordered_map<uint32_t, std::vector<std::string>>& receiver_encoded,
        const std::unordered_map<uint32_t, std::vector<std::string>>& sender_encoded) {
        
        // 保存Receiver编码数据
        std::ofstream receiver_file("data/receiver_encoded.txt");
        receiver_file << "# Receiver编码数据 (邻域区间前缀分解)\n";
        receiver_file << "# δ = " << DELTA << ", 邻域模式\n";
        receiver_file << "# 格式: IP -> 前缀列表\n\n";
        
        for (uint32_t ip : receiver_ips) {
            receiver_file << ip << " -> ";
            const auto& prefixes = receiver_encoded.at(ip);
            for (size_t i = 0; i < prefixes.size(); i++) {
                if (i > 0) receiver_file << ", ";
                receiver_file << prefixes[i];
            }
            receiver_file << "\n";
        }
        receiver_file.close();
        
        // 保存Sender编码数据
        std::ofstream sender_file("data/sender_encoded.txt");
        sender_file << "# Sender编码数据 (通配符填充前缀)\n";
        sender_file << "# 通配符位数 = " << WILDCARD_BITS << " (log2(2*" << DELTA << "-1)+1)\n";
        sender_file << "# 格式: IP -> 前缀列表\n\n";
        
        for (uint32_t ip : sender_ips) {
            sender_file << ip << " -> ";
            const auto& prefixes = sender_encoded.at(ip);
            for (size_t i = 0; i < prefixes.size(); i++) {
                if (i > 0) sender_file << ", ";
                sender_file << prefixes[i];
            }
            sender_file << "\n";
        }
        sender_file.close();
        
        // 保存APSI格式的数据（用于实际求交）
        save_apsi_format_data(receiver_encoded, sender_encoded);
        
        std::cout << "\n=== 编码数据保存完成 ===" << std::endl;
        std::cout << "✓ data/receiver_encoded.txt - Receiver编码数据" << std::endl;
        std::cout << "✓ data/sender_encoded.txt - Sender编码数据" << std::endl;
        std::cout << "✓ data/receiver_items.txt - APSI格式Receiver数据" << std::endl;
        std::cout << "✓ data/sender_items.txt - APSI格式Sender数据" << std::endl;
    }
    
    // 保存APSI格式的数据
    void save_apsi_format_data(
        const std::unordered_map<uint32_t, std::vector<std::string>>& receiver_encoded,
        const std::unordered_map<uint32_t, std::vector<std::string>>& sender_encoded) {
        
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
        std::ofstream receiver_items_file("data/receiver_items.txt");
        receiver_items_file << "# APSI格式Receiver数据 (唯一前缀集合)\n";
        receiver_items_file << "# 总计 " << all_receiver_prefixes.size() << " 个唯一前缀\n\n";
        
        for (const auto& prefix : all_receiver_prefixes) {
            receiver_items_file << prefix << "\n";
        }
        receiver_items_file.close();
        
        // 保存Sender items (去重后的前缀集合)
        std::ofstream sender_items_file("data/sender_items.txt");
        sender_items_file << "# APSI格式Sender数据 (唯一前缀集合)\n";
        sender_items_file << "# 总计 " << all_sender_prefixes.size() << " 个唯一前缀\n\n";
        
        for (const auto& prefix : all_sender_prefixes) {
            sender_items_file << prefix << "\n";
        }
        sender_items_file.close();
        
        // 保存映射关系（前缀到原始IP的映射）
        save_mapping_data(receiver_encoded, sender_encoded);
        
        std::cout << "✓ 去重后Receiver前缀数: " << all_receiver_prefixes.size() << std::endl;
        std::cout << "✓ 去重后Sender前缀数: " << all_sender_prefixes.size() << std::endl;
    }
    
    // 保存映射关系数据
    void save_mapping_data(
        const std::unordered_map<uint32_t, std::vector<std::string>>& receiver_encoded,
        const std::unordered_map<uint32_t, std::vector<std::string>>& sender_encoded) {
        
        // 保存前缀到原始IP的反向映射
        std::ofstream receiver_mapping_file("data/receiver_prefix_to_ip.txt");
        receiver_mapping_file << "# Receiver前缀到原始IP的映射\n";
        receiver_mapping_file << "# 格式: 前缀 -> 原始IP\n\n";
        
        for (const auto& pair : receiver_encoded) {
            uint32_t original_ip = pair.first;
            for (const auto& prefix : pair.second) {
                receiver_mapping_file << prefix << " -> " << original_ip << "\n";
            }
        }
        receiver_mapping_file.close();
        
        std::ofstream sender_mapping_file("data/sender_prefix_to_ip.txt");
        sender_mapping_file << "# Sender前缀到原始IP的映射\n";
        sender_mapping_file << "# 格式: 前缀 -> 原始IP\n\n";
        
        for (const auto& pair : sender_encoded) {
            uint32_t original_ip = pair.first;
            for (const auto& prefix : pair.second) {
                sender_mapping_file << prefix << " -> " << original_ip << "\n";
            }
        }
        sender_mapping_file.close();
        
        std::cout << "✓ data/receiver_prefix_to_ip.txt - Receiver前缀映射" << std::endl;
        std::cout << "✓ data/sender_prefix_to_ip.txt - Sender前缀映射" << std::endl;
    }
    
    // 验证编码正确性 - 详细版本
    void verify_encoding(
        const std::vector<uint32_t>& receiver_ips,
        const std::vector<uint32_t>& sender_ips,
        const std::unordered_map<uint32_t, std::vector<std::string>>& receiver_encoded,
        const std::unordered_map<uint32_t, std::vector<std::string>>& sender_encoded) {
        
        std::cout << "\n=== 详细编码验证 ===" << std::endl;
        
        // 首先验证应该匹配的receiver数量
        int expected_matching_receivers = 0;
        std::vector<std::pair<uint32_t, std::vector<uint32_t>>> matching_pairs;
        
        // 找出所有应该匹配的receiver及其邻域内的sender
        for (uint32_t receiver_ip : receiver_ips) {
            std::vector<uint32_t> senders_in_neighborhood;
            
            for (uint32_t sender_ip : sender_ips) {
                if (std::abs((int64_t)receiver_ip - (int64_t)sender_ip) <= DELTA) {
                    senders_in_neighborhood.push_back(sender_ip);
                }
            }
            
            if (!senders_in_neighborhood.empty()) {
                expected_matching_receivers++;
                matching_pairs.emplace_back(receiver_ip, senders_in_neighborhood);
                
                if (expected_matching_receivers <= 5) {
                    std::cout << "期望匹配 " << expected_matching_receivers << ": "
                              << "R[" << receiver_ip << "] <-> S" << senders_in_neighborhood.size() 
                              << "个sender" << std::endl;
                }
            }
        }
        
        std::cout << "期望有匹配的receiver总数: " << expected_matching_receivers << std::endl;
        
        // 详细验证前缀匹配
        int verified_receivers = 0;
        int total_prefix_matches = 0;
        
        std::cout << "\n验证前缀匹配:" << std::endl;
        
        for (const auto& pair : matching_pairs) {
            uint32_t receiver_ip = pair.first;
            const auto& neighbor_senders = pair.second;
            
            const auto& receiver_prefixes = receiver_encoded.at(receiver_ip);
            bool receiver_has_match = false;
            int receiver_prefix_matches = 0;
            
            for (uint32_t sender_ip : neighbor_senders) {
                const auto& sender_prefixes = sender_encoded.at(sender_ip);
                
                // 检查是否有前缀匹配
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
            
            // 进一步诊断
            std::cout << "\n=== 编码诊断 ===" << std::endl;
            if (!receiver_ips.empty() && !sender_ips.empty()) {
                uint32_t sample_r = receiver_ips[0];
                uint32_t sample_s = sender_ips[0];
                
                std::cout << "样本R[" << sample_r << "]编码:" << std::endl;
                const auto& r_prefixes = receiver_encoded.at(sample_r);
                for (size_t i = 0; i < std::min((size_t)3, r_prefixes.size()); i++) {
                    std::cout << "  " << r_prefixes[i] << std::endl;
                }
                
                std::cout << "样本S[" << sample_s << "]编码:" << std::endl;
                const auto& s_prefixes = sender_encoded.at(sample_s);
                for (size_t i = 0; i < std::min((size_t)3, s_prefixes.size()); i++) {
                    std::cout << "  " << s_prefixes[i] << std::endl;
                }
                
                std::cout << "样本距离: " << std::abs((int64_t)sample_r - (int64_t)sample_s) << std::endl;
            }
        } else {
            std::cout << "⚠️ 编码验证部分成功，但有 " 
                      << (expected_matching_receivers - verified_receivers) 
                      << " 个receiver未通过前缀匹配" << std::endl;
        }
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
};

int main() {
    std::cout << "=== IP数据编码器 ===" << std::endl;
    std::cout << "对生成的IP数据进行前缀编码以用于APSI" << std::endl;
    std::cout << std::endl;
    
    PrefixEncoder encoder;
    
    // 读取生成的IP数据
    std::cout << "=== 读取IP数据 ===" << std::endl;
    auto receiver_ips = encoder.read_ip_file("data/receiver_ips.txt");
    auto sender_ips = encoder.read_ip_file("data/sender_ips.txt");
    
    if (receiver_ips.empty() || sender_ips.empty()) {
        std::cerr << "错误: 无法读取IP数据文件！请先运行数据生成器。" << std::endl;
        return 1;
    }
    
    // 编码数据
    auto receiver_encoded = encoder.encode_receiver_data(receiver_ips);
    auto sender_encoded = encoder.encode_sender_data(sender_ips);
    
    // 保存编码后的数据
    encoder.save_encoded_data(receiver_ips, sender_ips, receiver_encoded, sender_encoded);
    
    // 验证编码正确性
    encoder.verify_encoding(receiver_ips, sender_ips, receiver_encoded, sender_encoded);
    
    std::cout << "\n=== 编码完成 ===" << std::endl;
    std::cout << "请继续运行APSI求交程序来处理编码后的数据。" << std::endl;
    
    return 0;
}