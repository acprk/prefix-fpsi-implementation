#include <iostream>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <string>
#include <bitset>
#include <random>
#include <cassert>
#include <algorithm>
#include <chrono>
#include <climits>

// 需要添加pair的哈希函数支持
namespace std {
    template<>
    struct hash<std::pair<uint32_t, uint32_t>> {
        size_t operator()(const std::pair<uint32_t, uint32_t>& p) const {
            auto h1 = std::hash<uint32_t>{}(p.first);
            auto h2 = std::hash<uint32_t>{}(p.second);
            return h1 ^ (h2 << 1);
        }
    };
}

// 基础的前缀生成工具类
class PrefixGenerator {
private:
    int distance_threshold;
    int max_bit_length;
    
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
    std::vector<std::string> decompose_interval(int left, int right) const {
        std::vector<std::string> prefixes;
        
        while (left <= right) {
            // 找到最大的2^k使得[left, left + 2^k - 1] ⊆ [left, right]
            int k = 0;
            while (left + (1 << (k + 1)) - 1 <= right && (left & ((1 << (k + 1)) - 1)) == 0) {
                k++;
            }
            
            // 生成对应的前缀
            int prefix_length = max_bit_length - k;
            if (prefix_length > 0) {
                std::string prefix = to_binary_string(left >> k, prefix_length);
                prefix += std::string(k, '*'); // '*' 表示通配符
                prefixes.push_back(prefix);
            }
            
            left += (1 << k);
        }
        
        return prefixes;
    }
    
public:
    PrefixGenerator(int d, int max_value) : distance_threshold(d) {
        max_bit_length = 32; // 假设32位整数
        while ((1U << (max_bit_length - 1)) > max_value && max_bit_length > 1) {
            max_bit_length--;
        }
    }
    
    // 生成整数x的距离邻域的前缀表示
    std::vector<std::string> generate_neighborhood_prefixes(uint32_t x) const {
        int left = std::max(0, (int)x - distance_threshold);
        int right = x + distance_threshold;
        return decompose_interval(left, right);
    }
    
    // 生成单个整数的所有可能前缀（用于接收方）
    std::vector<std::string> generate_element_prefixes(uint32_t x) const {
        std::vector<std::string> prefixes;
        
        // 生成不同长度的前缀
        for (int prefix_len = 1; prefix_len <= max_bit_length; prefix_len++) {
            std::string binary = to_binary_string(x, max_bit_length);
            std::string prefix = binary.substr(0, prefix_len);
            
            // 添加通配符
            int wildcards = max_bit_length - prefix_len;
            prefix += std::string(wildcards, '*');
            prefixes.push_back(prefix);
        }
        
        return prefixes;
    }
    
    // 检查两个前缀是否兼容
    bool are_prefixes_compatible(const std::string& p1, const std::string& p2) const {
        if (p1.length() != p2.length()) return false;
        
        for (size_t i = 0; i < p1.length(); i++) {
            if (p1[i] != '*' && p2[i] != '*' && p1[i] != p2[i]) {
                return false;
            }
        }
        return true;
    }
};

// 简化的私有集合交集接口（实际实现需要复杂的密码学协议）
class PrivateSetIntersection {
public:
    // 模拟PSI协议的结果
    static std::vector<std::string> compute_intersection(
        const std::unordered_set<std::string>& set1,
        const std::unordered_set<std::string>& set2) {
        
        std::vector<std::string> intersection;
        for (const auto& item : set1) {
            if (set2.count(item)) {
                intersection.push_back(item);
            }
        }
        return intersection;
    }
};

// Sender类：拥有集合A，生成A的邻域前缀
class Sender {
private:
    std::vector<uint32_t> dataset_A;
    PrefixGenerator* prefix_gen;
    std::unordered_map<std::string, std::vector<uint32_t>> prefix_to_elements;
    
public:
    Sender(const std::vector<uint32_t>& A, int distance_threshold, int max_value) 
        : dataset_A(A) {
        prefix_gen = new PrefixGenerator(distance_threshold, max_value);
        build_prefix_mapping();
    }
    
