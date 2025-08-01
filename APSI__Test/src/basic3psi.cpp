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

// SEAL headers
#include "seal/seal.h"
#include "seal/util/numth.h"

using namespace std;
using namespace apsi;
using namespace apsi::sender;
using namespace apsi::receiver;
using namespace apsi::network;
using namespace seal;

// 高精度计时器类
class PrecisionTimer {
private:
    chrono::high_resolution_clock::time_point start_time;
    string operation_name;
    
public:
    PrecisionTimer(const string& name) : operation_name(name) {
        start_time = chrono::high_resolution_clock::now();
        cout << "[TIMER START] " << operation_name << endl;
    }
    
    ~PrecisionTimer() {
        auto end_time = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::microseconds>(end_time - start_time);
        cout << "[TIMER END] " << operation_name << " took: " 
             << duration.count() / 1000.0 << " ms" << endl;
    }
    
    double get_elapsed_ms() {
        auto current_time = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::microseconds>(current_time - start_time);
        return duration.count() / 1000.0;
    }
    
    void checkpoint(const string& checkpoint_name) {
        double elapsed = get_elapsed_ms();
        cout << "[CHECKPOINT] " << operation_name << " - " << checkpoint_name 
             << ": " << elapsed << " ms" << endl;
    }
};

class APSIDistancePSI {
private:
    static constexpr int DELTA = 50;

    // 生成优化的SEAL参数
    string generate_valid_seal_params(size_t sender_size, size_t receiver_size) {
        PrecisionTimer timer("Parameter Generation");
        
        cout << "Generating SEAL parameters for Sender=" << sender_size 
             << ", Receiver=" << receiver_size << endl;

        size_t poly_modulus_degree;
        vector<int> coeff_modulus_bits;
        uint64_t plain_modulus;
        uint32_t felts_per_item = 8;

        // 根据数据集大小优化参数
        if (sender_size <= 16384) {
            poly_modulus_degree = 4096;
            coeff_modulus_bits = {40, 32, 32, 40};
            plain_modulus = 40961;
        } else if (sender_size <= 65536) {
            poly_modulus_degree = 8192;
            coeff_modulus_bits = {50, 35, 35, 50};
            plain_modulus = 65537;
        } else if (sender_size <= 262144) {
            poly_modulus_degree = 16384;
            coeff_modulus_bits = {50, 40, 40, 50};
            plain_modulus = 114689;
        } else {
            // 超大数据集优化
            poly_modulus_degree = 32768;
            coeff_modulus_bits = {60, 50, 50, 60};
            plain_modulus = 786433;
        }

        timer.checkpoint("Basic parameter selection");

        // 确保plain_modulus支持批处理
        uint64_t target_modulus = 2 * poly_modulus_degree;
        if (plain_modulus % target_modulus != 1) {
            for (uint64_t candidate = target_modulus + 1; candidate < target_modulus * 20; candidate += target_modulus) {
                if (seal::util::is_prime(candidate)) {
                    plain_modulus = candidate;
                    break;
                }
            }
        }

        timer.checkpoint("Plain modulus optimization");

        // 计算bundle_size和table_size
        uint32_t bundle_size = poly_modulus_degree / felts_per_item;
        uint32_t target_table_size = (sender_size * 105) / 100; // 减少到105%提高效率
        uint32_t table_size = ((target_table_size + bundle_size - 1) / bundle_size) * bundle_size;

        // 验证item_bit_count
        uint32_t plain_modulus_bits = static_cast<uint32_t>(floor(log2(plain_modulus)));
        uint32_t item_bit_count = felts_per_item * plain_modulus_bits;

        if (item_bit_count < 80 || item_bit_count > 128) {
            felts_per_item = (item_bit_count < 80) ? (80 + plain_modulus_bits - 1) / plain_modulus_bits : 128 / plain_modulus_bits;
            bundle_size = poly_modulus_degree / felts_per_item;
            table_size = ((target_table_size + bundle_size - 1) / bundle_size) * bundle_size;
            item_bit_count = felts_per_item * plain_modulus_bits;
        }

        timer.checkpoint("Table size calculation");

        // 生成JSON参数
        stringstream params_json;
        params_json << "{\n";
        params_json << "  \"table_params\": {\n";
        params_json << "    \"hash_func_count\": 3,\n";
        params_json << "    \"table_size\": " << table_size << ",\n";
        params_json << "    \"max_items_per_bin\": 80\n"; // 减少bin大小
        params_json << "  },\n";
        params_json << "  \"item_params\": {\n";
        params_json << "    \"felts_per_item\": " << felts_per_item << "\n";
        params_json << "  },\n";
        params_json << "  \"query_params\": {\n";
        params_json << "    \"ps_low_degree\": 0,\n";
        params_json << "    \"query_powers\": [1, 3, 5]\n"; // 进一步减少query powers
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

        cout << "Generated parameters: poly_degree=" << poly_modulus_degree 
             << ", table_size=" << table_size << ", bundle_size=" << bundle_size << endl;

        return params_json.str();
    }

