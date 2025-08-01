// generate_datasets.cpp
// 生成Receiver和Sender的IP数据集，确保精确100个receiver有邻域覆盖

#include <iostream>
#include <vector>
#include <random>
#include <string>
#include <fstream>
#include <unordered_set>
#include <algorithm>
#include <iomanip>
#include <cmath>
#include <set>
#include <chrono>
#include <numeric>

class IPDatasetGenerator {
private:
    static constexpr int DELTA = 250;
    static constexpr size_t RECEIVER_SIZE = 16384;  // 2^10
    static constexpr size_t SENDER_SIZE = 65536;   // 2^16
    static constexpr int TARGET_MATCHES = 100;
    
    std::mt19937 rng;
    
    // 将32位整数转换为IP地址字符串（用于调试）
    std::string uint32_to_ip(uint32_t ip) const {
        return std::to_string((ip >> 24) & 0xFF) + "." +
               std::to_string((ip >> 16) & 0xFF) + "." +
               std::to_string((ip >> 8) & 0xFF) + "." +
               std::to_string(ip & 0xFF);
    }
    
public:
    IPDatasetGenerator(unsigned seed = 42) : rng(seed) {}
    
    // 生成真实的IP地址分布
    std::vector<uint32_t> generate_realistic_ips(size_t count) {
        std::unordered_set<uint32_t> unique_ips;
        
        // 真实的网络段分布
        std::vector<std::pair<uint32_t, uint32_t>> network_ranges = {
            // 中国电信网络 (三位数IP)
            {0xDA000000, 0xDAFFFFFF}, // 218.x.x.x
            {0xDE000000, 0xDEFFFFFF}, // 222.x.x.x
            {0xCA600000, 0xCA60FFFF}, // 202.96.x.x
            {0xCB000000, 0xCBFFFFFF}, // 203.x.x.x
            {0xD2000000, 0xD2FFFFFF}, // 210.x.x.x
            {0xD3000000, 0xD3FFFFFF}, // 211.x.x.x
            
            // 中国联通网络
            {0xDD000000, 0xDDFFFFFF}, // 221.x.x.x
            {0x7D000000, 0x7DFFFFFF}, // 125.x.x.x
            {0x70000000, 0x70FFFFFF}, // 112.x.x.x
            {0x7B000000, 0x7BFFFFFF}, // 123.x.x.x
            
            // 中国移动网络
            {0xB7000000, 0xB7FFFFFF}, // 183.x.x.x
            {0x78000000, 0x78FFFFFF}, // 120.x.x.x
            {0x75000000, 0x75FFFFFF}, // 117.x.x.x
            
            // 海外网络段
            {0xD8000000, 0xD8FFFFFF}, // 216.x.x.x (美国)
            {0xC6000000, 0xC6FFFFFF}, // 198.x.x.x (北美)
            {0xAD000000, 0xADFFFFFF}, // 173.x.x.x (美国)
            {0x97000000, 0x97FFFFFF}, // 151.x.x.x (欧洲)
            {0xB9000000, 0xB9FFFFFF}, // 185.x.x.x (欧洲)
            
            // 亚太地区
            {0x96000000, 0x96FFFFFF}, // 150.x.x.x (日本)
            {0x85000000, 0x85FFFFFF}, // 133.x.x.x (日本)
            {0x76000000, 0x76FFFFFF}, // 118.x.x.x (韩国)
            {0xAF000000, 0xAFFFFFFF}, // 175.x.x.x (东南亚)
            
            // CDN和云服务
            {0x68000000, 0x68FFFFFF}, // 104.x.x.x (Cloudflare)
            {0xA2000000, 0xA2FFFFFF}, // 162.x.x.x (云服务)
            {0x8E000000, 0x8EFFFFFF}, // 142.x.x.x (云服务)
            {0xC7000000, 0xC7FFFFFF}, // 199.x.x.x (CDN)
        };
        
        // 权重分布（让三位数IP更常见）
        std::vector<double> weights = {
            25.0, 20.0, 15.0, 18.0, 16.0, 14.0,  // 电信
            20.0, 12.0, 10.0, 8.0,               // 联通
            18.0, 15.0, 12.0,                    // 移动
            8.0, 7.0, 6.0, 5.0, 6.0,             // 海外
            5.0, 4.0, 4.0, 5.0,                  // 亚太
            6.0, 5.0, 4.0, 5.0                   // CDN
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
            
            // 过滤掉网络地址和广播地址
            uint8_t last_octet = ip & 0xFF;
            if (last_octet != 0 && last_octet != 255) {
                unique_ips.insert(ip);
            }
        }
        
        std::vector<uint32_t> result(unique_ips.begin(), unique_ips.end());
        std::sort(result.begin(), result.end());
        return result;
    }
    