    ~Sender() {
        delete prefix_gen;
    }
    
    // 构建前缀到元素的映射
    void build_prefix_mapping() {
        std::cout << "Sender: 构建前缀映射..." << std::endl;
        
        for (size_t i = 0; i < dataset_A.size(); i++) {
            uint32_t a = dataset_A[i];
            auto prefixes = prefix_gen->generate_neighborhood_prefixes(a);
            
            // 只对前几个元素显示详细信息
            if (i < 5) {
                std::cout << "元素 " << a << " 的邻域前缀: ";
                for (const auto& prefix : prefixes) {
                    std::cout << prefix << " ";
                }
                std::cout << std::endl;
            } else if (i == 5) {
                std::cout << "... (省略其余元素的详细信息)" << std::endl;
            }
            
            for (const auto& prefix : prefixes) {
                prefix_to_elements[prefix].push_back(a);
            }
        }
        
        std::cout << "Sender: 总共生成了 " << prefix_to_elements.size() << " 个不同的前缀" << std::endl;
    }
    
    // 获取所有前缀集合（用于PSI）
    std::unordered_set<std::string> get_prefix_set() const {
        std::unordered_set<std::string> prefix_set;
        for (const auto& pair : prefix_to_elements) {
            prefix_set.insert(pair.first);
        }
        return prefix_set;
    }
    
    // 根据PSI结果重构匹配的原始元素
    std::vector<uint32_t> reconstruct_elements(const std::vector<std::string>& common_prefixes) const {
        std::unordered_set<uint32_t> result_set;
        
        for (const std::string& prefix : common_prefixes) {
            auto it = prefix_to_elements.find(prefix);
            if (it != prefix_to_elements.end()) {
                for (uint32_t element : it->second) {
                    result_set.insert(element);
                }
            }
        }
        
        return std::vector<uint32_t>(result_set.begin(), result_set.end());
    }
    
    // 打印统计信息
    void print_statistics() const {
        std::cout << "=== Sender 统计信息 ===" << std::endl;
        std::cout << "数据集大小: " << dataset_A.size() << std::endl;
        std::cout << "生成的前缀数量: " << prefix_to_elements.size() << std::endl;
        
        // 计算平均每个前缀对应的元素数量
        int total_mappings = 0;
        for (const auto& pair : prefix_to_elements) {
            total_mappings += pair.second.size();
        }
        double avg_elements_per_prefix = (double)total_mappings / prefix_to_elements.size();
        std::cout << "平均每个前缀对应元素数: " << avg_elements_per_prefix << std::endl;
    }
};

// Receiver类：拥有集合B，生成B的元素前缀
class Receiver {
private:
    std::vector<uint32_t> dataset_B;
    PrefixGenerator* prefix_gen;
    std::unordered_map<std::string, std::vector<uint32_t>> prefix_to_elements;
    
public:
    Receiver(const std::vector<uint32_t>& B, int distance_threshold, int max_value) 
        : dataset_B(B) {
        prefix_gen = new PrefixGenerator(distance_threshold, max_value);
        build_prefix_mapping();
    }
    
    ~Receiver() {
        delete prefix_gen;
    }
    
    // 构建前缀到元素的映射
    void build_prefix_mapping() {
        std::cout << "Receiver: 构建前缀映射..." << std::endl;
        
        for (size_t i = 0; i < dataset_B.size(); i++) {
            uint32_t b = dataset_B[i];
            auto prefixes = prefix_gen->generate_element_prefixes(b);
            
            // 只对前几个元素显示详细信息
            if (i < 3) {
                std::cout << "元素 " << b << " 的前缀: ";
                // 只显示前几个前缀以避免输出过多
                for (size_t j = 0; j < std::min((size_t)5, prefixes.size()); j++) {
                    std::cout << prefixes[j] << " ";
                }
                if (prefixes.size() > 5) std::cout << "... ";
                std::cout << std::endl;
            } else if (i == 3) {
                std::cout << "... (省略其余元素的详细信息)" << std::endl;
            }
            
            for (const auto& prefix : prefixes) {
                prefix_to_elements[prefix].push_back(b);
            }
        }
        
        std::cout << "Receiver: 总共生成了 " << prefix_to_elements.size() << " 个不同的前缀" << std::endl;
    }
    
