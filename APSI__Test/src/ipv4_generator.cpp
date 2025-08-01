#include <iostream>
#include <fstream>
#include <vector>
#include <set>
#include <random>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <map>
#include <sys/stat.h>

struct IPRange {
    uint32_t network;
    uint32_t mask;
    int prefix_length;
    std::string organization;
    std::string description;
};

class RealWorldIPv4Generator {
private:
    std::random_device rd;
    std::mt19937 gen;
    
    const size_t RECEIVER_SIZE = 1 << 10;  // 2^10 = 1024
    const size_t INTERSECTION_SIZE = 100;
    const std::vector<size_t> SENDER_SIZES = {
        1 << 12,  // 2^12 = 4,096
        1 << 14,  // 2^14 = 16,384
        1 << 16,  // 2^16 = 65,536
        1 << 18,  // 2^18 = 262,144
        1 << 20,  // 2^20 = 1,048,576
        1 << 22   // 2^22 = 4,194,304
    };
    const std::vector<int> DELTA_VALUES = {10, 50, 250};
    
    std::vector<IPRange> real_ip_ranges;
    std::vector<uint32_t> receiver_data;
    std::vector<std::vector<uint32_t>> intersection_elements_by_delta;
    
public:
    RealWorldIPv4Generator() : gen(rd()) {
        initialize_real_ip_ranges();
    }
    
    void initialize_real_ip_ranges() {
        // MIT - 18.0.0.0/8
        add_ip_range("18.0.0.0", 8, "MIT", "Massachusetts Institute of Technology");
        
        // Stanford University - Historical ranges
        add_ip_range("171.64.0.0", 16, "Stanford", "Stanford University");
        add_ip_range("171.65.0.0", 16, "Stanford", "Stanford University");
        add_ip_range("171.66.0.0", 16, "Stanford", "Stanford University");
        add_ip_range("171.67.0.0", 16, "Stanford", "Stanford University");
        add_ip_range("128.12.0.0", 16, "Stanford", "Stanford University");
        
        // Google (sample ranges from real data)
        add_ip_range("8.8.4.0", 24, "Google", "Google DNS");
        add_ip_range("8.8.8.0", 24, "Google", "Google DNS");
        add_ip_range("64.233.160.0", 19, "Google", "Google Services");
        add_ip_range("66.102.0.0", 20, "Google", "Google Services");
        add_ip_range("66.249.64.0", 19, "Google", "GoogleBot");
        add_ip_range("72.14.192.0", 18, "Google", "Google Infrastructure");
        add_ip_range("74.125.0.0", 16, "Google", "Google Cloud");
        add_ip_range("142.250.0.0", 15, "Google", "Google Global");
        add_ip_range("172.217.0.0", 16, "Google", "Google Services");
        add_ip_range("173.194.0.0", 16, "Google", "Google Infrastructure");
        add_ip_range("216.58.192.0", 19, "Google", "Google Services");
        add_ip_range("216.239.32.0", 19, "Google", "Google Infrastructure");
        
        // Amazon AWS (common ranges)
        add_ip_range("3.0.0.0", 8, "Amazon", "Amazon Web Services");
        add_ip_range("13.32.0.0", 15, "Amazon", "AWS CloudFront");
        add_ip_range("13.224.0.0", 14, "Amazon", "AWS CloudFront");
        add_ip_range("52.0.0.0", 11, "Amazon", "AWS EC2");
        add_ip_range("54.0.0.0", 8, "Amazon", "AWS Global");
        add_ip_range("99.80.0.0", 13, "Amazon", "AWS CloudFront");
        add_ip_range("205.251.192.0", 19, "Amazon", "AWS Route53");
        
        // Microsoft (common ranges)
        add_ip_range("13.64.0.0", 11, "Microsoft", "Azure Cloud");
        add_ip_range("20.0.0.0", 8, "Microsoft", "Microsoft Azure");
        add_ip_range("40.64.0.0", 10, "Microsoft", "Azure Services");
        add_ip_range("52.96.0.0", 12, "Microsoft", "Office 365");
        add_ip_range("104.40.0.0", 13, "Microsoft", "Azure US");
        add_ip_range("131.253.0.0", 16, "Microsoft", "Microsoft Corporate");
        add_ip_range("157.54.0.0", 15, "Microsoft", "Microsoft Services");
        add_ip_range("191.232.0.0", 13, "Microsoft", "Azure Brazil");
        add_ip_range("207.46.0.0", 16, "Microsoft", "Microsoft Research");
        
        // University ranges (realistic examples)
        add_ip_range("128.32.0.0", 16, "UC Berkeley", "University of California Berkeley");
        add_ip_range("128.83.0.0", 16, "UC Davis", "University of California Davis");
        add_ip_range("128.97.0.0", 16, "UC San Diego", "University of California San Diego");
        add_ip_range("128.111.0.0", 16, "UCLA", "University of California Los Angeles");
        add_ip_range("128.143.0.0", 16, "CMU", "Carnegie Mellon University");
        add_ip_range("129.21.0.0", 16, "Caltech", "California Institute of Technology");
        add_ip_range("129.74.0.0", 16, "Cornell", "Cornell University");
        add_ip_range("129.105.0.0", 16, "Princeton", "Princeton University");
        add_ip_range("129.219.0.0", 16, "Yale", "Yale University");
        add_ip_range("129.237.0.0", 16, "Harvard", "Harvard University");
        add_ip_range("130.91.0.0", 16, "Columbia", "Columbia University");
        add_ip_range("140.247.0.0", 16, "NYU", "New York University");
        add_ip_range("198.32.0.0", 16, "UPenn", "University of Pennsylvania");
        
        // Enterprise ranges (common corporate blocks)
        add_ip_range("12.0.0.0", 8, "AT&T", "AT&T Corporate");
        add_ip_range("198.105.0.0", 16, "IBM", "IBM Corporate");
        add_ip_range("9.0.0.0", 8, "IBM", "IBM Global Network");
        add_ip_range("129.42.0.0", 16, "HP", "Hewlett Packard Enterprise");
        add_ip_range("15.0.0.0", 8, "HP", "HP Corporate");
        add_ip_range("156.56.0.0", 16, "Intel", "Intel Corporation");
        add_ip_range("134.134.0.0", 16, "Intel", "Intel Research");
        add_ip_range("4.0.0.0", 8, "Level3", "Level 3 Communications");
        add_ip_range("208.87.0.0", 16, "Cisco", "Cisco Systems");
        add_ip_range("144.254.0.0", 16, "Cisco", "Cisco Research");
        
        std::cout << "已初始化 " << real_ip_ranges.size() << " 个真实IP地址段" << std::endl;
    }
    
