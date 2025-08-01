#include <iostream>
#include <fstream>
#include <vector>
#include <set>
#include <random>
#include <iomanip>
#include <sstream>
#include <algorithm>

class APSIDataGenerator {
private:
    std::random_device rd;
    std::mt19937 gen;
    std::uniform_int_distribution<uint32_t> dis;
    
    const size_t RECEIVER_SIZE = 1 << 10;  // 2^10 = 1024
    const size_t SENDER_SIZE = 1 << 10;    // 2^16 = 65536
    const size_t INTERSECTION_SIZE = 100;
    
    std::vector<uint32_t> intersection_elements;
    std::vector<uint32_t> receiver_data;
    std::vector<uint32_t> sender_data;
    
public:
    APSIDataGenerator() : gen(rd()), dis(0, UINT32_MAX) {}
    
    std::string uint32_to_hex(uint32_t value) {
        std::stringstream ss;
        ss << std::hex << std::setfill('0') << std::setw(8) << value;
        return ss.str();
    }
    
    void generate_intersection_elements() {
        std::cout << "生成 " << INTERSECTION_SIZE << " 个交集元素..." << std::endl;
        
        std::set<uint32_t> unique_elements;
        while (unique_elements.size() < INTERSECTION_SIZE) {
            uint32_t element = dis(gen);
            unique_elements.insert(element);
        }
        
        intersection_elements.assign(unique_elements.begin(), unique_elements.end());
        
        std::cout << "前10个交集元素: ";
        for (size_t i = 0; i < std::min<size_t>(10, intersection_elements.size()); ++i) {
            std::cout << uint32_to_hex(intersection_elements[i]) << " ";
        }
        std::cout << std::endl;
    }
    
    void generate_receiver_data() {
        std::cout << "生成接收方数据 (2^10 = " << RECEIVER_SIZE << " 个元素)..." << std::endl;
        
        std::set<uint32_t> unique_elements;
        
        // 首先添加所有交集元素
        for (uint32_t elem : intersection_elements) {
            unique_elements.insert(elem);
        }
        
        // 然后添加随机元素直到达到目标大小
        while (unique_elements.size() < RECEIVER_SIZE) {
            uint32_t element = dis(gen);
            unique_elements.insert(element);
        }
        
        receiver_data.assign(unique_elements.begin(), unique_elements.end());
        
        // 随机打乱顺序
        std::shuffle(receiver_data.begin(), receiver_data.end(), gen);
        
        std::cout << "接收方数据生成完成，实际大小: " << receiver_data.size() << std::endl;
    }
    
    void generate_sender_data() {
        std::cout << "生成发送方数据 (2^16 = " << SENDER_SIZE << " 个元素)..." << std::endl;
        
        std::set<uint32_t> unique_elements;
        
        // 首先添加所有交集元素
        for (uint32_t elem : intersection_elements) {
            unique_elements.insert(elem);
        }
        
        // 然后添加随机元素直到达到目标大小
        while (unique_elements.size() < SENDER_SIZE) {
            uint32_t element = dis(gen);
            unique_elements.insert(element);
        }
        
        sender_data.assign(unique_elements.begin(), unique_elements.end());
        
        // 随机打乱顺序
        std::shuffle(sender_data.begin(), sender_data.end(), gen);
        
        std::cout << "发送方数据生成完成，实际大小: " << sender_data.size() << std::endl;
    }
    
    void save_intersection_file(const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("无法创建交集文件: " + filename);
        }
        
        for (uint32_t elem : intersection_elements) {
            file << uint32_to_hex(elem) << std::endl;
        }
        
        file.close();
        std::cout << "交集元素已保存到: " << filename << std::endl;
    }
    
    void save_receiver_file(const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("无法创建接收方文件: " + filename);
        }
        
        for (uint32_t elem : receiver_data) {
            file << uint32_to_hex(elem) << std::endl;
        }
        
        file.close();
        std::cout << "接收方数据已保存到: " << filename << std::endl;
    }
    
    void save_sender_file(const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("无法创建发送方文件: " + filename);
        }
        
        for (uint32_t elem : sender_data) {
            file << uint32_to_hex(elem) << std::endl;
        }
        
        file.close();
        std::cout << "发送方数据已保存到: " << filename << std::endl;
    }
    
    void generate_all_data() {
        std::cout << "=== APSI 数据生成器 ===" << std::endl;
        std::cout << "参数配置:" << std::endl;
        std::cout << "- 接收方数据大小: 2^10 = " << RECEIVER_SIZE << std::endl;
        std::cout << "- 发送方数据大小: 2^16 = " << SENDER_SIZE << std::endl;
        std::cout << "- 交集大小: " << INTERSECTION_SIZE << std::endl;
        std::cout << "- 数据类型: 32位无符号整数" << std::endl;
        std::cout << std::endl;
        
        generate_intersection_elements();
        generate_receiver_data();
        generate_sender_data();
        
        // 验证交集
        std::set<uint32_t> receiver_set(receiver_data.begin(), receiver_data.end());
        std::set<uint32_t> sender_set(sender_data.begin(), sender_data.end());
        
        size_t actual_intersection = 0;
        for (uint32_t elem : intersection_elements) {
            if (receiver_set.count(elem) && sender_set.count(elem)) {
                actual_intersection++;
            }
        }
        
        std::cout << std::endl << "验证结果:" << std::endl;
        std::cout << "- 期望交集大小: " << INTERSECTION_SIZE << std::endl;
        std::cout << "- 实际交集大小: " << actual_intersection << std::endl;
        
        if (actual_intersection == INTERSECTION_SIZE) {
            std::cout << "✅ 数据生成验证成功！" << std::endl;
        } else {
            std::cout << "❌ 数据生成验证失败！" << std::endl;
        }
    }
    
    void save_all_files(const std::string& data_dir) {
        save_intersection_file(data_dir + "/intersection.txt");
        save_receiver_file(data_dir + "/receiver_query.txt");
        save_sender_file(data_dir + "/sender_db.csv");
        
        // 生成统计信息文件
        std::ofstream stats(data_dir + "/data_stats.txt");
        stats << "APSI 数据集统计信息" << std::endl;
        stats << "===================" << std::endl;
        stats << "生成时间: " << std::time(nullptr) << std::endl;
        stats << "接收方数据大小: " << receiver_data.size() << std::endl;
        stats << "发送方数据大小: " << sender_data.size() << std::endl;
        stats << "交集大小: " << intersection_elements.size() << std::endl;
        stats << "数据类型: 32位十六进制字符串" << std::endl;
        stats.close();
        
        std::cout << "所有文件已保存到目录: " << data_dir << std::endl;
    }
};

int main() {
    try {
        APSIDataGenerator generator;
        
        // 生成所有数据
        generator.generate_all_data();
        
        // 保存到文件
        generator.save_all_files("../data");
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
}