    // 获取所有前缀集合（用于PSI）
    std::unordered_set<std::string> get_prefix_set() const {
        std::unordered_set<std::string> prefix_set;
        for (const auto& pair : prefix_to_elements) {
            prefix_set.insert(pair.first);
        }
        return prefix_set;
    }
    
    // 根据PSI结果重构匹配的原始元素
    std::vector<uint32_t> reconstruct_elements(const std::vector<std::string>& common_prefixes) const {
        std::unordered_set<uint32_t> result_set;
        
        for (const std::string& prefix : common_prefixes) {
            auto it = prefix_to_elements.find(prefix);
            if (it != prefix_to_elements.end()) {
                for (uint32_t element : it->second) {
                    result_set.insert(element);
                }
            }
        }
        
        return std::vector<uint32_t>(result_set.begin(), result_set.end());
    }
    
    // 打印统计信息
    void print_statistics() const {
        std::cout << "=== Receiver 统计信息 ===" << std::endl;
        std::cout << "数据集大小: " << dataset_B.size() << std::endl;
        std::cout << "生成的前缀数量: " << prefix_to_elements.size() << std::endl;
        
        int total_mappings = 0;
        for (const auto& pair : prefix_to_elements) {
            total_mappings += pair.second.size();
        }
        double avg_elements_per_prefix = (double)total_mappings / prefix_to_elements.size();
        std::cout << "平均每个前缀对应元素数: " << avg_elements_per_prefix << std::endl;
    }
};

// 隐私距离感知集合交集协调器
class PrivateDistanceAwareIntersection {
private:
    Sender* sender;
    Receiver* receiver;
    int distance_threshold;
    
public:
    PrivateDistanceAwareIntersection(Sender* s, Receiver* r, int d) 
        : sender(s), receiver(r), distance_threshold(d) {}
    
    // 执行隐私计算协议
    std::vector<std::pair<uint32_t, uint32_t>> compute_intersection() {
        std::cout << "\n=== 开始隐私距离感知集合交集计算 ===" << std::endl;
        
        // 步骤1：获取双方的前缀集合
        auto sender_prefixes = sender->get_prefix_set();
        auto receiver_prefixes = receiver->get_prefix_set();
        
        std::cout << "Sender前缀数量: " << sender_prefixes.size() << std::endl;
        std::cout << "Receiver前缀数量: " << receiver_prefixes.size() << std::endl;
        
        // 步骤2：执行私有集合交集（PSI）
        std::cout << "执行PSI协议..." << std::endl;
        auto common_prefixes = PrivateSetIntersection::compute_intersection(
            sender_prefixes, receiver_prefixes);
        
        std::cout << "找到 " << common_prefixes.size() << " 个公共前缀" << std::endl;
        
        // 显示一些公共前缀示例
        if (!common_prefixes.empty()) {
            std::cout << "公共前缀示例: ";
            for (size_t i = 0; i < std::min((size_t)5, common_prefixes.size()); i++) {
                std::cout << common_prefixes[i] << " ";
            }
            if (common_prefixes.size() > 5) std::cout << "...";
            std::cout << std::endl;
        }
        
        // 步骤3：重构候选元素
        auto sender_candidates = sender->reconstruct_elements(common_prefixes);
        auto receiver_candidates = receiver->reconstruct_elements(common_prefixes);
        
        std::cout << "Sender候选元素数量: " << sender_candidates.size() << std::endl;
        std::cout << "Receiver候选元素数量: " << receiver_candidates.size() << std::endl;
        
        // 显示候选元素示例
        if (!sender_candidates.empty()) {
            std::cout << "Sender候选元素示例: ";
            for (size_t i = 0; i < std::min((size_t)5, sender_candidates.size()); i++) {
                std::cout << sender_candidates[i] << " ";
            }
            if (sender_candidates.size() > 5) std::cout << "...";
            std::cout << std::endl;
        }
        
        if (!receiver_candidates.empty()) {
            std::cout << "Receiver候选元素示例: ";
            for (size_t i = 0; i < std::min((size_t)5, receiver_candidates.size()); i++) {
                std::cout << receiver_candidates[i] << " ";
            }
            if (receiver_candidates.size() > 5) std::cout << "...";
            std::cout << std::endl;
        }
        
        // 步骤4：验证真实距离并生成最终结果
        std::vector<std::pair<uint32_t, uint32_t>> final_results;
        
        std::cout << "验证真实距离条件..." << std::endl;
        for (uint32_t a : sender_candidates) {
            for (uint32_t b : receiver_candidates) {
                if (abs((int)a - (int)b) <= distance_threshold) {
                    final_results.emplace_back(a, b);
                }
            }
        }
        
        std::cout << "最终找到 " << final_results.size() << " 个满足距离条件的配对" << std::endl;
        
        return final_results;
    }
    