    // 验证SEAL参数
    bool validate_seal_params(const PSIParams& params) {
        PrecisionTimer timer("Parameter Validation");
        
        try {
            EncryptionParameters seal_params(scheme_type::bfv);
            const auto& apsi_seal_params = params.seal_params();
            seal_params.set_poly_modulus_degree(apsi_seal_params.poly_modulus_degree());
            seal_params.set_coeff_modulus(CoeffModulus::Create(apsi_seal_params.poly_modulus_degree(), 
                apsi_seal_params.coeff_modulus().size() == 4 ? vector<int>{40, 32, 32, 40} : vector<int>{50, 40, 40, 50}));
            seal_params.set_plain_modulus(apsi_seal_params.plain_modulus());

            SEALContext context(seal_params);
            bool is_valid = context.parameters_set() && context.first_context_data()->qualifiers().using_batching;
            
            cout << "SEAL validation result: " << (is_valid ? "VALID" : "INVALID") << endl;
            return is_valid;
        } catch (const exception& e) {
            cout << "SEAL validation failed: " << e.what() << endl;
            return false;
        }
    }

    // 从字符串创建Item
    Item create_item_from_string(const string& str) {
        vector<uint8_t> hash(SHA256_DIGEST_LENGTH);
        SHA256(reinterpret_cast<const unsigned char*>(str.c_str()), str.length(), hash.data());
        uint64_t low_word = 0, high_word = 0;
        for (size_t i = 0; i < 8 && i < hash.size(); i++) {
            low_word |= (static_cast<uint64_t>(hash[i]) << (i * 8));
        }
        for (size_t i = 8; i < 16 && i < hash.size(); i++) {
            high_word |= (static_cast<uint64_t>(hash[i]) << ((i - 8) * 8));
        }
        return Item(low_word, high_word);
    }

    // 批量创建Items（优化版本）
    vector<Item> create_items_batch(const vector<string>& strings) {
        PrecisionTimer timer("Batch Item Creation");
        
        vector<Item> items;
        items.reserve(strings.size());
        
        size_t processed = 0;
        for (const auto& str : strings) {
            items.push_back(create_item_from_string(str));
            processed++;
            
            if (processed % 10000 == 0) {
                timer.checkpoint("Processed " + to_string(processed) + " items");
            }
        }
        
        cout << "Created " << items.size() << " items from strings" << endl;
        return items;
    }

    // 读取文件函数保持不变
    vector<string> read_prefix_file(const string& filename) {
        PrecisionTimer timer("Reading prefix file: " + filename);
        
        vector<string> prefixes;
        ifstream file(filename);
        string line;
        while (getline(file, line)) {
            if (!line.empty() && line[0] != '#') prefixes.push_back(line);
        }
        file.close();
        
        cout << "Read " << prefixes.size() << " prefixes from " << filename << endl;
        return prefixes;
    }

    unordered_map<string, uint32_t> read_mapping_file(const string& filename) {
        PrecisionTimer timer("Reading mapping file: " + filename);
        
        unordered_map<string, uint32_t> mapping;
        ifstream file(filename);
        string line;
        while (getline(file, line)) {
            if (!line.empty() && line[0] != '#') {
                size_t arrow_pos = line.find(" -> ");
                if (arrow_pos != string::npos) {
                    string prefix = line.substr(0, arrow_pos);
                    try {
                        mapping[prefix] = stoul(line.substr(arrow_pos + 4));
                    } catch (...) {}
                }
            }
        }
        file.close();
        
        cout << "Read " << mapping.size() << " mappings from " << filename << endl;
        return mapping;
    }

    vector<uint32_t> read_ip_file(const string& filename) {
        PrecisionTimer timer("Reading IP file: " + filename);
        
        vector<uint32_t> ips;
        ifstream file(filename);
        string line;
        while (getline(file, line)) {
            if (!line.empty() && line[0] != '#') {
                try {
                    ips.push_back(stoul(line));
                } catch (...) {}
            }
        }
        file.close();
        
        cout << "Read " << ips.size() << " IPs from " << filename << endl;
        return ips;
    }