    void add_ip_range(const std::string& network_str, int prefix_length, 
                     const std::string& organization, const std::string& description) {
        IPRange range;
        range.network = ipv4_to_uint32(network_str);
        range.prefix_length = prefix_length;
        range.mask = (0xFFFFFFFF << (32 - prefix_length)) & 0xFFFFFFFF;
        range.organization = organization;
        range.description = description;
        real_ip_ranges.push_back(range);
    }
    
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
    
    std::string uint32_to_ipv4(uint32_t ip) {
        std::stringstream ss;
        ss << ((ip >> 24) & 0xFF) << "."
           << ((ip >> 16) & 0xFF) << "."
           << ((ip >> 8) & 0xFF) << "."
           << (ip & 0xFF);
        return ss.str();
    }
    
    bool create_directory(const std::string& path) {
        #ifdef _WIN32
            return _mkdir(path.c_str()) == 0 || errno == EEXIST;
        #else
            return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
        #endif
    }
    
    // 从真实IP段中生成地址
    uint32_t generate_ip_from_real_range(const IPRange& range) {
        uint32_t host_bits = 32 - range.prefix_length;
        uint32_t max_hosts = (1ULL << host_bits) - 1;
        
        std::uniform_int_distribution<uint32_t> host_dis(1, max_hosts - 1); // 避免网络地址和广播地址
        uint32_t host_part = host_dis(gen);
        
        return (range.network & range.mask) | host_part;
    }
    
    // 在指定IP地址周围delta范围内生成地址
    std::vector<uint32_t> generate_addresses_in_delta(uint32_t center_ip, int delta, size_t count) {
        std::vector<uint32_t> addresses;
        std::set<uint32_t> unique_addresses;
        
        std::uniform_int_distribution<int> delta_dis(-delta, delta);
        
        while (unique_addresses.size() < count) {
            uint32_t new_ip = center_ip;
            
            // 在每个字节上应用delta变化
            for (int byte_pos = 0; byte_pos < 4; ++byte_pos) {
                int current_byte = (new_ip >> (8 * byte_pos)) & 0xFF;
                int delta_change = delta_dis(gen);
                int new_byte = std::max(0, std::min(255, current_byte + delta_change));
                
                // 清除原来的字节并设置新值
                new_ip &= ~(0xFF << (8 * byte_pos));
                new_ip |= (new_byte << (8 * byte_pos));
            }
            
            unique_addresses.insert(new_ip);
        }
        
        addresses.assign(unique_addresses.begin(), unique_addresses.end());
        return addresses;
    }
    
