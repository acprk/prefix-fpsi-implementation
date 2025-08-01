#include <iostream>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <string>
#include <fstream>
#include <random>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <climits>

// 真实IP地址生成器
class RealisticIPGenerator {
private:
    std::mt19937 rng;
    
    // 将IP地址字符串转换为32位整数
    uint32_t ip_to_uint32(const std::string& ip) const {
        std::istringstream iss(ip);
        std::string octet;
        uint32_t result = 0;
        int shift = 24;
        
        while (std::getline(iss, octet, '.') && shift >= 0) {
            uint32_t val = std::stoul(octet);
            result |= (val << shift);
            shift -= 8;
        }
        return result;
    }
    
    // 将32位整数转换为IP地址字符串
    std::string uint32_to_ip(uint32_t ip) const {
        std::ostringstream oss;
        oss << ((ip >> 24) & 0xFF) << "."
            << ((ip >> 16) & 0xFF) << "."
            << ((ip >> 8) & 0xFF) << "."
            << (ip & 0xFF);
        return oss.str();
    }
    
public:
    RealisticIPGenerator(unsigned seed = 12345) : rng(seed) {}
    
    // 生成真实的网络段IP
    std::vector<uint32_t> generate_realistic_ips(size_t count) {
        std::unordered_set<uint32_t> unique_ips;
        
        // 更真实的IP分布，主要使用三位数段
        std::vector<std::pair<uint32_t, uint32_t>> network_ranges = {
            // 中国电信网络 (大量三位数IP)
            {ip_to_uint32("218.0.0.0"), ip_to_uint32("218.255.255.255")},
            {ip_to_uint32("222.0.0.0"), ip_to_uint32("222.255.255.255")},
            {ip_to_uint32("202.96.0.0"), ip_to_uint32("202.96.255.255")},
            {ip_to_uint32("203.0.0.0"), ip_to_uint32("203.255.255.255")},
            {ip_to_uint32("210.0.0.0"), ip_to_uint32("210.255.255.255")},
            {ip_to_uint32("211.0.0.0"), ip_to_uint32("211.255.255.255")},
            
            // 中国联通网络
            {ip_to_uint32("221.0.0.0"), ip_to_uint32("221.255.255.255")},
            {ip_to_uint32("125.0.0.0"), ip_to_uint32("125.255.255.255")},
            {ip_to_uint32("112.0.0.0"), ip_to_uint32("112.255.255.255")},
            {ip_to_uint32("123.0.0.0"), ip_to_uint32("123.255.255.255")},
            
            // 中国移动网络
            {ip_to_uint32("183.0.0.0"), ip_to_uint32("183.255.255.255")},
            {ip_to_uint32("120.0.0.0"), ip_to_uint32("120.255.255.255")},
            {ip_to_uint32("117.0.0.0"), ip_to_uint32("117.255.255.255")},
            
            // 海外运营商
            {ip_to_uint32("216.0.0.0"), ip_to_uint32("216.255.255.255")},  // 美国
            {ip_to_uint32("198.0.0.0"), ip_to_uint32("198.255.255.255")},  // 北美
            {ip_to_uint32("173.0.0.0"), ip_to_uint32("173.255.255.255")},  // 美国
            {ip_to_uint32("151.0.0.0"), ip_to_uint32("151.255.255.255")},  // 欧洲
            {ip_to_uint32("185.0.0.0"), ip_to_uint32("185.255.255.255")},  // 欧洲
            
            // 亚太地区
            {ip_to_uint32("150.0.0.0"), ip_to_uint32("150.255.255.255")},  // 日本
            {ip_to_uint32("133.0.0.0"), ip_to_uint32("133.255.255.255")},  // 日本
            {ip_to_uint32("118.0.0.0"), ip_to_uint32("118.255.255.255")},  // 韩国
            {ip_to_uint32("175.0.0.0"), ip_to_uint32("175.255.255.255")},  // 东南亚
            
            // CDN和云服务
            {ip_to_uint32("104.0.0.0"), ip_to_uint32("104.255.255.255")},  // Cloudflare
            {ip_to_uint32("162.0.0.0"), ip_to_uint32("162.255.255.255")},  // 各种云服务
            {ip_to_uint32("142.0.0.0"), ip_to_uint32("142.255.255.255")},  // 云服务
            {ip_to_uint32("199.0.0.0"), ip_to_uint32("199.255.255.255")},  // CDN
            
            // 教育网络
            {ip_to_uint32("166.111.0.0"), ip_to_uint32("166.111.255.255")},  // 清华
            {ip_to_uint32("202.120.0.0"), ip_to_uint32("202.120.255.255")},  // 上交
            {ip_to_uint32("219.0.0.0"), ip_to_uint32("219.255.255.255")},    // 教育网
            
            // 政府和机构
            {ip_to_uint32("159.0.0.0"), ip_to_uint32("159.255.255.255")},
            {ip_to_uint32("128.0.0.0"), ip_to_uint32("128.255.255.255")},
            {ip_to_uint32("129.0.0.0"), ip_to_uint32("129.255.255.255")},
            
            // 企业专线
            {ip_to_uint32("140.0.0.0"), ip_to_uint32("140.255.255.255")},
            {ip_to_uint32("144.0.0.0"), ip_to_uint32("144.255.255.255")},
            {ip_to_uint32("156.0.0.0"), ip_to_uint32("156.255.255.255")},
            
            // 少量内网IP (模拟企业出口)
            {ip_to_uint32("192.168.0.0"), ip_to_uint32("192.168.255.255")},
            {ip_to_uint32("172.16.0.0"), ip_to_uint32("172.31.255.255")},
            {ip_to_uint32("10.0.0.0"), ip_to_uint32("10.255.255.255")}
        };
        
        // 设置权重，让三位数IP更常见
        std::vector<double> weights = {
            // 电信 (高权重)
            25.0, 20.0, 15.0, 18.0, 16.0, 14.0,
            // 联通 (高权重) 
            20.0, 12.0, 10.0, 8.0,
            // 移动 (高权重)
            18.0, 15.0, 12.0,
            // 海外 (中等权重)
            8.0, 7.0, 6.0, 5.0, 6.0,
            // 亚太 (中等权重)
            5.0, 4.0, 4.0, 5.0,
            // CDN (中等权重)
            6.0, 5.0, 4.0, 5.0,
            // 教育网 (低权重)
            2.0, 2.0, 3.0,
            // 政府机构 (低权重)
            2.0, 2.0, 2.0,
            // 企业 (低权重)
            3.0, 3.0, 3.0,
            // 内网 (很低权重)
            1.0, 0.5, 0.5
        };
        
        // 计算累积权重
        std::vector<double> cumulative_weights;
        double total_weight = 0.0;
        for (double w : weights) {
            total_weight += w;
            cumulative_weights.push_back(total_weight);
        }
        
        std::uniform_real_distribution<double> weight_dist(0.0, total_weight);
        
        while (unique_ips.size() < count) {
            // 根据权重选择网络段
            double random_weight = weight_dist(rng);
            size_t range_idx = 0;
            for (size_t i = 0; i < cumulative_weights.size(); i++) {
                if (random_weight <= cumulative_weights[i]) {
                    range_idx = i;
                    break;
                }
            }
            
            auto& range = network_ranges[range_idx];
            
            // 在该网络段内生成随机IP
            std::uniform_int_distribution<uint32_t> ip_dist(range.first, range.second);
            uint32_t ip = ip_dist(rng);
            
            // 过滤掉一些特殊IP
            uint8_t last_octet = ip & 0xFF;
            if (last_octet != 0 && last_octet != 255) {  // 排除网络地址和广播地址
                unique_ips.insert(ip);
            }
        }
        
        std::vector<uint32_t> result(unique_ips.begin(), unique_ips.end());
        std::sort(result.begin(), result.end());
        return result;
    }
    
