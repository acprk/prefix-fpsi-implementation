// src/psi_fixed.cpp
// 修复版APSI距离隐私集合求交，解决SEAL参数问题

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <openssl/sha.h>

// APSI headers
#include "apsi/log.h"
#include "apsi/psi_params.h"
#include "apsi/sender_db.h"
#include "apsi/receiver.h"
#include "apsi/sender.h"
#include "apsi/item.h"
#include "apsi/network/stream_channel.h"
#include "apsi/oprf/oprf_sender.h"
#include "apsi/thread_pool_mgr.h"

// SEAL headers for parameter validation
#include "seal/seal.h"
#include "seal/util/numth.h"

using namespace std;
using namespace apsi;
using namespace apsi::sender;
using namespace apsi::receiver;
using namespace apsi::network;
using namespace seal;

class APSIDistancePSI {
private:
    static constexpr int DELTA = 50;
    
    // 生成有效的SEAL参数
    string generate_valid_seal_params(size_t sender_size, size_t receiver_size) {
        cout << "生成有效的SEAL参数用于数据量: Sender=" << sender_size << ", Receiver=" << receiver_size << endl;
        
        // **使用APSI推荐的参数配置**
        // 参考 APSI/parameters/ 目录中的配置文件
        
        size_t poly_modulus_degree;
        vector<int> coeff_modulus_bits;
        uint64_t plain_modulus;
        uint32_t felts_per_item;
        
        if (sender_size <= 1024) {
            // 参考 256K-1.json 的简化版本
            poly_modulus_degree = 4096;
            coeff_modulus_bits = {40, 32, 32, 40};
            plain_modulus = 40961;  // 这是一个适合batching的质数
            felts_per_item = 8;
        } else if (sender_size <= 16384) {
            // 参考 1M-1.json 的简化版本  
            poly_modulus_degree = 8192;
            coeff_modulus_bits = {50, 35, 35, 50};
            plain_modulus = 65537;  // 2^16 + 1，适合batching
            felts_per_item = 8;
        } else if (sender_size <= 65536) {
            // 参考更大数据集的配置
            poly_modulus_degree = 16384;
            coeff_modulus_bits = {50, 40, 40, 50};
            plain_modulus = 114689;  // 适合32768度的质数
            felts_per_item = 8;
        } else {
            // 大数据集配置
            poly_modulus_degree = 32768;
            coeff_modulus_bits = {50, 40, 40, 40, 50};
            plain_modulus = 65537;  // 使用65537确保batching支持
            felts_per_item = 8;
        }
        
        // 验证plain_modulus是否支持batching
        uint64_t target_modulus = 2 * poly_modulus_degree;
        if (plain_modulus % target_modulus != 1) {
            // 如果不支持，找一个支持的
            for (uint64_t candidate = target_modulus + 1; candidate < target_modulus * 20; candidate += target_modulus) {
                if (seal::util::is_prime(candidate)) {
                    plain_modulus = candidate;
                    break;
                }
            }
        }
        
        // **关键修复: 计算正确的table_size**
        // table_size必须是floor(poly_modulus_degree / felts_per_item)的倍数
        uint32_t bundle_size = poly_modulus_degree / felts_per_item;  // floor除法
        
        // 计算满足条件的table_size
        // 目标是sender_size的120%，但必须是bundle_size的倍数
        uint32_t target_table_size = (sender_size * 120) / 100;
        uint32_t table_size = ((target_table_size + bundle_size - 1) / bundle_size) * bundle_size;  // 向上取整到bundle_size的倍数
        
        // 计算和验证item_bit_count
        uint32_t plain_modulus_bits = static_cast<uint32_t>(floor(log2(plain_modulus)));
        uint32_t item_bit_count = felts_per_item * plain_modulus_bits;
        
        cout << "选择的参数:" << endl;
        cout << "  poly_modulus_degree: " << poly_modulus_degree << endl;
        cout << "  plain_modulus: " << plain_modulus << " (位数: " << plain_modulus_bits << ")" << endl;
        cout << "  felts_per_item: " << felts_per_item << endl;
        cout << "  bundle_size (poly_deg/felts): " << bundle_size << endl;
        cout << "  目标table_size: " << target_table_size << endl;
        cout << "  实际table_size: " << table_size << " (bundle_size的倍数)" << endl;
        cout << "  item_bit_count: " << item_bit_count << " (必须在80-128之间)" << endl;
        cout << "  coeff_modulus_bits: [";
        for (size_t i = 0; i < coeff_modulus_bits.size(); i++) {
            cout << coeff_modulus_bits[i];
            if (i < coeff_modulus_bits.size() - 1) cout << ", ";
        }
        cout << "]" << endl;
        cout << "  验证batching: " << plain_modulus << " % " << target_modulus << " = " << (plain_modulus % target_modulus) << endl;
        cout << "  验证table_size: " << table_size << " % " << bundle_size << " = " << (table_size % bundle_size) << endl;
        
        // 检查item_bit_count是否在有效范围内
        if (item_bit_count < 80 || item_bit_count > 128) {
            cout << "❌ 警告: item_bit_count " << item_bit_count << " 不在80-128范围内!" << endl;
            
            // 调整felts_per_item以获得有效的item_bit_count
            if (item_bit_count < 80) {
                felts_per_item = (80 + plain_modulus_bits - 1) / plain_modulus_bits;  // 向上取整
            } else {
                felts_per_item = 128 / plain_modulus_bits;  // 向下取整
            }
            
            // 重新计算bundle_size和table_size
            bundle_size = poly_modulus_degree / felts_per_item;
            table_size = ((target_table_size + bundle_size - 1) / bundle_size) * bundle_size;
            item_bit_count = felts_per_item * plain_modulus_bits;
            
            cout << "  调整后的felts_per_item: " << felts_per_item << endl;
            cout << "  调整后的bundle_size: " << bundle_size << endl;
            cout << "  调整后的table_size: " << table_size << endl;
            cout << "  调整后的item_bit_count: " << item_bit_count << endl;
        }
        
        // 生成JSON格式的参数
        stringstream params_json;
        params_json << "{\n";
        params_json << "  \"table_params\": {\n";
        params_json << "    \"hash_func_count\": 3,\n";
        params_json << "    \"table_size\": " << table_size << ",\n";
        params_json << "    \"max_items_per_bin\": 92\n";
        params_json << "  },\n";
        params_json << "  \"item_params\": {\n";
        params_json << "    \"felts_per_item\": " << felts_per_item << "\n";
        params_json << "  },\n";
        params_json << "  \"query_params\": {\n";
        params_json << "    \"ps_low_degree\": 0,\n";
        params_json << "    \"query_powers\": [1, 3, 4, 5, 8, 14, 20, 26, 32, 38, 41, 42, 43, 45, 46]\n";
        params_json << "  },\n";
        params_json << "  \"seal_params\": {\n";
        params_json << "    \"plain_modulus\": " << plain_modulus << ",\n";
        params_json << "    \"poly_modulus_degree\": " << poly_modulus_degree << ",\n";
        params_json << "    \"coeff_modulus_bits\": [";
        for (size_t i = 0; i < coeff_modulus_bits.size(); i++) {
            params_json << coeff_modulus_bits[i];
            if (i < coeff_modulus_bits.size() - 1) params_json << ", ";
        }
        params_json << "]\n";
        params_json << "  }\n";
        params_json << "}";
        
        return params_json.str();
    }
    