    // 验证结果的正确性（与暴力方法对比）
    void verify_correctness(const std::vector<uint32_t>& A, const std::vector<uint32_t>& B) {
        std::cout << "\n=== 验证结果正确性 ===" << std::endl;
        
        // 隐私方法结果
        auto private_result = compute_intersection();
        
        // 暴力方法结果
        std::vector<std::pair<uint32_t, uint32_t>> brute_force_result;
        for (uint32_t a : A) {
            for (uint32_t b : B) {
                if (abs((int)a - (int)b) <= distance_threshold) {
                    brute_force_result.emplace_back(a, b);
                }
            }
        }
        
        // 比较结果
        std::sort(private_result.begin(), private_result.end());
        std::sort(brute_force_result.begin(), brute_force_result.end());
        
        bool is_correct = (private_result == brute_force_result);
        
        std::cout << "隐私方法找到配对数: " << private_result.size() << std::endl;
        std::cout << "暴力方法找到配对数: " << brute_force_result.size() << std::endl;
        std::cout << "结果正确性: " << (is_correct ? "✓ 正确" : "✗ 错误") << std::endl;
        
        // 输出具体的交集元素
        std::cout << "\n=== 距离感知交集结果详情 ===" << std::endl;
        if (brute_force_result.empty()) {
            std::cout << "没有找到满足距离条件的配对" << std::endl;
        } else {
            std::cout << "所有满足距离≤" << distance_threshold << "的配对:" << std::endl;
            for (size_t i = 0; i < brute_force_result.size(); i++) {
                auto [a, b] = brute_force_result[i];
                uint32_t distance = abs((int)a - (int)b);
                std::cout << "  " << (i+1) << ". (" << a << ", " << b << ") - 距离: " << distance << std::endl;
                
                // 如果结果太多，只显示前20个
                if (i >= 19 && brute_force_result.size() > 20) {
                    std::cout << "  ... (还有 " << (brute_force_result.size() - 20) << " 个配对)" << std::endl;
                    break;
                }
            }
        }
        
        // 如果结果不正确，进行详细差异分析
        if (!is_correct) {
            std::cout << "\n=== 详细差异分析 ===" << std::endl;
            
            // 找到隐私方法中有但暴力方法中没有的配对
            std::unordered_set<std::pair<uint32_t, uint32_t>, 
                std::hash<std::pair<uint32_t, uint32_t>>> brute_set(
                    brute_force_result.begin(), brute_force_result.end());
            
            std::vector<std::pair<uint32_t, uint32_t>> false_positives;
            for (const auto& pair : private_result) {
                if (brute_set.find(pair) == brute_set.end()) {
                    false_positives.push_back(pair);
                }
            }
            
            // 找到暴力方法中有但隐私方法中没有的配对
            std::unordered_set<std::pair<uint32_t, uint32_t>, 
                std::hash<std::pair<uint32_t, uint32_t>>> private_set(
                    private_result.begin(), private_result.end());
            
            std::vector<std::pair<uint32_t, uint32_t>> false_negatives;
            for (const auto& pair : brute_force_result) {
                if (private_set.find(pair) == private_set.end()) {
                    false_negatives.push_back(pair);
                }
            }
            
            if (!false_positives.empty()) {
                std::cout << "假阳性（隐私方法多找到的）: " << false_positives.size() << " 个" << std::endl;
                for (size_t i = 0; i < std::min((size_t)5, false_positives.size()); i++) {
                    auto [a, b] = false_positives[i];
                    uint32_t distance = abs((int)a - (int)b);
                    std::cout << "  (" << a << ", " << b << ") - 距离: " << distance << std::endl;
                }
                if (false_positives.size() > 5) {
                    std::cout << "  ... (还有 " << (false_positives.size() - 5) << " 个)" << std::endl;
                }
            }
            
            if (!false_negatives.empty()) {
                std::cout << "假阴性（隐私方法漏掉的）: " << false_negatives.size() << " 个" << std::endl;
                for (size_t i = 0; i < std::min((size_t)5, false_negatives.size()); i++) {
                    auto [a, b] = false_negatives[i];
                    uint32_t distance = abs((int)a - (int)b);
                    std::cout << "  (" << a << ", " << b << ") - 距离: " << distance << std::endl;
                }
                if (false_negatives.size() > 5) {
                    std::cout << "  ... (还有 " << (false_negatives.size() - 5) << " 个)" << std::endl;
                }
            }
        }
    }
};