    // 统计有多少个receiver在邻域内有sender（而不是统计匹配对数）
    int count_receivers_with_matches(const std::vector<uint32_t>& sender_ips, 
                                    const std::vector<uint32_t>& receiver_ips) {
        int receivers_with_matches = 0;
        
        for (uint32_t receiver_ip : receiver_ips) {
            int64_t receiver_min = (int64_t)receiver_ip - DELTA;
            int64_t receiver_max = (int64_t)receiver_ip + DELTA;
            
            // 检查是否有至少一个sender在该receiver的邻域内
            bool has_sender_in_neighborhood = false;
            for (uint32_t sender_ip : sender_ips) {
                if (sender_ip >= receiver_min && sender_ip <= receiver_max) {
                    has_sender_in_neighborhood = true;
                    break;
                }
            }
            
            if (has_sender_in_neighborhood) {
                receivers_with_matches++;
            }
        }
        
        return receivers_with_matches;
    }
    
    // 统计所有距离匹配对的总数（用于显示完整的匹配对数量）
    int count_matches(const std::vector<uint32_t>& senders, 
                     const std::vector<uint32_t>& receivers) {
        int matches = 0;
        for (uint32_t s : senders) {
            for (uint32_t r : receivers) {
                if (std::abs((int64_t)s - (int64_t)r) <= DELTA) {
                    matches++;
                }
            }
        }
        return matches;
    }
    