    // 生成指定IP的邻域IP
    uint32_t generate_neighbor_ip(uint32_t base_ip, int max_distance) {
        std::uniform_int_distribution<int> dis(-max_distance, max_distance);
        int offset = dis(rng);
        
        int64_t neighbor = (int64_t)base_ip + offset;
        neighbor = std::max((int64_t)0, std::min((int64_t)UINT32_MAX, neighbor));
        
        return (uint32_t)neighbor;
    }
    
    std::string get_ip_string(uint32_t ip) const {
        return uint32_to_ip(ip);
    }
    
    std::mt19937& get_rng() { return rng; }
};

// 前缀生成器
class PrefixGenerator {
private:
    int distance_threshold;
    static const int max_bit_length = 32;
    
    // 将整数转换为固定长度的二进制字符串
    std::string to_binary_string(uint32_t value, int length) const {
        std::string result(length, '0');
        for (int i = length - 1; i >= 0 && value > 0; i--) {
            result[i] = '0' + (value & 1);
            value >>= 1;
        }
        return result;
    }
    
    // 计算区间的二进制分解
    std::vector<std::string> decompose_interval(uint32_t left, uint32_t right) const {
        std::vector<std::string> prefixes;
        
        while (left <= right) {
            // 找到最大的2^k使得[left, left + 2^k - 1] ⊆ [left, right]
            int k = 0;
            while (left + (1U << (k + 1)) - 1 <= right && (left & ((1U << (k + 1)) - 1)) == 0) {
                k++;
            }
            
            // 生成对应的前缀
            int prefix_length = max_bit_length - k;
            if (prefix_length > 0 && k < 32) {
                std::string prefix = to_binary_string(left >> k, prefix_length);
                prefix += std::string(k, '*'); // '*' 表示通配符
                prefixes.push_back(prefix);
            }
            
            left += (1U << k);
        }
        
        return prefixes;
    }
    
public:
    PrefixGenerator(int d) : distance_threshold(d) {}
    