    // 验证SEAL参数是否有效
    bool validate_seal_params(const PSIParams& params) {
        try {
            // 创建SEAL上下文来验证参数
            EncryptionParameters seal_params(scheme_type::bfv);
            
            // 获取APSI中的SEAL参数
            const auto& apsi_seal_params = params.seal_params();
            
            // 设置多项式模数度
            size_t poly_deg = apsi_seal_params.poly_modulus_degree();
            seal_params.set_poly_modulus_degree(poly_deg);
            
            // **使用与生成参数时一致的coeff_modulus_bits逻辑**
            vector<int> coeff_bits;
            if (poly_deg == 4096) {
                coeff_bits = {40, 32, 32, 40};
            } else if (poly_deg == 8192) {
                coeff_bits = {50, 35, 35, 50};
            } else if (poly_deg == 16384) {
                coeff_bits = {50, 40, 40, 50};
            } else if (poly_deg == 32768) {
                coeff_bits = {50, 40, 40, 40, 50};
            } else {
                // 默认参数
                coeff_bits = {40, 32, 32, 40};
            }
            
            seal_params.set_coeff_modulus(CoeffModulus::Create(poly_deg, coeff_bits));
            
            // 设置明文模数
            seal_params.set_plain_modulus(apsi_seal_params.plain_modulus());
            
            // 验证item_bit_count
            uint64_t plain_mod_val = apsi_seal_params.plain_modulus().value();
            uint32_t plain_modulus_bits = static_cast<uint32_t>(floor(log2(plain_mod_val)));
            uint32_t felts_per_item = params.item_params().felts_per_item;
            uint32_t item_bit_count = felts_per_item * plain_modulus_bits;
            
            cout << "SEAL参数验证:" << endl;
            cout << "  poly_modulus_degree: " << poly_deg << endl;
            cout << "  plain_modulus: " << plain_mod_val << " (位数: " << plain_modulus_bits << ")" << endl;
            cout << "  felts_per_item: " << felts_per_item << endl;
            cout << "  item_bit_count: " << item_bit_count << " (要求: 80-128)" << endl;
            
            // 检查item_bit_count范围
            if (item_bit_count < 80 || item_bit_count > 128) {
                cout << "❌ item_bit_count " << item_bit_count << " 不在80-128范围内" << endl;
                return false;
            }
            
            // 创建上下文并检查参数有效性
            SEALContext context(seal_params);
            
            if (!context.parameters_set()) {
                cout << "❌ SEAL参数无效" << endl;
                return false;
            }
            
            if (!context.first_context_data()->qualifiers().using_batching) {
                cout << "❌ SEAL参数不支持批处理" << endl;
                cout << "要求: plain_modulus ≡ 1 (mod 2*poly_modulus_degree)" << endl;
                
                uint64_t poly_deg_val = apsi_seal_params.poly_modulus_degree();
                uint64_t mod_result = plain_mod_val % (2 * poly_deg_val);
                
                cout << "实际: " << plain_mod_val << " % " << (2 * poly_deg_val) << " = " << mod_result << endl;
                return false;
            }
            
            cout << "✅ SEAL参数验证通过" << endl;
            cout << "  支持批处理: 是" << endl;
            cout << "  item_bit_count: " << item_bit_count << " (有效)" << endl;
            cout << "  coeff_modulus_bits: [";
            for (size_t i = 0; i < coeff_bits.size(); i++) {
                cout << coeff_bits[i];
                if (i < coeff_bits.size() - 1) cout << ", ";
            }
            cout << "]" << endl;
            
            return true;
            
        } catch (const exception& e) {
            cout << "❌ SEAL参数验证失败: " << e.what() << endl;
            return false;
        }
    }
    