// 生成随机数据的工具函数
std::vector<uint32_t> generate_random_data(size_t count, uint32_t max_value, unsigned seed) {
    std::vector<uint32_t> data;
    std::mt19937 gen(seed);
    std::uniform_int_distribution<uint32_t> dis(0, max_value);
    
    for (size_t i = 0; i < count; i++) {
        data.push_back(dis(gen));
    }
    
    // 排序并去重以获得更好的测试效果
    std::sort(data.begin(), data.end());
    data.erase(std::unique(data.begin(), data.end()), data.end());
    
    return data;
}

// 性能测试函数
void performance_test() {
    std::cout << "=== 大规模性能测试 ===" << std::endl;
    
    // 测试参数
    const size_t data_size = 1024;  // 2^10
    const uint32_t max_32bit_value = UINT32_MAX;  // 32位最大值
    const int distance_threshold = 10;
    
    std::cout << "测试参数:" << std::endl;
    std::cout << "数据集大小: " << data_size << " (2^10)" << std::endl;
    std::cout << "数值范围: 0 到 " << max_32bit_value << " (32位)" << std::endl;
    std::cout << "距离阈值: " << distance_threshold << std::endl;
    std::cout << std::endl;
    
    // 生成随机数据
    std::cout << "生成随机数据..." << std::endl;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    auto A = generate_random_data(data_size, max_32bit_value, 12345);
    auto B = generate_random_data(data_size, max_32bit_value, 54321);
    
    auto data_gen_time = std::chrono::high_resolution_clock::now();
    auto data_gen_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        data_gen_time - start_time).count();
    
    std::cout << "实际生成数据量 - A: " << A.size() << ", B: " << B.size() 
              << " (去重后)" << std::endl;
    std::cout << "数据生成时间: " << data_gen_duration << " ms" << std::endl;
    
    // 显示数据范围
    if (!A.empty() && !B.empty()) {
        std::cout << "A的数值范围: [" << A.front() << ", " << A.back() << "]" << std::endl;
        std::cout << "B的数值范围: [" << B.front() << ", " << B.back() << "]" << std::endl;
    }
    std::cout << std::endl;
    
    // 创建Sender和Receiver
    std::cout << "创建Sender和Receiver..." << std::endl;
    auto sender_start = std::chrono::high_resolution_clock::now();
    
    Sender sender(A, distance_threshold, max_32bit_value);
    
    auto sender_end = std::chrono::high_resolution_clock::now();
    auto sender_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        sender_end - sender_start).count();
    
    auto receiver_start = std::chrono::high_resolution_clock::now();
    
    Receiver receiver(B, distance_threshold, max_32bit_value);
    
    auto receiver_end = std::chrono::high_resolution_clock::now();
    auto receiver_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        receiver_end - receiver_start).count();
    
    std::cout << "Sender构建时间: " << sender_time << " ms" << std::endl;
    std::cout << "Receiver构建时间: " << receiver_time << " ms" << std::endl;
    std::cout << std::endl;
    
    // 打印统计信息
    sender.print_statistics();
    receiver.print_statistics();
    
    // 执行PSI和距离验证
    std::cout << "\n=== 执行隐私计算协议 ===" << std::endl;
    auto psi_start = std::chrono::high_resolution_clock::now();
    
    PrivateDistanceAwareIntersection pdai(&sender, &receiver, distance_threshold);
    auto results = pdai.compute_intersection();
    
    auto psi_end = std::chrono::high_resolution_clock::now();
    auto psi_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        psi_end - psi_start).count();
    
    std::cout << "PSI计算时间: " << psi_time << " ms" << std::endl;
    std::cout << "找到的匹配对数量: " << results.size() << std::endl;
    
    // 显示一些结果样例
    std::cout << "\n前10个匹配对样例:" << std::endl;
    for (size_t i = 0; i < std::min((size_t)10, results.size()); i++) {
        auto [a, b] = results[i];
        std::cout << "(" << a << ", " << b << ") - 距离: " << abs((int)a - (int)b) << std::endl;
    }
    
    // 总体性能统计
    auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        psi_end - start_time).count();
    
    std::cout << "\n=== 性能总结 ===" << std::endl;
    std::cout << "总执行时间: " << total_time << " ms" << std::endl;
    std::cout << "平均每个元素处理时间: " << (double)total_time / (A.size() + B.size()) << " ms" << std::endl;
    
    // 内存使用估算
    auto sender_prefixes = sender.get_prefix_set();
    auto receiver_prefixes = receiver.get_prefix_set();
    size_t total_prefixes = sender_prefixes.size() + receiver_prefixes.size();
    size_t estimated_memory = total_prefixes * 40; // 估算每个前缀约40字节
    
    std::cout << "估算内存使用: " << estimated_memory / 1024 / 1024 << " MB" << std::endl;
}