    // 生成整数x的距离邻域的前缀表示（Sender模式）
    std::vector<std::string> generate_neighborhood_prefixes(uint32_t x) const {
        int64_t left_64 = std::max((int64_t)0, (int64_t)x - (int64_t)distance_threshold);
        int64_t right_64 = std::min((int64_t)UINT32_MAX, (int64_t)x + (int64_t)distance_threshold);
        
        uint32_t left = (uint32_t)left_64;
        uint32_t right = (uint32_t)right_64;
        
        return decompose_interval(left, right);
    }
    
    // 生成单个整数的邻域前缀（Receiver模式） - 正确版本
    std::vector<std::string> generate_element_prefixes(uint32_t x) const {
        // Receiver模式：也应该生成邻域区间的前缀分解
        // 与Sender相同，都是对 [x-δ, x+δ] 进行前缀分解
        return generate_neighborhood_prefixes(x);
    }
};

// 数据生成和导出器
class DatasetGenerator {
private:
    RealisticIPGenerator* ip_gen;
    PrefixGenerator* prefix_gen;
    const int delta;
    
    // 检查两个IP向量之间的匹配数
    int count_matches_between_vectors(const std::vector<uint32_t>& senders, 
                                     const std::vector<uint32_t>& receivers) {
        int matches = 0;
        for (uint32_t s : senders) {
            for (uint32_t r : receivers) {
                if (std::abs((int64_t)s - (int64_t)r) <= delta) {
                    matches++;
                }
            }
        }
        return matches;
    }
    
public:
    DatasetGenerator(int d = 50) : delta(d) {
        ip_gen = new RealisticIPGenerator(42);  // 固定种子确保可重现
        prefix_gen = new PrefixGenerator(d);
    }
    
    ~DatasetGenerator() {
        delete ip_gen;
        delete prefix_gen;
    }
    