    // 创建无冲突的APSI Item对象
    Item create_item_from_string(const string& str) {
        vector<uint8_t> hash(SHA256_DIGEST_LENGTH);
        
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        SHA256_Update(&sha256, str.c_str(), str.length());
        SHA256_Final(hash.data(), &sha256);
        
        uint64_t low_word = 0;
        uint64_t high_word = 0;
        
        for (size_t i = 0; i < 8 && i < hash.size(); i++) {
            low_word |= (static_cast<uint64_t>(hash[i]) << (i * 8));
        }
        
        for (size_t i = 8; i < 16 && i < hash.size(); i++) {
            high_word |= (static_cast<uint64_t>(hash[i]) << ((i - 8) * 8));
        }
        
        return Item(low_word, high_word);
    }
    
    // 将Item转换为十六进制字符串
    string item_to_hex_string(const Item& item) {
        auto data_span = item.get_as<uint8_t>();
        const uint8_t* data = data_span.data();
        size_t size = data_span.size();
        
        stringstream ss;
        ss << hex << setfill('0');
        for (size_t i = 0; i < size; i++) {
            ss << setw(2) << static_cast<int>(data[i]);
        }
        return ss.str();
    }
    
    // 验证Item映射无冲突
    void verify_no_collisions(const vector<Item>& items, const vector<string>& prefixes, const string& name) {
        unordered_set<string> item_hashes;
        unordered_map<string, string> hash_to_prefix;
        int collision_count = 0;
        
        for (size_t i = 0; i < items.size(); i++) {
            string item_hex = item_to_hex_string(items[i]);
            
            if (item_hashes.count(item_hex) > 0) {
                collision_count++;
                cout << "❌ 发现冲突!" << endl;
                cout << "   前缀1: " << hash_to_prefix[item_hex] << endl;
                cout << "   前缀2: " << prefixes[i] << endl;
            } else {
                item_hashes.insert(item_hex);
                hash_to_prefix[item_hex] = prefixes[i];
            }
        }
        
        if (collision_count == 0) {
            cout << "✅ " << name << " Items无冲突 (唯一Items: " << item_hashes.size() << ")" << endl;
        } else {
            cout << "❌ " << name << " Items存在 " << collision_count << " 个冲突!" << endl;
        }
    }
    