    // 生成完整的数据集 - 确保精确100个receiver有邻域覆盖
    std::pair<std::vector<uint32_t>, std::vector<uint32_t>> generate_datasets() {
        std::cout << "=== 开始生成IP数据集（确保100个receiver有邻域覆盖）===" << std::endl;
        std::cout << "策略: 先生成924个无邻域覆盖的receiver，再生成100个有邻域覆盖的receiver" << std::endl;
        std::cout << std::endl;
        
        std::vector<uint32_t> sender_ips, receiver_ips;
        
        // 步骤1: 生成Sender数据集
        std::cout << "步骤1: 生成Sender数据集..." << std::endl;
        sender_ips = generate_realistic_ips(SENDER_SIZE);
        std::sort(sender_ips.begin(), sender_ips.end());
        std::cout << "✓ 生成了 " << sender_ips.size() << " 个Sender IP" << std::endl;
        
        // 步骤2: 先生成924个receiver，确保邻域内无sender
        std::cout << "步骤2: 生成924个receiver，邻域内无sender..." << std::endl;
        std::unordered_set<uint32_t> used_receiver_ips;
        
        int non_matched_receivers = 0;
        int attempts = 0;
        const int max_attempts = 1000000;
        
        while (non_matched_receivers < (RECEIVER_SIZE - TARGET_MATCHES) && attempts < max_attempts) {
            attempts++;
            
            uint32_t candidate_receiver = generate_realistic_ips(1)[0];
            
            if (used_receiver_ips.count(candidate_receiver) > 0) continue;
            
            // 检查该receiver的邻域[candidate-50, candidate+50]内是否有sender
            int64_t receiver_min = (int64_t)candidate_receiver - DELTA;
            int64_t receiver_max = (int64_t)candidate_receiver + DELTA;
            
            bool has_sender_in_neighborhood = false;
            for (uint32_t sender_ip : sender_ips) {
                if (sender_ip >= receiver_min && sender_ip <= receiver_max) {
                    has_sender_in_neighborhood = true;
                    break;
                }
            }
            
            if (!has_sender_in_neighborhood) {
                receiver_ips.push_back(candidate_receiver);
                used_receiver_ips.insert(candidate_receiver);
                non_matched_receivers++;
                
                if (non_matched_receivers % 100 == 0) {
                    std::cout << "  无邻域覆盖receiver进度: " << non_matched_receivers 
                              << "/" << (RECEIVER_SIZE - TARGET_MATCHES) << std::endl;
                }
            }
        }
        
        std::cout << "✓ 生成了 " << non_matched_receivers << " 个邻域内无sender的receiver" << std::endl;
        
        // 步骤3: 精确生成100个receiver，确保邻域内有sender
        std::cout << "步骤3: 精确生成100个receiver，邻域内有sender..." << std::endl;
        
        int matched_receivers = 0;
        attempts = 0;
        
        while (matched_receivers < TARGET_MATCHES && attempts < max_attempts) {
            attempts++;
            
            uint32_t candidate_receiver = generate_realistic_ips(1)[0];
            
            if (used_receiver_ips.count(candidate_receiver) > 0) continue;
            
            // 检查该receiver的邻域[candidate-50, candidate+50]内是否有sender
            int64_t receiver_min = (int64_t)candidate_receiver - DELTA;
            int64_t receiver_max = (int64_t)candidate_receiver + DELTA;
            
            bool has_sender_in_neighborhood = false;
            for (uint32_t sender_ip : sender_ips) {
                if (sender_ip >= receiver_min && sender_ip <= receiver_max) {
                    has_sender_in_neighborhood = true;
                    break;
                }
            }
            
            if (has_sender_in_neighborhood) {
                receiver_ips.push_back(candidate_receiver);
                used_receiver_ips.insert(candidate_receiver);
                matched_receivers++;
                
                if (matched_receivers % 25 == 0) {
                    std::cout << "  有邻域覆盖receiver进度: " << matched_receivers << "/100" << std::endl;
                }
            }
        }
        
        std::cout << "✓ 生成了 " << matched_receivers << " 个邻域内有sender的receiver" << std::endl;
        std::cout << "✓ receiver总数: " << receiver_ips.size() << std::endl;
        
        if (receiver_ips.size() != RECEIVER_SIZE) {
            std::cout << "警告: receiver总数不正确，当前: " << receiver_ips.size() 
                      << ", 预期: " << RECEIVER_SIZE << std::endl;
        }
        
        if (matched_receivers != TARGET_MATCHES) {
            std::cout << "警告: 有邻域覆盖的receiver数量不正确，当前: " << matched_receivers 
                      << ", 预期: " << TARGET_MATCHES << std::endl;
        }
        
        // 最终验证并输出详细匹配信息
        int receivers_with_matches = count_receivers_with_matches(sender_ips, receiver_ips);
        int total_distance_pairs = count_matches(sender_ips, receiver_ips);
        
        std::cout << "\n=== 最终验证 ===" << std::endl;
        std::cout << "有匹配的receiver数量: " << receivers_with_matches << " (目标:100)" << std::endl;
        std::cout << "总距离匹配对数: " << total_distance_pairs << " (>=100, 因为可能多对一)" << std::endl;
        
        // 验证邻域策略并输出详细匹配信息
        int receivers_with_neighbors = 0;
        int receivers_without_neighbors = 0;
        
        std::cout << "\n=== 详细匹配信息 ===" << std::endl;
        std::cout << "匹配上的receiver及其邻域内的sender:" << std::endl;
        std::cout << std::string(80, '-') << std::endl;
        
        for (size_t i = 0; i < receiver_ips.size(); i++) {
            uint32_t receiver_ip = receiver_ips[i];
            int64_t receiver_min = (int64_t)receiver_ip - DELTA;
            int64_t receiver_max = (int64_t)receiver_ip + DELTA;
            
            // 查找该receiver邻域内的所有sender
            std::vector<uint32_t> senders_in_neighborhood;
            for (uint32_t sender_ip : sender_ips) {
                if (sender_ip >= receiver_min && sender_ip <= receiver_max) {
                    senders_in_neighborhood.push_back(sender_ip);
                }
            }
            
            if (!senders_in_neighborhood.empty()) {
                receivers_with_neighbors++;
                std::cout << "Receiver yj = " << receiver_ip 
                          << " (邻域: [" << receiver_min << ", " << receiver_max << "])" << std::endl;
                std::cout << "  邻域内的sender数量: " << senders_in_neighborhood.size() << std::endl;
                std::cout << "  邻域内的sender xi值: ";
                for (size_t j = 0; j < senders_in_neighborhood.size(); j++) {
                    if (j > 0) std::cout << ", ";
                    std::cout << senders_in_neighborhood[j];
                    // 计算实际距离
                    int64_t distance = std::abs((int64_t)receiver_ip - (int64_t)senders_in_neighborhood[j]);
                    std::cout << "(距离:" << distance << ")";
                }
                std::cout << std::endl;
                
                // 只显示前20个匹配的receiver，避免输出过多
                if (receivers_with_neighbors <= 20) {
                    std::cout << std::endl;
                } else if (receivers_with_neighbors == 21) {
                    std::cout << "  ... (还有更多匹配receiver，不全部显示)" << std::endl;
                    std::cout << std::endl;
                }
            } else {
                receivers_without_neighbors++;
            }
        }
        
        std::cout << std::string(80, '-') << std::endl;
        std::cout << "邻域验证结果:" << std::endl;
        std::cout << "  有sender邻居的receiver: " << receivers_with_neighbors << " (目标:100)" << std::endl;
        std::cout << "  无sender邻居的receiver: " << receivers_without_neighbors << " (目标:924)" << std::endl;
        
        // 计算总的距离匹配对数
        int total_distance_pairs_check = 0;
        for (size_t i = 0; i < receiver_ips.size(); i++) {
            uint32_t receiver_ip = receiver_ips[i];
            for (uint32_t sender_ip : sender_ips) {
                if (std::abs((int64_t)receiver_ip - (int64_t)sender_ip) <= DELTA) {
                    total_distance_pairs_check++;
                }
            }
        }
        
        std::cout << "  总距离匹配对数: " << total_distance_pairs_check << std::endl;
        
        if (receivers_with_neighbors == TARGET_MATCHES && 
            receivers_without_neighbors == (RECEIVER_SIZE - TARGET_MATCHES)) {
            std::cout << "✅ 邻域策略验证成功！" << std::endl;
            std::cout << "✅ 精确生成了100个receiver有sender邻居，924个receiver无sender邻居" << std::endl;
        } else {
            std::cout << "❌ 邻域策略验证失败" << std::endl;
        }
        
        // 排序数据集
        std::sort(sender_ips.begin(), sender_ips.end());
        std::sort(receiver_ips.begin(), receiver_ips.end());
        
        return {sender_ips, receiver_ips};
    }
    