    // 生成符合要求的数据集 - 修正版本，确保精确100个匹配对
    void generate_datasets() {
        const size_t dataset_size = 1024; // 2^10
        const int target_matches = 100;
        
        std::cout << "=== 开始生成真实场景测试数据 ===" << std::endl;
        std::cout << "数据集大小: " << dataset_size << " (2^10)" << std::endl;
        std::cout << "距离阈值 δ: " << delta << std::endl;
        std::cout << "目标匹配数: " << target_matches << " 个精确匹配对" << std::endl;
        std::cout << std::endl;
        
        // 最多尝试10次生成
        int attempt = 0;
        int actual_matches = 0;
        std::vector<uint32_t> sender_ips, receiver_ips;
        
        while (attempt < 10 && actual_matches != target_matches) {
            attempt++;
            std::cout << "尝试第 " << attempt << " 次生成..." << std::endl;
            
            // 步骤1: 生成Sender数据集
            std::cout << "步骤1: 生成Sender数据集..." << std::endl;
            sender_ips = ip_gen->generate_realistic_ips(dataset_size);
            
            // 步骤2: 精确生成Receiver数据集
            std::cout << "步骤2: 精确生成Receiver数据集..." << std::endl;
            receiver_ips.clear();
            std::unordered_set<uint32_t> used_receiver_ips;
            
            // 2.1: 随机选择exactly target_matches个sender来生成匹配对
            std::vector<uint32_t> selected_senders = sender_ips;
            std::shuffle(selected_senders.begin(), selected_senders.end(), ip_gen->get_rng());
            selected_senders.resize(target_matches);
            
            std::cout << "  为 " << target_matches << " 个选定的Sender生成匹配的Receiver..." << std::endl;
            
            // 为每个选定的sender生成一个邻域内的receiver
            for (uint32_t sender_ip : selected_senders) {
                uint32_t receiver_ip;
                int attempts = 0;
                
                do {
                    receiver_ip = ip_gen->generate_neighbor_ip(sender_ip, delta);
                    attempts++;
                } while (used_receiver_ips.count(receiver_ip) > 0 && attempts < 1000);
                
                if (attempts < 1000) {
                    receiver_ips.push_back(receiver_ip);
                    used_receiver_ips.insert(receiver_ip);
                }
            }
            
            std::cout << "  实际生成邻域IP: " << receiver_ips.size() << " 个" << std::endl;
            
            // 2.2: 生成其余的非匹配receiver IP
            size_t remaining_count = dataset_size - receiver_ips.size();
            std::cout << "  生成其余 " << remaining_count << " 个不匹配的Receiver..." << std::endl;
            
            // 生成足够多的候选IP
            std::vector<uint32_t> candidate_ips = ip_gen->generate_realistic_ips(remaining_count * 3);
            
            for (uint32_t candidate : candidate_ips) {
                if (receiver_ips.size() >= dataset_size) break;
                
                // 检查这个candidate是否与任何sender匹配
                bool has_match = false;
                for (uint32_t sender_ip : sender_ips) {
                    if (std::abs((int64_t)candidate - (int64_t)sender_ip) <= delta) {
                        has_match = true;
                        break;
                    }
                }
                
                // 只有不匹配且未使用的才加入
                if (!has_match && used_receiver_ips.count(candidate) == 0) {
                    receiver_ips.push_back(candidate);
                    used_receiver_ips.insert(candidate);
                }
            }
            
            // 如果还不够，用更安全的方法生成
            while (receiver_ips.size() < dataset_size) {
                uint32_t safe_ip = ip_gen->generate_realistic_ips(1)[0];
                
                // 确保距离足够远
                bool is_safe = true;
                for (uint32_t sender_ip : sender_ips) {
                    if (std::abs((int64_t)safe_ip - (int64_t)sender_ip) <= delta) {
                        is_safe = false;
                        break;
                    }
                }
                
                if (is_safe && used_receiver_ips.count(safe_ip) == 0) {
                    receiver_ips.push_back(safe_ip);
                    used_receiver_ips.insert(safe_ip);
                }
            }
            
            // 验证匹配数量
            actual_matches = count_matches_between_vectors(sender_ips, receiver_ips);
            std::cout << "验证: 实际匹配数量 = " << actual_matches << " 个" << std::endl;
            
            if (actual_matches == target_matches) {
                std::cout << "✅ 成功生成精确 " << target_matches << " 个匹配对！" << std::endl;
                break;
            } else {
                std::cout << "❌ 匹配数不符合要求，重新生成..." << std::endl;
            }
        }
        
        if (actual_matches != target_matches) {
            std::cout << "⚠️  警告: 尝试 " << attempt << " 次后仍无法生成精确匹配数，当前匹配数: " << actual_matches << std::endl;
        }
        
        // 排序数据集
        std::sort(sender_ips.begin(), sender_ips.end());
        std::sort(receiver_ips.begin(), receiver_ips.end());
        
        std::cout << std::endl;
        
        // 步骤3: 生成前缀数据
        std::cout << "步骤3: 生成前缀数据..." << std::endl;
        auto sender_prefixes = generate_sender_prefixes(sender_ips);
        auto receiver_prefixes = generate_receiver_prefixes(receiver_ips);
        
        // 步骤4: 导出数据到文件
        std::cout << "步骤4: 导出数据到文件..." << std::endl;
        export_to_files(sender_ips, receiver_ips, sender_prefixes, receiver_prefixes);
        
        // 输出统计信息
        print_statistics(sender_ips, receiver_ips, sender_prefixes, receiver_prefixes, actual_matches);
    }
    