// 小规模验证测试
void small_scale_test() {
    std::cout << "=== 小规模验证测试 ===" << std::endl;
    
    // 小规模测试数据
    std::vector<uint32_t> A = {100, 150, 200, 250, 300};
    std::vector<uint32_t> B = {120, 180, 230, 280};
    int distance_threshold = 30;
    uint32_t max_value = 1000;
    
    std::cout << "测试参数:" << std::endl;
    std::cout << "集合A: ";
    for (uint32_t a : A) std::cout << a << " ";
    std::cout << std::endl;
    
    std::cout << "集合B: ";
    for (uint32_t b : B) std::cout << b << " ";
    std::cout << std::endl;
    
    std::cout << "距离阈值: " << distance_threshold << std::endl;
    std::cout << std::endl;
    
    // 创建Sender和Receiver
    Sender sender(A, distance_threshold, max_value);
    Receiver receiver(B, distance_threshold, max_value);
    
    // 打印统计信息
    sender.print_statistics();
    receiver.print_statistics();
    
    // 执行隐私计算并验证正确性
    PrivateDistanceAwareIntersection pdai(&sender, &receiver, distance_threshold);
    pdai.verify_correctness(A, B);
}

// 主函数和测试代码
int main() {
    std::cout << "=== 隐私计算下的前缀提取系统测试 ===" << std::endl;
    std::cout << std::endl;
    
    // 首先运行小规模验证测试
    small_scale_test();
    
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << std::endl;
    
    // 然后运行大规模性能测试
    performance_test();
    
    return 0;
}