    // 保存数据集到文件
    void save_datasets(const std::vector<uint32_t>& sender_ips,
                      const std::vector<uint32_t>& receiver_ips) {
        
        // 保存Sender数据集
        std::ofstream sender_file("data/sender_ips.txt");
        sender_file << "# Sender IP数据集 (2^16 = " << sender_ips.size() << " 个)\n";
        sender_file << "# 格式: 32位无符号整数\n";
        sender_file << "# 距离阈值δ = " << DELTA << "\n\n";
        
        for (size_t i = 0; i < sender_ips.size(); i++) {
            sender_file << sender_ips[i] << "\n";
        }
        sender_file.close();
        
        // 保存Receiver数据集
        std::ofstream receiver_file("data/receiver_ips.txt");
        receiver_file << "# Receiver IP数据集 (2^10 = " << receiver_ips.size() << " 个)\n";
        receiver_file << "# 格式: 32位无符号整数\n";
        receiver_file << "# 距离阈值δ = " << DELTA << "\n";
        receiver_file << "# 包含 " << TARGET_MATCHES << " 个有邻域覆盖的receiver\n\n";
        
        for (size_t i = 0; i < receiver_ips.size(); i++) {
            receiver_file << receiver_ips[i] << "\n";
        }
        receiver_file.close();
        
        // 保存详细统计信息
        std::ofstream stats_file("data/dataset_stats.txt");
        stats_file << "=== IP数据集统计信息 ===\n\n";
        
        stats_file << "Sender数据集:\n";
        stats_file << "  数量: " << sender_ips.size() << " (2^16)\n";
        stats_file << "  范围: [" << sender_ips.front() << ", " << sender_ips.back() << "]\n";
        stats_file << "  示例前10个: ";
        for (int i = 0; i < std::min(10, (int)sender_ips.size()); i++) {
            stats_file << sender_ips[i] << " ";
        }
        stats_file << "\n\n";
        
        stats_file << "Receiver数据集:\n";
        stats_file << "  数量: " << receiver_ips.size() << " (2^10)\n";
        stats_file << "  范围: [" << receiver_ips.front() << ", " << receiver_ips.back() << "]\n";
        stats_file << "  示例前10个: ";
        for (int i = 0; i < std::min(10, (int)receiver_ips.size()); i++) {
            stats_file << receiver_ips[i] << " ";
        }
        stats_file << "\n\n";
        
        // 使用正确的统计方法
        int receivers_with_matches_count = count_receivers_with_matches(sender_ips, receiver_ips);
        int total_distance_pairs_count = count_matches(sender_ips, receiver_ips);
        
        stats_file << "匹配统计:\n";
        stats_file << "  距离阈值δ: " << DELTA << "\n";
        stats_file << "  有匹配的receiver数量: " << receivers_with_matches_count << " (目标: 100)\n";
        stats_file << "  总距离匹配对数: " << total_distance_pairs_count << " (可能>100，因为多对一)\n";
        stats_file << "  邻域策略验证: " << (receivers_with_matches_count == 100 ? "成功" : "失败") << "\n";
        
        stats_file.close();
        
        // 保存详细匹配信息
        std::ofstream detailed_matches_file("data/detailed_matches.txt");
        detailed_matches_file << "# 详细匹配信息\n";
        detailed_matches_file << "# 格式: Receiver yj -> 邻域内的sender列表\n";
        detailed_matches_file << "# 距离阈值δ = " << DELTA << "\n\n";
        
        for (size_t i = 0; i < receiver_ips.size(); i++) {
            uint32_t receiver_ip = receiver_ips[i];
            int64_t receiver_min = (int64_t)receiver_ip - DELTA;
            int64_t receiver_max = (int64_t)receiver_ip + DELTA;
            
            // 查找该receiver邻域内的所有sender
            std::vector<uint32_t> senders_in_neighborhood;
            for (uint32_t sender_ip : sender_ips) {
                if (sender_ip >= receiver_min && sender_ip <= receiver_max) {
                    senders_in_neighborhood.push_back(sender_ip);
                }
            }
            
            if (!senders_in_neighborhood.empty()) {
                detailed_matches_file << "Receiver yj = " << receiver_ip 
                                     << " (邻域: [" << receiver_min << ", " << receiver_max << "])\n";
                detailed_matches_file << "  邻域内sender数量: " << senders_in_neighborhood.size() << "\n";
                detailed_matches_file << "  邻域内sender xi值: ";
                for (size_t j = 0; j < senders_in_neighborhood.size(); j++) {
                    if (j > 0) detailed_matches_file << ", ";
                    detailed_matches_file << senders_in_neighborhood[j];
                    int64_t distance = std::abs((int64_t)receiver_ip - (int64_t)senders_in_neighborhood[j]);
                    detailed_matches_file << "(距离:" << distance << ")";
                }
                detailed_matches_file << "\n\n";
            }
        }
        detailed_matches_file.close();
        
        std::cout << "\n=== 文件保存完成 ===" << std::endl;
        std::cout << "✓ data/sender_ips.txt - Sender数据集 (" << sender_ips.size() << " 个IP)" << std::endl;
        std::cout << "✓ data/receiver_ips.txt - Receiver数据集 (" << receiver_ips.size() << " 个IP)" << std::endl;
        std::cout << "✓ data/dataset_stats.txt - 详细统计信息" << std::endl;
        std::cout << "✓ data/detailed_matches.txt - 详细匹配信息" << std::endl;
    }
    