    // 统计匹配数量
    int count_matches(const std::vector<uint32_t>& sender_ips, 
                     const std::vector<uint32_t>& receiver_ips) {
        return count_matches_between_vectors(sender_ips, receiver_ips);
    }
    
    // 生成Sender前缀数据
    std::unordered_map<uint32_t, std::vector<std::string>> generate_sender_prefixes(
        const std::vector<uint32_t>& sender_ips) {
        
        std::unordered_map<uint32_t, std::vector<std::string>> prefix_map;
        
        for (uint32_t ip : sender_ips) {
            prefix_map[ip] = prefix_gen->generate_neighborhood_prefixes(ip);
        }
        
        return prefix_map;
    }
    
    // 生成Receiver前缀数据
    std::unordered_map<uint32_t, std::vector<std::string>> generate_receiver_prefixes(
        const std::vector<uint32_t>& receiver_ips) {
        
        std::unordered_map<uint32_t, std::vector<std::string>> prefix_map;
        
        for (uint32_t ip : receiver_ips) {
            prefix_map[ip] = prefix_gen->generate_element_prefixes(ip);
        }
        
        return prefix_map;
    }
    
    // 导出数据到文件
    void export_to_files(const std::vector<uint32_t>& sender_ips,
                        const std::vector<uint32_t>& receiver_ips,
                        const std::unordered_map<uint32_t, std::vector<std::string>>& sender_prefixes,
                        const std::unordered_map<uint32_t, std::vector<std::string>>& receiver_prefixes) {
        
        // 文件1: Receiver原始IP数据
        std::ofstream receiver_ip_file("receiver_ip_data.txt");
        receiver_ip_file << "# Receiver IP地址原始数据 (2^10 = " << receiver_ips.size() << " 个)\n";
        receiver_ip_file << "# 格式: 序号, IP地址, 32位整数值, 十六进制\n";
        receiver_ip_file << "# 距离阈值 δ = " << delta << "\n\n";
        
        for (size_t i = 0; i < receiver_ips.size(); i++) {
            uint32_t ip = receiver_ips[i];
            receiver_ip_file << std::setw(4) << (i + 1) << ", "
                           << std::setw(15) << ip_gen->get_ip_string(ip) << ", "
                           << std::setw(10) << ip << ", "
                           << "0x" << std::hex << std::uppercase << ip << std::dec << "\n";
        }
        receiver_ip_file.close();
        
        // 文件2: Receiver前缀数据
        std::ofstream receiver_prefix_file("receiver_prefix_data.txt");
        receiver_prefix_file << "# Receiver前缀数据 (δ=" << delta << ", Receiver模式)\n";
        receiver_prefix_file << "# 每个IP的邻域区间 [IP-" << delta << ", IP+" << delta << "] 的前缀分解\n";
        receiver_prefix_file << "# 格式: IP地址 (32位整数) -> 邻域前缀列表\n\n";
        
        for (uint32_t ip : receiver_ips) {
            receiver_prefix_file << ip_gen->get_ip_string(ip) << " (" << ip << ") -> "
                               << receiver_prefixes.at(ip).size() << " 个邻域前缀:\n";
            
            // 显示邻域区间信息
            int64_t left_64 = std::max((int64_t)0, (int64_t)ip - (int64_t)delta);
            int64_t right_64 = std::min((int64_t)UINT32_MAX, (int64_t)ip + (int64_t)delta);
            receiver_prefix_file << "  邻域区间: [" << left_64 << ", " << right_64 << "] (共" 
                               << (right_64 - left_64 + 1) << "个数值)\n";
            
            const auto& prefixes = receiver_prefixes.at(ip);
            for (size_t i = 0; i < prefixes.size(); i++) {
                receiver_prefix_file << "  " << std::setw(2) << (i + 1) << ". " << prefixes[i] << "\n";
            }
            receiver_prefix_file << "\n";
        }
        receiver_prefix_file.close();
        
        // 文件3: Sender原始IP数据
        std::ofstream sender_ip_file("sender_ip_data.txt");
        sender_ip_file << "# Sender IP地址原始数据 (2^10 = " << sender_ips.size() << " 个)\n";
        sender_ip_file << "# 格式: 序号, IP地址, 32位整数值, 十六进制\n";
        sender_ip_file << "# 距离阈值 δ = " << delta << "\n\n";
        
        for (size_t i = 0; i < sender_ips.size(); i++) {
            uint32_t ip = sender_ips[i];
            sender_ip_file << std::setw(4) << (i + 1) << ", "
                         << std::setw(15) << ip_gen->get_ip_string(ip) << ", "
                         << std::setw(10) << ip << ", "
                         << "0x" << std::hex << std::uppercase << ip << std::dec << "\n";
        }
        sender_ip_file.close();
        
        // 文件4: Sender前缀数据
        std::ofstream sender_prefix_file("sender_prefix_data.txt");
        sender_prefix_file << "# Sender前缀数据 (δ=" << delta << ", Sender模式)\n";
        sender_prefix_file << "# 每个IP生成其邻域 [IP-" << delta << ", IP+" << delta << "] 的前缀分解\n";
        sender_prefix_file << "# 格式: IP地址 (32位整数) -> 邻域前缀列表\n\n";
        
        for (uint32_t ip : sender_ips) {
            sender_prefix_file << ip_gen->get_ip_string(ip) << " (" << ip << ") -> "
                             << sender_prefixes.at(ip).size() << " 个邻域前缀:\n";
            
            const auto& prefixes = sender_prefixes.at(ip);
            for (size_t i = 0; i < prefixes.size(); i++) {
                sender_prefix_file << "  " << std::setw(2) << (i + 1) << ". " << prefixes[i] << "\n";
            }
            sender_prefix_file << "\n";
        }
        sender_prefix_file.close();
    }
    