    // 生成接收方数据（从多个真实IP段采样）
    void generate_receiver_data() {
        std::cout << "生成接收方数据 (2^10 = " << RECEIVER_SIZE << " 个真实IPv4地址)..." << std::endl;
        
        std::set<uint32_t> unique_ips;
        std::uniform_int_distribution<size_t> range_dis(0, real_ip_ranges.size() - 1);
        
        // 统计每个组织的地址数量
        std::map<std::string, int> org_count;
        
        while (unique_ips.size() < RECEIVER_SIZE) {
            size_t range_idx = range_dis(gen);
            const IPRange& range = real_ip_ranges[range_idx];
            
            uint32_t ip = generate_ip_from_real_range(range);
            if (unique_ips.insert(ip).second) {
                org_count[range.organization]++;
            }
        }
        
        receiver_data.assign(unique_ips.begin(), unique_ips.end());
        std::shuffle(receiver_data.begin(), receiver_data.end(), gen);
        
        std::cout << "接收方数据生成完成，实际大小: " << receiver_data.size() << std::endl;
        std::cout << "组织分布统计:" << std::endl;
        for (const auto& pair : org_count) {
            std::cout << "  " << pair.first << ": " << pair.second << " 个地址" << std::endl;
        }
        
        std::cout << "前5个IPv4地址示例: ";
        for (size_t i = 0; i < std::min<size_t>(5, receiver_data.size()); ++i) {
            std::cout << uint32_to_ipv4(receiver_data[i]) << " ";
        }
        std::cout << std::endl;
    }
    
    // 为每个delta值生成交集元素
    void generate_intersection_elements() {
        std::cout << "为不同delta值生成真实IP交集元素..." << std::endl;
        
        intersection_elements_by_delta.clear();
        intersection_elements_by_delta.resize(DELTA_VALUES.size());
        
        for (size_t delta_idx = 0; delta_idx < DELTA_VALUES.size(); ++delta_idx) {
            int delta = DELTA_VALUES[delta_idx];
            std::cout << "  生成delta=" << delta << "的交集元素..." << std::endl;
            
            // 从接收方数据中直接选择100个作为交集元素（这些元素在receiver中）
            std::vector<uint32_t> selected_receiver_elements;
            std::sample(receiver_data.begin(), receiver_data.end(), 
                       std::back_inserter(selected_receiver_elements), INTERSECTION_SIZE, gen);
            
            intersection_elements_by_delta[delta_idx] = selected_receiver_elements;
            
            std::cout << "    delta=" << delta << " 交集大小: " 
                      << intersection_elements_by_delta[delta_idx].size() << std::endl;
        }
    }
    
    // 生成发送方数据（从真实IP段生成）
    std::vector<uint32_t> generate_sender_data(size_t sender_size, int delta_idx) {
        std::cout << "生成发送方数据 (大小: " << sender_size 
                  << ", delta: " << DELTA_VALUES[delta_idx] << ")..." << std::endl;
        
        std::set<uint32_t> unique_ips;
        std::uniform_int_distribution<size_t> range_dis(0, real_ip_ranges.size() - 1);
        int delta = DELTA_VALUES[delta_idx];
        
        // 首先为每个交集元素（在receiver中）生成其在delta范围内的邻居地址（放入sender中）
        for (uint32_t receiver_ip : intersection_elements_by_delta[delta_idx]) {
            auto delta_addresses = generate_addresses_in_delta(receiver_ip, delta, 1);
            unique_ips.insert(delta_addresses[0]);
        }
        
        std::cout << "  已添加 " << unique_ips.size() << " 个交集对应的delta邻居地址到sender中" << std::endl;
        
        // 然后从真实IP段生成剩余地址
        std::map<std::string, int> org_count;
        while (unique_ips.size() < sender_size) {
            size_t range_idx = range_dis(gen);
            const IPRange& range = real_ip_ranges[range_idx];
            
            uint32_t ip = generate_ip_from_real_range(range);
            if (unique_ips.insert(ip).second) {
                org_count[range.organization]++;
            }
        }
        
        std::vector<uint32_t> sender_data(unique_ips.begin(), unique_ips.end());
        std::shuffle(sender_data.begin(), sender_data.end(), gen);
        
        std::cout << "发送方数据生成完成，实际大小: " << sender_data.size() << std::endl;
        std::cout << "主要组织分布 (Top 5):" << std::endl;
        
        // 按数量排序并显示前5个
        std::vector<std::pair<int, std::string>> sorted_orgs;
        for (const auto& pair : org_count) {
            sorted_orgs.push_back({pair.second, pair.first});
        }
        std::sort(sorted_orgs.rbegin(), sorted_orgs.rend());
        
        for (size_t i = 0; i < std::min<size_t>(5, sorted_orgs.size()); ++i) {
            std::cout << "  " << sorted_orgs[i].second << ": " << sorted_orgs[i].first << " 个地址" << std::endl;
        }
        
        return sender_data;
    }
    