    // 读取前缀数据
    vector<string> read_prefix_file(const string& filename) {
        vector<string> prefixes;
        ifstream file(filename);
        
        if (!file.is_open()) {
            cerr << "错误: 无法打开文件 " << filename << endl;
            return prefixes;
        }
        
        string line;
        while (getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            prefixes.push_back(line);
        }
        
        file.close();
        cout << "✓ 从 " << filename << " 读取了 " << prefixes.size() << " 个前缀" << endl;
        return prefixes;
    }
    
    // 读取前缀到IP的映射
    unordered_map<string, uint32_t> read_mapping_file(const string& filename) {
        unordered_map<string, uint32_t> mapping;
        ifstream file(filename);
        
        if (!file.is_open()) {
            cerr << "错误: 无法打开文件 " << filename << endl;
            return mapping;
        }
        
        string line;
        while (getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            
            size_t arrow_pos = line.find(" -> ");
            if (arrow_pos != string::npos) {
                string prefix = line.substr(0, arrow_pos);
                string ip_str = line.substr(arrow_pos + 4);
                
                try {
                    uint32_t ip = stoul(ip_str);
                    mapping[prefix] = ip;
                } catch (const exception& e) {
                    cerr << "警告: 无法解析映射行: " << line << endl;
                }
            }
        }
        
        file.close();
        cout << "✓ 从 " << filename << " 读取了 " << mapping.size() << " 个映射" << endl;
        return mapping;
    }
    
    // 读取原始IP数据
    vector<uint32_t> read_ip_file(const string& filename) {
        vector<uint32_t> ips;
        ifstream file(filename);
        
        if (!file.is_open()) {
            cerr << "错误: 无法打开文件 " << filename << endl;
            return ips;
        }
        
        string line;
        while (getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            
            try {
                uint32_t ip = stoul(line);
                ips.push_back(ip);
            } catch (const exception& e) {
                cerr << "警告: 无法解析IP行: " << line << endl;
            }
        }
        
        file.close();
        return ips;
    }
    