    // 输出统计信息
    void print_statistics(const std::vector<uint32_t>& sender_ips,
                         const std::vector<uint32_t>& receiver_ips,
                         const std::unordered_map<uint32_t, std::vector<std::string>>& sender_prefixes,
                         const std::unordered_map<uint32_t, std::vector<std::string>>& receiver_prefixes,
                         int actual_matches) {
        
        std::cout << "=== 数据生成统计信息 ===" << std::endl;
        
        std::cout << "\nSender数据集:" << std::endl;
        std::cout << "  IP数量: " << sender_ips.size() << std::endl;
        std::cout << "  IP范围: [" << ip_gen->get_ip_string(sender_ips.front()) 
                  << " (" << sender_ips.front() << "), " 
                  << ip_gen->get_ip_string(sender_ips.back()) 
                  << " (" << sender_ips.back() << ")]" << std::endl;
        
        // 计算Sender前缀统计
        int total_sender_prefixes = 0;
        int min_sender_prefixes = INT_MAX, max_sender_prefixes = 0;
        for (const auto& pair : sender_prefixes) {
            int count = pair.second.size();
            total_sender_prefixes += count;
            min_sender_prefixes = std::min(min_sender_prefixes, count);
            max_sender_prefixes = std::max(max_sender_prefixes, count);
        }
        std::cout << "  总前缀数: " << total_sender_prefixes << std::endl;
        std::cout << "  平均每IP前缀数: " << (double)total_sender_prefixes / sender_ips.size() << std::endl;
        std::cout << "  前缀数范围: [" << min_sender_prefixes << ", " << max_sender_prefixes << "]" << std::endl;
        
        std::cout << "\nReceiver数据集:" << std::endl;
        std::cout << "  IP数量: " << receiver_ips.size() << std::endl;
        std::cout << "  IP范围: [" << ip_gen->get_ip_string(receiver_ips.front()) 
                  << " (" << receiver_ips.front() << "), " 
                  << ip_gen->get_ip_string(receiver_ips.back()) 
                  << " (" << receiver_ips.back() << ")]" << std::endl;
        
        // 计算Receiver前缀统计
        int total_receiver_prefixes = 0;
        for (const auto& pair : receiver_prefixes) {
            total_receiver_prefixes += pair.second.size();
        }
        std::cout << "  总前缀数: " << total_receiver_prefixes << std::endl;
        std::cout << "  平均每IP前缀数: " << (double)total_receiver_prefixes / receiver_ips.size() << std::endl;
        std::cout << "  每IP前缀数范围: 与Sender相同 (邻域区间前缀分解)" << std::endl;
        
        std::cout << "\n匹配统计:" << std::endl;
        std::cout << "  目标匹配数: 100" << std::endl;
        std::cout << "  实际匹配数: " << actual_matches << std::endl;
        std::cout << "  匹配率: " << std::fixed << std::setprecision(5) 
                  << (100.0 * actual_matches / (sender_ips.size() * receiver_ips.size())) << "%" << std::endl;
        
        // 状态检查
        if (actual_matches == 100) {
            std::cout << "  ✅ 状态: 精确匹配成功！" << std::endl;
        } else {
            std::cout << "  ❌ 状态: 匹配数不符合预期" << std::endl;
        }
        
        std::cout << "\n文件导出完成:" << std::endl;
        std::cout << "  1. receiver_ip_data.txt - Receiver原始IP数据 (" << receiver_ips.size() << " 个IP)" << std::endl;
        std::cout << "  2. receiver_prefix_data.txt - Receiver前缀数据 (" << total_receiver_prefixes << " 个前缀)" << std::endl;
        std::cout << "  3. sender_ip_data.txt - Sender原始IP数据 (" << sender_ips.size() << " 个IP)" << std::endl;
        std::cout << "  4. sender_prefix_data.txt - Sender前缀数据 (" << total_sender_prefixes << " 个前缀)" << std::endl;
        
        std::cout << "\n数据特征:" << std::endl;
        std::cout << "  - 使用真实网络段分布 (主要为三位数IP地址)" << std::endl;
        std::cout << "  - 电信/联通/移动等运营商网络 (218.x.x.x, 221.x.x.x, 183.x.x.x等)" << std::endl;
        std::cout << "  - 海外网络段 (216.x.x.x, 173.x.x.x, 151.x.x.x等)" << std::endl;
        std::cout << "  - 亚太地区网络 (150.x.x.x, 175.x.x.x等)" << std::endl;
        std::cout << "  - CDN和云服务 (104.x.x.x, 162.x.x.x等)" << std::endl;
        std::cout << "  - 少量内网IP (企业出口NAT)" << std::endl;
        std::cout << "  - δ=50的邻域前缀分解 (Sender和Receiver都使用相同策略)" << std::endl;
        std::cout << "  - Sender: 邻域前缀模式 [xi-50, xi+50] 的区间分解" << std::endl;
        std::cout << "  - Receiver: 邻域前缀模式 [yj-50, yj+50] 的区间分解" << std::endl;
    }
};

int main() {
    std::cout << "=== 真实场景IP数据生成器 (修正版) ===" << std::endl;
    std::cout << "目标: 生成2^10个IP地址，δ=50，确保精确100个匹配对" << std::endl;
    std::cout << "输出: 4个TXT文件包含原始IP和前缀数据" << std::endl;
    std::cout << std::endl;
    
    DatasetGenerator generator(50);  // δ=50
    generator.generate_datasets();
    
    std::cout << "\n=== 生成完成 ===" << std::endl;
    std::cout << "请查看当前目录下的4个TXT文件:" << std::endl;
    std::cout << "- receiver_ip_data.txt" << std::endl;
    std::cout << "- receiver_prefix_data.txt" << std::endl; 
    std::cout << "- sender_ip_data.txt" << std::endl;
    std::cout << "- sender_prefix_data.txt" << std::endl;
    
    return 0;
}