    // 输出样本数据用于验证
    void print_sample_data(const std::vector<uint32_t>& sender_ips,
                          const std::vector<uint32_t>& receiver_ips) {
        std::cout << "\n=== 样本数据预览 ===" << std::endl;
        
        std::cout << "Sender前10个IP:" << std::endl;
        for (int i = 0; i < std::min(10, (int)sender_ips.size()); i++) {
            std::cout << "  " << std::setw(2) << (i+1) << ". " 
                      << std::setw(10) << sender_ips[i] 
                      << " (" << uint32_to_ip(sender_ips[i]) << ")" << std::endl;
        }
        
        std::cout << "\nReceiver前10个IP:" << std::endl;
        for (int i = 0; i < std::min(10, (int)receiver_ips.size()); i++) {
            std::cout << "  " << std::setw(2) << (i+1) << ". " 
                      << std::setw(10) << receiver_ips[i] 
                      << " (" << uint32_to_ip(receiver_ips[i]) << ")" << std::endl;
        }
        
        // 检查一些匹配对
        std::cout << "\n检查匹配对示例:" << std::endl;
        int found_matches = 0;
        for (size_t i = 0; i < std::min((size_t)5, receiver_ips.size()) && found_matches < 5; i++) {
            for (size_t j = 0; j < sender_ips.size() && found_matches < 5; j++) {
                int64_t distance = std::abs((int64_t)receiver_ips[i] - (int64_t)sender_ips[j]);
                if (distance <= DELTA) {
                    std::cout << "  匹配 " << (found_matches + 1) << ": "
                              << "R[" << receiver_ips[i] << "] <-> S[" << sender_ips[j] << "] "
                              << "(距离=" << distance << ")" << std::endl;
                    found_matches++;
                }
            }
        }
    }
};

int main() {
    std::cout << "=== IP数据集生成器 ===" << std::endl;
    std::cout << "生成用于APSI距离隐私集合求交的测试数据" << std::endl;
    std::cout << std::endl;
    
    // 创建data目录（如果不存在）
    system("mkdir -p data");
    
    IPDatasetGenerator generator(42);  // 固定种子确保可重现
    
    // 生成数据集
    auto datasets = generator.generate_datasets();
    auto& sender_ips = datasets.first;
    auto& receiver_ips = datasets.second;
    
    // 输出样本数据
    generator.print_sample_data(sender_ips, receiver_ips);
    
    // 保存到文件
    generator.save_datasets(sender_ips, receiver_ips);
    
    std::cout << "\n=== 数据集生成完成 ===" << std::endl;
    std::cout << "请继续运行编码器来处理这些数据集。" << std::endl;
    
    return 0;
}