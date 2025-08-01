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

class APSIDistancePSI {
private:
    static constexpr int DELTA = 50;

    // Generate optimized SEAL parameters
    string generate_valid_seal_params(size_t sender_size, size_t receiver_size) {
        cout << "Generating SEAL parameters for Sender=" << sender_size << ", Receiver=" << receiver_size << endl;

        size_t poly_modulus_degree;
        vector<int> coeff_modulus_bits;
        uint64_t plain_modulus;
        uint32_t felts_per_item = 8;

        // Optimized parameters for different dataset sizes
        if (sender_size <= 16384) {
            poly_modulus_degree = 4096;
            coeff_modulus_bits = {40, 32, 32, 40};
            plain_modulus = 40961;
        } else if (sender_size <= 65536) {
            poly_modulus_degree = 8192;
            coeff_modulus_bits = {50, 35, 35, 50};
            plain_modulus = 65537;
        } else {
            poly_modulus_degree = 16384;
            coeff_modulus_bits = {50, 40, 40, 50};
            plain_modulus = 114689;
        }

        // Ensure plain_modulus supports batching
        uint64_t target_modulus = 2 * poly_modulus_degree;
        if (plain_modulus % target_modulus != 1) {
            for (uint64_t candidate = target_modulus + 1; candidate < target_modulus * 20; candidate += target_modulus) {
                if (seal::util::is_prime(candidate)) {
                    plain_modulus = candidate;
                    break;
                }
            }
        }

        // Calculate bundle_size and table_size
        uint32_t bundle_size = poly_modulus_degree / felts_per_item;
        uint32_t target_table_size = (sender_size * 110) / 100; // Reduced to 110% for efficiency
        uint32_t table_size = ((target_table_size + bundle_size - 1) / bundle_size) * bundle_size;

        // Validate item_bit_count
        uint32_t plain_modulus_bits = static_cast<uint32_t>(floor(log2(plain_modulus)));
        uint32_t item_bit_count = felts_per_item * plain_modulus_bits;

        if (item_bit_count < 80 || item_bit_count > 128) {
            felts_per_item = (item_bit_count < 80) ? (80 + plain_modulus_bits - 1) / plain_modulus_bits : 128 / plain_modulus_bits;
            bundle_size = poly_modulus_degree / felts_per_item;
            table_size = ((target_table_size + bundle_size - 1) / bundle_size) * bundle_size;
            item_bit_count = felts_per_item * plain_modulus_bits;
        }

        // Generate JSON parameters
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
        params_json << "    \"query_powers\": [1, 3, 5, 7, 9]\n"; // Reduced query powers
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

    // Validate SEAL parameters
    bool validate_seal_params(const PSIParams& params) {
        try {
            EncryptionParameters seal_params(scheme_type::bfv);
            const auto& apsi_seal_params = params.seal_params();
            seal_params.set_poly_modulus_degree(apsi_seal_params.poly_modulus_degree());
            seal_params.set_coeff_modulus(CoeffModulus::Create(apsi_seal_params.poly_modulus_degree(), 
                apsi_seal_params.coeff_modulus().size() == 4 ? vector<int>{40, 32, 32, 40} : vector<int>{50, 40, 40, 50}));
            seal_params.set_plain_modulus(apsi_seal_params.plain_modulus());

            SEALContext context(seal_params);
            if (!context.parameters_set() || !context.first_context_data()->qualifiers().using_batching) {
                return false;
            }
            return true;
        } catch (const exception& e) {
            return false;
        }
    }

    // Create Item from string
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

    // Convert Item to hex string
    string item_to_hex_string(const Item& item) {
        auto data_span = item.get_as<uint8_t>();
        stringstream ss;
        ss << hex << setfill('0');
        for (size_t i = 0; i < data_span.size(); i++) {
            ss << setw(2) << static_cast<int>(data_span[i]);
        }
        return ss.str();
    }

    // Read prefixes from file
    vector<string> read_prefix_file(const string& filename) {
        vector<string> prefixes;
        ifstream file(filename);
        string line;
        while (getline(file, line)) {
            if (!line.empty() && line[0] != '#') prefixes.push_back(line);
        }
        file.close();
        return prefixes;
    }

    // Read prefix-to-IP mapping
    unordered_map<string, uint32_t> read_mapping_file(const string& filename) {
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
        return mapping;
    }

    // Read IPs from file
    vector<uint32_t> read_ip_file(const string& filename) {
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
        return ips;
    }

public:
    // Run APSI intersection
    vector<string> run_apsi_intersection(const vector<string>& receiver_prefixes,
                                        const vector<string>& sender_prefixes) {
        auto start_time = chrono::high_resolution_clock::now();
        vector<string> intersection_prefixes;

        try {
            // Set APSI environment
            ThreadPoolMgr::SetThreadCount(8); // Increased threads
            Log::SetLogLevel(Log::Level::info);

            // Create communication channel
            stringstream channel_stream;
            StreamChannel channel(channel_stream);

            // Generate and validate parameters
            string params_str = generate_valid_seal_params(sender_prefixes.size(), receiver_prefixes.size());
            PSIParams params = PSIParams::Load(params_str);
            if (!validate_seal_params(params)) {
                return {};
            }

            // Create Sender database
            shared_ptr<SenderDB> sender_db = make_shared<SenderDB>(params);
            vector<Item> sender_items;
            sender_items.reserve(sender_prefixes.size());
            for (const auto& prefix : sender_prefixes) {
                sender_items.push_back(create_item_from_string(prefix));
            }
            sender_db->insert_or_assign(sender_items);

            // Prepare Receiver data
            vector<Item> receiver_items;
            receiver_items.reserve(receiver_prefixes.size());
            for (const auto& prefix : receiver_prefixes) {
                receiver_items.push_back(create_item_from_string(prefix));
            }

            // OPRF phase
            oprf::OPRFReceiver oprf_receiver = Receiver::CreateOPRFReceiver(receiver_items);
            Request oprf_request = Receiver::CreateOPRFRequest(oprf_receiver);
            channel.send(move(oprf_request));
            Request received_request = channel.receive_operation(sender_db->get_seal_context());
            OPRFRequest received_oprf_request = to_oprf_request(move(received_request));
            Sender::RunOPRF(received_oprf_request, sender_db->get_oprf_key(), channel);
            Response response = channel.receive_response();
            OPRFResponse oprf_response = to_oprf_response(response);
            auto receiver_oprf_items = Receiver::ExtractHashes(oprf_response, oprf_receiver);

            // PSI query phase
            Receiver receiver_obj(params);
            pair<Request, IndexTranslationTable> query_data = receiver_obj.create_query(receiver_oprf_items.first);
            channel.send(move(query_data.first));
            Request received_query_request = channel.receive_operation(sender_db->get_seal_context());
            Query query(to_query_request(received_query_request), sender_db);
            Sender::RunQuery(query, channel);

            // Process results
            Response query_response = channel.receive_response();
            QueryResponse query_resp = to_query_response(query_response);
            vector<ResultPart> result_parts;
            for (uint32_t i = 0; i < query_resp->package_count; i++) {
                result_parts.push_back(channel.receive_result(receiver_obj.get_seal_context()));
            }
            vector<MatchRecord> results = receiver_obj.process_result(receiver_oprf_items.second, query_data.second, result_parts);

            // Extract intersection
            for (size_t i = 0; i < receiver_prefixes.size() && i < results.size(); i++) {
                if (results[i].found) {
                    intersection_prefixes.push_back(receiver_prefixes[i]);
                }
            }

            auto end_time = chrono::high_resolution_clock::now();
            auto duration = chrono::duration_cast<chrono::milliseconds>(end_time - start_time);
            cout << "APSI execution time: " << duration.count() << " ms" << endl;
            cout << "Found " << intersection_prefixes.size() << " matching prefixes" << endl;

        } catch (const exception& e) {
            cerr << "APSI failed: " << e.what() << endl;
        }

        return intersection_prefixes;
    }

    // Main pipeline
    void run_complete_pipeline() {
        system("mkdir -p results");

        // Read data
        auto receiver_prefixes = read_prefix_file("data/receiver_items.txt");
        auto sender_prefixes = read_prefix_file("data/sender_items.txt");
        if (receiver_prefixes.empty() || sender_prefixes.empty()) {
            cerr << "Error: Failed to read prefix files" << endl;
            return;
        }

        auto receiver_mapping = read_mapping_file("data/receiver_prefix_to_ip.txt");
        auto sender_mapping = read_mapping_file("data/sender_prefix_to_ip.txt");
        auto original_receiver_ips = read_ip_file("data/receiver_ips.txt");
        auto original_sender_ips = read_ip_file("data/sender_ips.txt");

        // Run APSI
        auto intersection_prefixes = run_apsi_intersection(receiver_prefixes, sender_prefixes);

        // Save results
        ofstream prefix_file("results/intersection_prefixes.txt");
        for (size_t i = 0; i < intersection_prefixes.size(); i++) {
            prefix_file << (i + 1) << ". " << intersection_prefixes[i] << "\n";
        }
        prefix_file.close();

        // Analyze results
        unordered_set<uint32_t> matched_receiver_ips;
        vector<pair<uint32_t, uint32_t>> detected_ip_pairs;
        for (const auto& prefix : intersection_prefixes) {
            if (receiver_mapping.count(prefix)) {
                matched_receiver_ips.insert(receiver_mapping.at(prefix));
            }
        }
        for (uint32_t receiver_ip : matched_receiver_ips) {
            for (uint32_t sender_ip : original_sender_ips) {
                if (abs((int64_t)receiver_ip - (int64_t)sender_ip) <= DELTA) {
                    detected_ip_pairs.emplace_back(receiver_ip, sender_ip);
                }
            }
        }

        cout << "Intersection prefixes: " << intersection_prefixes.size() << endl;
        cout << "Receiver IPs involved: " << matched_receiver_ips.size() << endl;
        cout << "IP distance matches: " << detected_ip_pairs.size() << endl;
    }
};

int main() {
    apsi::Log::SetLogLevel(apsi::Log::Level::info);
    APSIDistancePSI psi_runner;
    psi_runner.run_complete_pipeline();
    return 0;
}