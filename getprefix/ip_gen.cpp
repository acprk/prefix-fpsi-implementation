#include <iostream>
#include <vector>
#include <random>
#include <string>
#include <sstream>
#include <iomanip>
#include <unordered_set>
#include <algorithm>
#include <fstream>

class IPDataGenerator {
private:
    std::mt19937 rng;
    
    // 将32位整数转换为IP地址字符串
    std::string uint32_to_ip(uint32_t ip) const {
        std::ostringstream oss;
        oss << ((ip >> 24) & 0xFF) << "."
            << ((ip >> 16) & 0xFF) << "."
            << ((ip >> 8) & 0xFF) << "."
            << (ip & 0xFF);
        return oss.str();
    }
    
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
    
public:
    IPDataGenerator(unsigned seed = std::random_device{}()) : rng(seed) {}
    
    // 生成完全随机的IP地址
    std::vector<uint32_t> generate_random_ips(size_t count) {
        std::vector<uint32_t> ips;
        std::uniform_int_distribution<uint32_t> dis(0, UINT32_MAX);
        
        std::unordered_set<uint32_t> unique_ips;
        while (unique_ips.size() < count) {
            uint32_t ip = dis(rng);
            unique_ips.insert(ip);
        }
        
        ips.assign(unique_ips.begin(), unique_ips.end());
        std::sort(ips.begin(), ips.end());
        return ips;
    }
    
    // 生成指定网络段内的IP地址
    std::vector<uint32_t> generate_subnet_ips(const std::string& subnet_base, 
                                             uint8_t prefix_length, 
                                             size_t count) {
        uint32_t base_ip = ip_to_uint32(subnet_base);
        uint32_t mask = (0xFFFFFFFF << (32 - prefix_length));
        uint32_t network = base_ip & mask;
        uint32_t host_bits = 32 - prefix_length;
        uint32_t max_hosts = (1U << host_bits) - 2; // 减去网络地址和广播地址
        
        if (count > max_hosts) {
            count = max_hosts;
        }
        
        std::vector<uint32_t> ips;
        std::unordered_set<uint32_t> unique_ips;
        std::uniform_int_distribution<uint32_t> dis(1, max_hosts);
        
        while (unique_ips.size() < count) {
            uint32_t host_part = dis(rng);
            uint32_t ip = network | host_part;
            unique_ips.insert(ip);
        }
        
        ips.assign(unique_ips.begin(), unique_ips.end());
        std::sort(ips.begin(), ips.end());
        return ips;
    }
    
    // 生成企业网络模拟数据（多个子网）
    std::vector<uint32_t> generate_enterprise_ips(size_t count) {
        std::vector<uint32_t> all_ips;
        
        // 常见的企业内网段
        std::vector<std::pair<std::string, uint8_t>> enterprise_subnets = {
            {"192.168.1.0", 24},    // 家庭/小型办公
            {"192.168.10.0", 24},   // 部门网络
            {"10.0.0.0", 16},       // 大型企业网络
            {"172.16.0.0", 20},     // 中型企业网络
            {"192.168.100.0", 24}   // 服务器网络
        };
        
        size_t ips_per_subnet = count / enterprise_subnets.size();
        size_t remaining = count % enterprise_subnets.size();
        
        for (size_t i = 0; i < enterprise_subnets.size(); i++) {
            size_t subnet_count = ips_per_subnet;
            if (i < remaining) subnet_count++;
            
            auto subnet_ips = generate_subnet_ips(
                enterprise_subnets[i].first, 
                enterprise_subnets[i].second, 
                subnet_count
            );
            
            all_ips.insert(all_ips.end(), subnet_ips.begin(), subnet_ips.end());
        }
        
        std::sort(all_ips.begin(), all_ips.end());
        return all_ips;
    }
    
    // 生成地理位置相关的IP地址（模拟）
    std::vector<uint32_t> generate_geographic_ips(const std::string& region, size_t count) {
        std::vector<std::pair<std::string, uint8_t>> regional_blocks;
        
        if (region == "Asia") {
            regional_blocks = {
                {"202.96.0.0", 16},     // 中国电信
                {"218.0.0.0", 15},      // 中国联通
                {"125.0.0.0", 14},      // 日本
                {"175.45.0.0", 16}      // 韩国
            };
        } else if (region == "North_America") {
            regional_blocks = {
                {"8.8.0.0", 16},        // Google DNS
                {"4.0.0.0", 14},        // Level 3
                {"24.0.0.0", 13},       // Comcast
                {"66.0.0.0", 15}        // AT&T
            };
        } else if (region == "Europe") {
            regional_blocks = {
                {"85.0.0.0", 12},       // 西欧
                {"95.0.0.0", 12},       // 东欧
                {"178.0.0.0", 12},      // 俄罗斯
                {"185.0.0.0", 12}       // 新分配
            };
        } else {
            // 默认使用随机IP
            return generate_random_ips(count);
        }
        
        std::vector<uint32_t> all_ips;
        size_t ips_per_block = count / regional_blocks.size();
        size_t remaining = count % regional_blocks.size();
        
        for (size_t i = 0; i < regional_blocks.size(); i++) {
            size_t block_count = ips_per_block;
            if (i < remaining) block_count++;
            
            auto block_ips = generate_subnet_ips(
                regional_blocks[i].first, 
                regional_blocks[i].second, 
                block_count
            );
            
            all_ips.insert(all_ips.end(), block_ips.begin(), block_ips.end());
        }
        
        std::sort(all_ips.begin(), all_ips.end());
        return all_ips;
    }
    