    // 执行APSI协议的辅助函数
    vector<string> execute_apsi_protocol(const PSIParams& params,
                                        const vector<string>& sender_prefixes,
                                        const vector<string>& receiver_prefixes) {
        vector<string> intersection_prefixes;
        
        try {
            // 创建通信通道
            stringstream channel_stream;
            StreamChannel channel(channel_stream);

            // 创建Sender数据库
            shared_ptr<SenderDB> sender_db;
            vector<Item> sender_items;
            {
                PrecisionTimer timer("Sender Database Creation");
                sender_db = make_shared<SenderDB>(params);
                timer.checkpoint("SenderDB object created");
                
                sender_items = create_items_batch(sender_prefixes);
                timer.checkpoint("Sender items created");
                
                sender_db->insert_or_assign(sender_items);
                timer.checkpoint("Sender database populated");
            }

            // 准备Receiver数据
            vector<Item> receiver_items;
            {
                PrecisionTimer timer("Receiver Data Preparation");
                receiver_items = create_items_batch(receiver_prefixes);
            }

            // 完整的APSI协议执行
            auto start_protocol = chrono::high_resolution_clock::now();
            
            // OPRF阶段
            {
                PrecisionTimer timer("OPRF Phase");
                
                auto oprf_receiver = Receiver::CreateOPRFReceiver(receiver_items);
                timer.checkpoint("OPRF receiver created");
                
                auto oprf_request = Receiver::CreateOPRFRequest(oprf_receiver);
                timer.checkpoint("OPRF request created");
                
                // 发送OPRF请求 - 使用移动语义
                channel.send(std::move(oprf_request));
                timer.checkpoint("OPRF request sent");
                
                // 接收并处理OPRF请求
                auto received_request = channel.receive_operation(sender_db->get_seal_context());
                timer.checkpoint("OPRF request received by sender");
                
                auto received_oprf_request = to_oprf_request(std::move(received_request));
                timer.checkpoint("OPRF request converted");
                
                // Sender运行OPRF
                Sender::RunOPRF(received_oprf_request, sender_db->get_oprf_key(), channel);
                timer.checkpoint("OPRF computation completed");
                
                // 接收OPRF响应
                auto response = channel.receive_response();
                timer.checkpoint("OPRF response received");
                
                auto oprf_response = to_oprf_response(response);
                timer.checkpoint("OPRF response converted");
                
                // 提取哈希值
                auto receiver_oprf_items = Receiver::ExtractHashes(oprf_response, oprf_receiver);
                timer.checkpoint("OPRF hashes extracted");
                
                cout << "OPRF phase completed successfully" << endl;
                
                // PSI查询阶段
                {
                    PrecisionTimer query_timer("PSI Query Phase");
                    
                    Receiver receiver_obj(params);
                    query_timer.checkpoint("Receiver object created");
                    
                    // 创建查询
                    auto query_result = receiver_obj.create_query(receiver_oprf_items.first);
                    query_timer.checkpoint("Query created");
                    
                    // 发送查询 - 使用移动语义
                    channel.send(std::move(query_result.first));
                    query_timer.checkpoint("Query sent");
                    
                    // 接收并处理查询
                    auto received_query_request = channel.receive_operation(sender_db->get_seal_context());
                    query_timer.checkpoint("Query received by sender");
                    
                    // 创建Query对象 - 使用移动语义
                    Query query(to_query_request(std::move(received_query_request)), sender_db);
                    query_timer.checkpoint("Query object created");
                    
                    // Sender运行查询
                    Sender::RunQuery(query, channel);
                    query_timer.checkpoint("Query processing completed");
                    
                    cout << "PSI query phase completed successfully" << endl;
                    
                    // 处理结果
                    {
                        PrecisionTimer result_timer("Result Processing");
                        
                        auto query_response = channel.receive_response();
                        result_timer.checkpoint("Query response received");
                        
                        auto query_resp = to_query_response(query_response);
                        result_timer.checkpoint("Query response converted");
                        
                        cout << "Processing " << query_resp->package_count << " result packages" << endl;
                        
                        vector<ResultPart> result_parts;
                        result_parts.reserve(query_resp->package_count);
                        
                        for (uint32_t i = 0; i < query_resp->package_count; i++) {
                            result_parts.push_back(channel.receive_result(receiver_obj.get_seal_context()));
                            if ((i + 1) % 100 == 0) {
                                result_timer.checkpoint("Processed " + to_string(i + 1) + " result packages");
                            }
                        }
                        result_timer.checkpoint("All result packages received");
                        
                        // 处理最终结果
                        auto results = receiver_obj.process_result(
                            receiver_oprf_items.second, 
                            query_result.second, 
                            result_parts
                        );
                        result_timer.checkpoint("Results processed");
                        
                        // 提取交集
                        for (size_t i = 0; i < receiver_prefixes.size() && i < results.size(); i++) {
                            if (results[i].found) {
                                intersection_prefixes.push_back(receiver_prefixes[i]);
                            }
                        }
                        result_timer.checkpoint("Intersection extracted");
                        
                        cout << "Found " << intersection_prefixes.size() << " matching prefixes" << endl;
                    }
                }
            }
            
        } catch (const exception& e) {
            cerr << "APSI protocol execution failed: " << e.what() << endl;
        }
        
        return intersection_prefixes;
    }

public:
    // 运行APSI交集
    vector<string> run_apsi_intersection(const vector<string>& receiver_prefixes,
                                        const vector<string>& sender_prefixes) {
        PrecisionTimer total_timer("Total APSI Intersection");
        vector<string> intersection_prefixes;

        try {
            // 设置APSI环境
            {
                PrecisionTimer timer("APSI Environment Setup");
                ThreadPoolMgr::SetThreadCount(16); // 增加线程数
                Log::SetLogLevel(Log::Level::warning); // 减少日志输出
                timer.checkpoint("Thread pool and logging setup");
            }

            // 生成和验证参数
            string params_str;
            {
                PrecisionTimer timer("Parameter Setup");
                params_str = generate_valid_seal_params(sender_prefixes.size(), receiver_prefixes.size());
                timer.checkpoint("Parameter generation completed");
                
                auto params = PSIParams::Load(params_str);
                timer.checkpoint("Parameter loading completed");
                
                if (!validate_seal_params(params)) {
                    cout << "Parameter validation failed!" << endl;
                    return {};
                }
                timer.checkpoint("Parameter validation completed");
                
                // 执行完整的APSI协议
                return execute_apsi_protocol(params, sender_prefixes, receiver_prefixes);
            }
        } catch (const exception& e) {
            cerr << "APSI failed: " << e.what() << endl;
        }

        return intersection_prefixes;
    }