    // 验证本地交集
    void verify_local_intersection(const vector<string>& receiver_prefixes,
                                  const vector<string>& sender_prefixes) {
        cout << "\n=== 本地交集验证 ===" << endl;
        
        unordered_set<string> sender_set(sender_prefixes.begin(), sender_prefixes.end());
        vector<string> local_intersection;
        
        for (const auto& receiver_prefix : receiver_prefixes) {
            if (sender_set.count(receiver_prefix) > 0) {
                local_intersection.push_back(receiver_prefix);
            }
        }
        
        cout << "本地计算的交集大小: " << local_intersection.size() << endl;
        
        if (local_intersection.size() > 0) {
            cout << "本地交集示例：" << endl;
            for (size_t i = 0; i < min((size_t)10, local_intersection.size()); i++) {
                cout << "  " << (i+1) << ". " << local_intersection[i] << endl;
            }
        }
        
        // 保存本地交集用于对比
        ofstream local_file("results/local_intersection.txt");
        local_file << "# 本地计算的前缀交集\n";
        local_file << "# 总计 " << local_intersection.size() << " 个交集\n\n";
        for (size_t i = 0; i < local_intersection.size(); i++) {
            local_file << (i + 1) << ". " << local_intersection[i] << "\n";
        }
        local_file.close();
        cout << "✓ 本地交集已保存到 results/local_intersection.txt" << endl;
    }
    
public:
    // 执行APSI隐私集合求交
    vector<string> run_apsi_intersection(const vector<string>& receiver_prefixes,
                                        const vector<string>& sender_prefixes) {
        
        cout << "\n=== 执行APSI距离隐私集合求交（修复版）===" << endl;
        cout << "Receiver前缀数: " << receiver_prefixes.size() << endl;
        cout << "Sender前缀数: " << sender_prefixes.size() << endl;
        
        // 先验证本地交集
        verify_local_intersection(receiver_prefixes, sender_prefixes);
        
        auto start_time = chrono::high_resolution_clock::now();
        
        try {
            // 设置APSI环境
            ThreadPoolMgr::SetThreadCount(4);
            Log::SetLogLevel(Log::Level::info);
            Log::SetConsoleDisabled(false);
            
            // 创建通信通道
            stringstream channel_stream;
            StreamChannel channel(channel_stream);
            
            // 1. 生成有效的PSI参数
            cout << "\n步骤1: 生成有效的PSI参数..." << endl;
            
            string params_str = generate_valid_seal_params(sender_prefixes.size(), receiver_prefixes.size());
            
            // 保存参数到文件供调试用
            ofstream params_file("results/generated_params.json");
            params_file << params_str;
            params_file.close();
            cout << "✓ 生成的参数已保存到 results/generated_params.json" << endl;
            
            PSIParams params = PSIParams::Load(params_str);
            cout << "✓ PSI参数加载完成" << endl;
            
            // 验证SEAL参数
            if (!validate_seal_params(params)) {
                cerr << "❌ SEAL参数验证失败，无法继续执行" << endl;
                return {};
            }
            
            // 2. 创建Sender数据库
            cout << "\n步骤2: 创建Sender数据库..." << endl;
            shared_ptr<SenderDB> sender_db = make_shared<SenderDB>(params);
            
            // 将sender前缀转换为Item对象并插入数据库
            vector<Item> sender_items;
            for (const auto& prefix : sender_prefixes) {
                sender_items.push_back(create_item_from_string(prefix));
            }
            
            // 验证无冲突性
            cout << "验证Sender Items无冲突性..." << endl;
            verify_no_collisions(sender_items, sender_prefixes, "Sender");
            
            cout << "插入 " << sender_items.size() << " 个sender items到数据库..." << endl;
            
            try {
                sender_db->insert_or_assign(sender_items);
                cout << "✓ Sender数据库创建完成" << endl;
            } catch (const exception& e) {
                cerr << "❌ 插入Sender数据失败: " << e.what() << endl;
                return {};
            }
            
            // 3. 准备Receiver数据
            cout << "\n步骤3: 准备Receiver数据..." << endl;
            vector<Item> receiver_items;
            for (const auto& prefix : receiver_prefixes) {
                receiver_items.push_back(create_item_from_string(prefix));
            }
            
            // 验证无冲突性
            cout << "验证Receiver Items无冲突性..." << endl;
            verify_no_collisions(receiver_items, receiver_prefixes, "Receiver");
            
            cout << "✓ Receiver数据准备完成，包含 " << receiver_items.size() << " 个前缀items" << endl;
            
            // 4. OPRF阶段
            cout << "\n步骤4: 执行OPRF阶段..." << endl;
            
            // Receiver创建OPRF请求
            oprf::OPRFReceiver oprf_receiver = Receiver::CreateOPRFReceiver(receiver_items);
            Request oprf_request = Receiver::CreateOPRFRequest(oprf_receiver);
            
            // 发送OPRF请求
            channel.send(move(oprf_request));
            
            // Sender处理OPRF请求
            Request received_request = channel.receive_operation(sender_db->get_seal_context());
            OPRFRequest received_oprf_request = to_oprf_request(move(received_request));
            
            // 运行OPRF
            Sender::RunOPRF(received_oprf_request, sender_db->get_oprf_key(), channel);
            
            // Receiver接收OPRF响应并提取哈希
            Response response = channel.receive_response();
            OPRFResponse oprf_response = to_oprf_response(response);
            auto receiver_oprf_items = Receiver::ExtractHashes(oprf_response, oprf_receiver);
            
            cout << "✓ OPRF处理完成，获得 " << receiver_oprf_items.first.size() << " 个哈希items" << endl;
            
            // 5. PSI查询阶段
            cout << "\n步骤5: 执行PSI查询阶段..." << endl;
            
            // 创建Receiver对象并生成查询
            Receiver receiver_obj(params);
            pair<Request, IndexTranslationTable> query_data = receiver_obj.create_query(receiver_oprf_items.first);
            IndexTranslationTable itt = query_data.second;
            Request query_request = move(query_data.first);
            
            // 发送查询请求
            channel.send(move(query_request));
            
            // Sender接收并处理查询
            Request received_query_request = channel.receive_operation(sender_db->get_seal_context());
            QueryRequest query_req = to_query_request(received_query_request);
            
            // 创建Query对象并运行
            Query query(move(query_req), sender_db);
            Sender::RunQuery(query, channel);
            
            cout << "✓ PSI查询处理完成" << endl;
            
            // 6. 接收并处理结果
            cout << "\n步骤6: 接收并处理结果..." << endl;
            
            // 接收查询响应
            Response query_response = channel.receive_response();
            QueryResponse query_resp = to_query_response(query_response);
            uint32_t result_part_count = query_resp->package_count;
            
            cout << "✓ 预期接收 " << result_part_count << " 个结果包" << endl;
            
            // 接收所有ResultPart
            vector<ResultPart> result_parts;
            while (result_part_count--) {
                ResultPart result_part = channel.receive_result(receiver_obj.get_seal_context());
                result_parts.push_back(move(result_part));
            }
            
            // 处理结果
            vector<MatchRecord> results = receiver_obj.process_result(receiver_oprf_items.second, itt, result_parts);
            
            // 提取匹配的前缀
            vector<string> intersection_prefixes;
            for (size_t i = 0; i < receiver_prefixes.size(); i++) {
                if (i < results.size() && results[i].found) {
                    intersection_prefixes.push_back(receiver_prefixes[i]);
                    cout << "匹配项 " << (intersection_prefixes.size()) << ": " << receiver_prefixes[i] << endl;
                }
            }
            
            auto end_time = chrono::high_resolution_clock::now();
            auto duration = chrono::duration_cast<chrono::milliseconds>(end_time - start_time);
            
            cout << "\n=== APSI执行完成 ===" << endl;
            cout << "✓ 执行时间: " << duration.count() << " ms" << endl;
            cout << "✓ 找到 " << intersection_prefixes.size() << " 个匹配的前缀" << endl;
            
            return intersection_prefixes;
            
        } catch (const exception& e) {
            cerr << "❌ APSI执行失败: " << e.what() << endl;
            return {};
        }
    }
    