    // 生成具有邻近性的IP地址（用于测试距离感知算法）
    std::vector<uint32_t> generate_clustered_ips(size_t cluster_count, 
                                                 size_t ips_per_cluster, 
                                                 uint32_t cluster_spread = 1000) {
        std::vector<uint32_t> all_ips;
        std::uniform_int_distribution<uint32_t> cluster_center_dis(cluster_spread, UINT32_MAX - cluster_spread);
        std::uniform_int_distribution<uint32_t> offset_dis(0, cluster_spread);
        
        for (size_t i = 0; i < cluster_count; i++) {
            uint32_t center = cluster_center_dis(rng);
            
            std::unordered_set<uint32_t> cluster_ips;
            while (cluster_ips.size() < ips_per_cluster) {
                uint32_t offset = offset_dis(rng);
                uint32_t ip = center + offset;
                cluster_ips.insert(ip);
            }
            
            all_ips.insert(all_ips.end(), cluster_ips.begin(), cluster_ips.end());
        }
        
        std::sort(all_ips.begin(), all_ips.end());
        return all_ips;
    }
    
    // 打印IP地址列表（同时显示IP格式和32位整数）
    void print_ips(const std::vector<uint32_t>& ips, size_t max_display = 20) const {
        std::cout << "生成了 " << ips.size() << " 个IP地址:" << std::endl;
        std::cout << std::setw(4) << "序号" << std::setw(18) << "IP地址" 
                  << std::setw(15) << "32位整数" << std::setw(15) << "十六进制" << std::endl;
        std::cout << std::string(52, '-') << std::endl;
        
        for (size_t i = 0; i < std::min(max_display, ips.size()); i++) {
            std::cout << std::setw(4) << (i + 1) 
                      << std::setw(18) << uint32_to_ip(ips[i])
                      << std::setw(15) << ips[i]
                      << std::setw(15) << "0x" << std::hex << ips[i] << std::dec
                      << std::endl;
        }
        
        if (ips.size() > max_display) {
            std::cout << "... (还有 " << (ips.size() - max_display) << " 个IP地址)" << std::endl;
        }
        std::cout << std::endl;
    }
    
    // 保存IP地址到文件
    void save_to_file(const std::vector<uint32_t>& ips, 
                     const std::string& filename, 
                     bool include_ip_format = true) const {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "无法打开文件: " << filename << std::endl;
            return;
        }
        
        if (include_ip_format) {
            file << "# IP地址数据文件\n";
            file << "# 格式: IP地址, 32位整数\n";
            for (uint32_t ip : ips) {
                file << uint32_to_ip(ip) << ", " << ip << "\n";
            }
        } else {
            for (uint32_t ip : ips) {
                file << ip << "\n";
            }
        }
        
        file.close();
        std::cout << "已保存 " << ips.size() << " 个IP地址到 " << filename << std::endl;
    }
    
    // 分析IP地址分布
    void analyze_distribution(const std::vector<uint32_t>& ips) const {
        if (ips.empty()) return;
        
        std::cout << "=== IP地址分布分析 ===" << std::endl;
        std::cout << "总数量: " << ips.size() << std::endl;
        std::cout << "最小值: " << uint32_to_ip(ips.front()) << " (" << ips.front() << ")" << std::endl;
        std::cout << "最大值: " << uint32_to_ip(ips.back()) << " (" << ips.back() << ")" << std::endl;
        
        // 计算分布密度
        if (ips.size() > 1) {
            uint64_t total_range = (uint64_t)ips.back() - ips.front();
            double density = (double)ips.size() / total_range;
            std::cout << "分布范围: " << total_range << std::endl;
            std::cout << "分布密度: " << std::scientific << density << std::fixed << std::endl;
        }
        
        // 统计网络段分布
        std::unordered_map<uint32_t, int> subnet_count;
        for (uint32_t ip : ips) {
            uint32_t subnet = ip & 0xFFFFFF00; // /24网络
            subnet_count[subnet]++;
        }
        
        std::cout << "不同/24网络段数量: " << subnet_count.size() << std::endl;
        
        // 显示最大的几个网络段
        std::vector<std::pair<int, uint32_t>> sorted_subnets;
        for (const auto& pair : subnet_count) {
            sorted_subnets.emplace_back(pair.second, pair.first);
        }
        std::sort(sorted_subnets.rbegin(), sorted_subnets.rend());
        
        std::cout << "最大的网络段:" << std::endl;
        for (size_t i = 0; i < std::min((size_t)5, sorted_subnets.size()); i++) {
            uint32_t subnet = sorted_subnets[i].second;
            int count = sorted_subnets[i].first;
            std::cout << "  " << uint32_to_ip(subnet) << "/24: " << count << " 个IP" << std::endl;
        }
        std::cout << std::endl;
    }
};