    // 保存数据到CSV文件
    void save_ipv4_csv_file(const std::vector<uint32_t>& data, const std::string& filename, 
                           const std::string& dataset_type = "data") {
        std::ofstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("无法创建文件: " + filename);
        }
        
        // 写入CSV头部
        file << "ip_address,organization,dataset_type" << std::endl;
        
        // 写入数据
        for (uint32_t ip : data) {
            std::string ip_str = uint32_to_ipv4(ip);
            std::string org = find_organization_for_ip(ip);
            file << ip_str << "," << org << "," << dataset_type << std::endl;
        }
        
        file.close();
        std::cout << "CSV数据已保存到: " << filename << std::endl;
    }
    
    // 根据IP地址查找对应的组织
    std::string find_organization_for_ip(uint32_t ip) {
        for (const auto& range : real_ip_ranges) {
            if ((ip & range.mask) == (range.network & range.mask)) {
                return range.organization;
            }
        }
        return "Unknown";
    }
    
    // 验证交集：检查receiver中的交集元素是否在sender中有delta范围内的邻居
    size_t verify_intersection(const std::vector<uint32_t>& sender_data, 
                              const std::vector<uint32_t>& intersection_elements, int delta) {
        std::set<uint32_t> sender_set(sender_data.begin(), sender_data.end());
        
        size_t actual_intersection = 0;
        
        // 对于每个交集元素（在receiver中），检查sender中是否有其delta范围内的邻居
        for (uint32_t receiver_ip : intersection_elements) {
            bool found_neighbor = false;
            
            // 检查sender中是否有在delta范围内的地址
            for (uint32_t sender_ip : sender_set) {
                if (is_within_delta(receiver_ip, sender_ip, delta)) {
                    found_neighbor = true;
                    break;
                }
            }
            
            if (found_neighbor) {
                actual_intersection++;
            }
        }
        
        return actual_intersection;
    }
    
    // 检查两个IP地址是否在delta范围内
    bool is_within_delta(uint32_t ip1, uint32_t ip2, int delta) {
        for (int byte_pos = 0; byte_pos < 4; ++byte_pos) {
            int byte1 = (ip1 >> (8 * byte_pos)) & 0xFF;
            int byte2 = (ip2 >> (8 * byte_pos)) & 0xFF;
            
            if (std::abs(byte1 - byte2) > delta) {
                return false;
            }
        }
        return true;
    }
    
    // 生成所有数据集
    void generate_all_datasets() {
        std::cout << "=== 真实世界IPv4 APSI 数据集生成器 ===" << std::endl;
        std::cout << "参数配置:" << std::endl;
        std::cout << "- 接收方数据大小: 2^10 = " << RECEIVER_SIZE << std::endl;
        std::cout << "- 发送方数据大小: ";
        for (size_t size : SENDER_SIZES) {
            std::cout << size << " ";
        }
        std::cout << std::endl;
        std::cout << "- Delta值: ";
        for (int delta : DELTA_VALUES) {
            std::cout << delta << " ";
        }
        std::cout << std::endl;
        std::cout << "- 交集大小: " << INTERSECTION_SIZE << std::endl;
        std::cout << "- 数据来源: 真实的大学和企业IP地址段" << std::endl;
        std::cout << std::endl;
        
        // 生成接收方数据
        generate_receiver_data();
        
        // 生成交集元素
        generate_intersection_elements();
        
        // 创建指定的数据目录
        std::string base_dir = "/home/luck/xzy/intPSI/APSI_Test/data";
        create_directory(base_dir);
        
        // 保存接收方数据（只需保存一次）
        save_ipv4_csv_file(receiver_data, base_dir + "/receiver_query.csv", "receiver");
        
        // 为每个delta值和发送方大小组合生成数据
        int dataset_count = 0;
        for (size_t delta_idx = 0; delta_idx < DELTA_VALUES.size(); ++delta_idx) {
            int delta = DELTA_VALUES[delta_idx];
            
            // 保存当前delta的交集元素
            std::string intersection_filename = base_dir + "/intersection_delta_" + std::to_string(delta) + ".csv";
            save_ipv4_csv_file(intersection_elements_by_delta[delta_idx], intersection_filename, "intersection");
            
            for (size_t sender_size : SENDER_SIZES) {
                dataset_count++;
                std::cout << "\n--- 生成数据集 " << dataset_count << "/18 ---" << std::endl;
                
                // 生成发送方数据
                auto sender_data = generate_sender_data(sender_size, delta_idx);
                
                // 构造文件名
                std::string sender_filename = base_dir + "/sender_db_2e" + std::to_string((int)log2(sender_size)) + 
                                            "_delta_" + std::to_string(delta) + ".csv";
                
                // 保存发送方数据
                save_ipv4_csv_file(sender_data, sender_filename, "sender");
                
                // 验证交集
                size_t actual_intersection = verify_intersection(sender_data, intersection_elements_by_delta[delta_idx], delta);
                std::cout << "验证结果 - 期望交集: " << INTERSECTION_SIZE 
                          << ", 实际交集: " << actual_intersection;
                if (actual_intersection == INTERSECTION_SIZE) {
                    std::cout << " ✅" << std::endl;
                } else {
                    std::cout << " ❌" << std::endl;
                }
            }
        }
        
        // 生成统计信息文件
        generate_statistics_file(base_dir);
        
        std::cout << "\n=== 真实世界数据集生成完成 ===" << std::endl;
        std::cout << "生成了 " << dataset_count << " 个发送方数据集" << std::endl;
        std::cout << "所有CSV文件已保存到目录: " << base_dir << std::endl;
    }
    