    // 验证结果并还原为原始IP匹配对
    void verify_and_analyze_results(const vector<string>& intersection_prefixes,
                                   const unordered_map<string, uint32_t>& receiver_mapping,
                                   const unordered_map<string, uint32_t>& sender_mapping,
                                   const vector<uint32_t>& original_receiver_ips,
                                   const vector<uint32_t>& original_sender_ips) {
        
        cout << "\n=== 结果验证与分析 ===" << endl;
        
        if (intersection_prefixes.empty()) {
            cout << "❌ APSI未找到任何交集，分析问题..." << endl;
            
            // 比较本地计算结果
            unordered_set<string> sender_set;
            for (const auto& [prefix, ip] : sender_mapping) {
                sender_set.insert(prefix);
            }
            
            int expected_matches = 0;
            for (const auto& [prefix, ip] : receiver_mapping) {
                if (sender_set.count(prefix) > 0) {
                    expected_matches++;
                }
            }
            
            cout << "预期应该有 " << expected_matches << " 个交集前缀" << endl;
            cout << "可能的问题：" << endl;
            cout << "1. 前缀编码不匹配" << endl;
            cout << "2. APSI参数配置问题" << endl;
            cout << "3. Item转换过程有误" << endl;
            return;
        }
        
        // 从交集前缀中提取原始IP
        unordered_set<uint32_t> matched_receiver_ips;
        vector<pair<uint32_t, uint32_t>> detected_ip_pairs;
        
        for (const auto& prefix : intersection_prefixes) {
            if (receiver_mapping.count(prefix) > 0) {
                uint32_t receiver_ip = receiver_mapping.at(prefix);
                matched_receiver_ips.insert(receiver_ip);
            }
        }
        
        cout << "前缀交集统计:" << endl;
        cout << "  总交集前缀数: " << intersection_prefixes.size() << endl;
        cout << "  涉及的Receiver IP数: " << matched_receiver_ips.size() << endl;
        
        // 为每个匹配的receiver找出其对应的sender
        for (uint32_t receiver_ip : matched_receiver_ips) {
            for (uint32_t sender_ip : original_sender_ips) {
                if (abs((int64_t)receiver_ip - (int64_t)sender_ip) <= DELTA) {
                    detected_ip_pairs.emplace_back(receiver_ip, sender_ip);
                }
            }
        }
        
        cout << "  检测到的IP距离匹配对数: " << detected_ip_pairs.size() << endl;
        
        // 保存结果
        save_results(intersection_prefixes, detected_ip_pairs, matched_receiver_ips);
    }
    