    // 主流水线
    void run_complete_pipeline() {
        PrecisionTimer total_timer("Complete Pipeline");
        
        system("mkdir -p results");

        // 读取数据
        vector<string> receiver_prefixes, sender_prefixes;
        unordered_map<string, uint32_t> receiver_mapping, sender_mapping;
        vector<uint32_t> original_receiver_ips, original_sender_ips;
        
        {
            PrecisionTimer timer("Data Loading");
            
            receiver_prefixes = read_prefix_file("data/receiver_items.txt");
            sender_prefixes = read_prefix_file("data/sender_items.txt");
            timer.checkpoint("Prefix files loaded");
            
            if (receiver_prefixes.empty() || sender_prefixes.empty()) {
                cerr << "Error: Failed to read prefix files" << endl;
                return;
            }

            receiver_mapping = read_mapping_file("data/receiver_prefix_to_ip.txt");
            sender_mapping = read_mapping_file("data/sender_prefix_to_ip.txt");
            timer.checkpoint("Mapping files loaded");
            
            original_receiver_ips = read_ip_file("data/receiver_ips.txt");
            original_sender_ips = read_ip_file("data/sender_ips.txt");
            timer.checkpoint("IP files loaded");
        }

        // 运行APSI
        vector<string> intersection_prefixes;
        {
            PrecisionTimer timer("APSI Execution");
            intersection_prefixes = run_apsi_intersection(receiver_prefixes, sender_prefixes);
        }

        // 保存和分析结果
        {
            PrecisionTimer timer("Result Analysis and Saving");
            
            ofstream prefix_file("results/intersection_prefixes.txt");
            for (size_t i = 0; i < intersection_prefixes.size(); i++) {
                prefix_file << (i + 1) << ". " << intersection_prefixes[i] << "\n";
            }
            prefix_file.close();
            timer.checkpoint("Prefix results saved");

            // 分析结果
            unordered_set<uint32_t> matched_receiver_ips;
            vector<pair<uint32_t, uint32_t>> detected_ip_pairs;
            
            for (const auto& prefix : intersection_prefixes) {
                if (receiver_mapping.count(prefix)) {
                    matched_receiver_ips.insert(receiver_mapping.at(prefix));
                }
            }
            timer.checkpoint("Receiver IP matching completed");
            
            for (uint32_t receiver_ip : matched_receiver_ips) {
                for (uint32_t sender_ip : original_sender_ips) {
                    if (abs((int64_t)receiver_ip - (int64_t)sender_ip) <= DELTA) {
                        detected_ip_pairs.emplace_back(receiver_ip, sender_ip);
                    }
                }
            }
            timer.checkpoint("Distance analysis completed");

            cout << "\n=== FINAL RESULTS ===" << endl;
            cout << "Intersection prefixes: " << intersection_prefixes.size() << endl;
            cout << "Receiver IPs involved: " << matched_receiver_ips.size() << endl;
            cout << "IP distance matches: " << detected_ip_pairs.size() << endl;
        }
    }
};

int main() {
    cout << "Starting APSI Distance PSI with detailed timing..." << endl;
    
    apsi::Log::SetLogLevel(apsi::Log::Level::warning);
    APSIDistancePSI psi_runner;
    psi_runner.run_complete_pipeline();
    
    cout << "Program completed." << endl;
    return 0;
}