private:
    void generate_statistics_file(const std::string& base_dir) {
        std::ofstream stats(base_dir + "/dataset_info.csv");
        stats << "metric,value,description" << std::endl;
        stats << "generation_time," << std::time(nullptr) << ",Unix timestamp" << std::endl;
        stats << "receiver_size," << RECEIVER_SIZE << ",Number of receiver IPs (2^10)" << std::endl;
        stats << "intersection_size," << INTERSECTION_SIZE << ",Number of intersection elements" << std::endl;
        stats << "data_source,real_world_ip_ranges,Source of IP addresses" << std::endl;
        stats << "total_sender_datasets," << SENDER_SIZES.size() * DELTA_VALUES.size() << ",Total sender datasets generated" << std::endl;
        stats.close();
        
        // 创建IP段信息CSV
        std::ofstream ranges_csv(base_dir + "/ip_ranges_info.csv");
        ranges_csv << "network,prefix_length,organization,description" << std::endl;
        for (const auto& range : real_ip_ranges) {
            ranges_csv << uint32_to_ipv4(range.network) << "/" << range.prefix_length << ","
                      << range.prefix_length << ","
                      << range.organization << ","
                      << range.description << std::endl;
        }
        ranges_csv.close();
        
        // 创建数据集列表CSV
        std::ofstream datasets_csv(base_dir + "/datasets_list.csv");
        datasets_csv << "dataset_id,filename,size_power,size_actual,delta,dataset_type" << std::endl;
        
        int dataset_id = 1;
        datasets_csv << dataset_id++ << ",receiver_query.csv,10," << RECEIVER_SIZE << ",N/A,receiver" << std::endl;
        
        for (int delta : DELTA_VALUES) {
            datasets_csv << dataset_id++ << ",intersection_delta_" << delta << ".csv,N/A," 
                        << INTERSECTION_SIZE << "," << delta << ",intersection" << std::endl;
        }
        
        for (int delta : DELTA_VALUES) {
            for (size_t sender_size : SENDER_SIZES) {
                int exp = (int)log2(sender_size);
                datasets_csv << dataset_id++ << ",sender_db_2e" << exp << "_delta_" << delta 
                            << ".csv," << exp << "," << sender_size << "," << delta << ",sender" << std::endl;
            }
        }
        datasets_csv.close();
        
        std::cout << "统计信息CSV文件已保存到:" << std::endl;
        std::cout << "- " << base_dir << "/dataset_info.csv" << std::endl;
        std::cout << "- " << base_dir << "/ip_ranges_info.csv" << std::endl;
        std::cout << "- " << base_dir << "/datasets_list.csv" << std::endl;
    }
};

int main() {
    try {
        RealWorldIPv4Generator generator;
        generator.generate_all_datasets();
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
}