// 测试和演示函数
void demonstrate_ip_generation() {
    std::cout << "=== 虚拟IP数据生成器演示 ===" << std::endl;
    
    IPDataGenerator generator(12345); // 固定种子以便重现结果
    
    // 1. 生成完全随机的IP地址
    std::cout << "\n1. 完全随机IP地址生成:" << std::endl;
    auto random_ips = generator.generate_random_ips(10);
    generator.print_ips(random_ips);
    generator.analyze_distribution(random_ips);
    
    // 2. 生成企业网络IP地址
    std::cout << "\n2. 企业网络IP地址生成:" << std::endl;
    auto enterprise_ips = generator.generate_enterprise_ips(15);
    generator.print_ips(enterprise_ips);
    generator.analyze_distribution(enterprise_ips);
    
    // 3. 生成地理区域IP地址
    std::cout << "\n3. 亚洲地区IP地址生成:" << std::endl;
    auto asia_ips = generator.generate_geographic_ips("Asia", 12);
    generator.print_ips(asia_ips);
    generator.analyze_distribution(asia_ips);
    
    // 4. 生成聚类IP地址（用于距离测试）
    std::cout << "\n4. 聚类IP地址生成（3个集群，每个5个IP）:" << std::endl;
    auto clustered_ips = generator.generate_clustered_ips(3, 5, 100);
    generator.print_ips(clustered_ips);
    generator.analyze_distribution(clustered_ips);
    
    // 5. 生成子网IP地址
    std::cout << "\n5. 特定子网IP地址生成 (192.168.1.0/24):" << std::endl;
    auto subnet_ips = generator.generate_subnet_ips("192.168.1.0", 24, 8);
    generator.print_ips(subnet_ips);
    generator.analyze_distribution(subnet_ips);
}

// 为原有的隐私计算系统生成测试数据
void generate_test_data_for_privacy_system() {
    std::cout << "\n=== 为隐私计算系统生成测试数据 ===" << std::endl;
    
    IPDataGenerator generator;
    
    // 生成两个具有一定重叠性的IP集合
    std::cout << "生成集合A（企业网络）:" << std::endl;
    auto set_A = generator.generate_enterprise_ips(50);
    generator.print_ips(set_A, 10);
    
    std::cout << "生成集合B（聚类网络，距离相近）:" << std::endl;
    auto set_B = generator.generate_clustered_ips(5, 10, 1000);
    generator.print_ips(set_B, 10);
    
    // 保存到文件
    generator.save_to_file(set_A, "ip_set_A.txt");
    generator.save_to_file(set_B, "ip_set_B.txt");
    
    std::cout << "已生成测试数据文件 ip_set_A.txt 和 ip_set_B.txt" << std::endl;
    std::cout << "这些文件包含的32位整数可直接用于隐私计算系统测试" << std::endl;
    
    // 显示一些统计信息用于距离分析
    std::cout << "\n=== 距离分析预览 ===" << std::endl;
    std::cout << "集合A范围: [" << set_A.front() << ", " << set_A.back() << "]" << std::endl;
    std::cout << "集合B范围: [" << set_B.front() << ", " << set_B.back() << "]" << std::endl;
    
    // 计算一些距离样例
    std::cout << "\n前5个A元素与前5个B元素的距离:" << std::endl;
    for (int i = 0; i < std::min(5, (int)std::min(set_A.size(), set_B.size())); i++) {
        uint64_t distance = std::abs((int64_t)set_A[i] - (int64_t)set_B[i]);
        std::cout << "distance(" << set_A[i] << ", " << set_B[i] << ") = " << distance << std::endl;
    }
}

int main() {
    // 运行演示
    demonstrate_ip_generation();
    
    std::cout << "\n" << std::string(60, '=') << std::endl;
    
    // 为隐私计算系统生成测试数据
    generate_test_data_for_privacy_system();
    
    return 0;
}