    // 保存结果到文件
    void save_results(const vector<string>& intersection_prefixes,
                     const vector<pair<uint32_t, uint32_t>>& detected_pairs,
                     const unordered_set<uint32_t>& matched_receivers) {
        
        // 保存交集前缀
        ofstream prefix_result_file("results/intersection_prefixes.txt");
        prefix_result_file << "# APSI距离隐私集合求交结果 - 前缀交集\n";
        prefix_result_file << "# 总计 " << intersection_prefixes.size() << " 个匹配前缀\n";
        prefix_result_file << "# 距离阈值δ = " << DELTA << "\n\n";
        
        for (size_t i = 0; i < intersection_prefixes.size(); i++) {
            prefix_result_file << (i + 1) << ". " << intersection_prefixes[i] << "\n";
        }
        prefix_result_file.close();
        
        cout << "\n=== 结果文件已保存 ===" << endl;
        cout << "✓ results/intersection_prefixes.txt - 前缀交集结果" << endl;
        cout << "✓ results/local_intersection.txt - 本地计算交集结果" << endl;
        cout << "✓ results/generated_params.json - 生成的SEAL参数" << endl;
    }
    
    // 主执行函数
    void run_complete_pipeline() {
        cout << "=== APSI距离隐私集合求交完整流程（修复版）===" << endl;
        cout << "基于前缀编码的距离感知隐私集合求交" << endl;
        cout << "距离阈值δ = " << DELTA << endl;
        cout << "使用SHA256确保无冲突映射，修复SEAL参数问题" << endl;
        cout << endl;
        
        // 创建results目录
        system("mkdir -p results");
        
        // 1. 读取编码后的前缀数据
        cout << "=== 步骤1: 读取编码数据 ===" << endl;
        auto receiver_prefixes = read_prefix_file("data/receiver_items.txt");
        auto sender_prefixes = read_prefix_file("data/sender_items.txt");
        
        if (receiver_prefixes.empty() || sender_prefixes.empty()) {
            cerr << "错误: 无法读取前缀数据文件！请先运行数据编码器。" << endl;
            return;
        }
        
        // 2. 读取映射关系
        cout << "\n=== 步骤2: 读取映射关系 ===" << endl;
        auto receiver_mapping = read_mapping_file("data/receiver_prefix_to_ip.txt");
        auto sender_mapping = read_mapping_file("data/sender_prefix_to_ip.txt");
        
        // 3. 读取原始IP数据（用于验证）
        cout << "\n=== 步骤3: 读取原始IP数据 ===" << endl;
        auto original_receiver_ips = read_ip_file("data/receiver_ips.txt");
        auto original_sender_ips = read_ip_file("data/sender_ips.txt");
        
        // 4. 执行APSI隐私集合求交
        cout << "\n=== 步骤4: 执行APSI求交 ===" << endl;
        auto intersection_prefixes = run_apsi_intersection(receiver_prefixes, sender_prefixes);
        
        // 5. 验证和分析结果（无论是否找到交集都进行分析）
        cout << "\n=== 步骤5: 验证和分析结果 ===" << endl;
        verify_and_analyze_results(intersection_prefixes, receiver_mapping, sender_mapping,
                                  original_receiver_ips, original_sender_ips);
        
        cout << "\n=== 完整流程执行完成 ===" << endl;
        if (!intersection_prefixes.empty()) {
            cout << "✅ APSI距离隐私集合求交验证成功！" << endl;
            cout << "找到 " << intersection_prefixes.size() << " 个匹配前缀" << endl;
        } else {
            cout << "⚠️ APSI未找到交集，请检查日志和本地交集对比" << endl;
        }
        cout << "所有结果已保存到 results/ 目录中" << endl;
    }
};

int main() {
    // 设置APSI日志级别
    apsi::Log::SetLogLevel(apsi::Log::Level::info);
    
    cout << "=== APSI距离隐私集合求交系统（修复版）===" << endl;
    cout << "基于前缀编码的距离感知隐私集合求交" << endl;
    cout << "距离阈值δ = 50" << endl;
    cout << "使用SHA256哈希确保映射无冲突" << endl;
    cout << "修复SEAL参数batching支持问题" << endl;
    cout << endl;
    
    APSIDistancePSI psi_runner;
    psi_runner.run_complete_pipeline();
    
    return 0;
}