#include "../mpc/vole/vole.hpp"
#include "bandokvs/band_okvs.h"
#include <future>
#include <vector>
#include <set>
#include <mutex>
#include <chrono>
#include <unordered_set>

using namespace band_okvs;
using namespace std;

// 为block类型定义哈希函数，用于unordered_set
struct FastPSIBlockHash {
    std::size_t operator()(const block& b) const {
        // 直接访问block的内部数据进行哈希
        const uint64_t* data = reinterpret_cast<const uint64_t*>(&b);
        return std::hash<uint64_t>{}(data[0]) ^ (std::hash<uint64_t>{}(data[1]) << 1);
    }
};

// 为block类型定义相等比较函数
struct FastPSIBlockEqual {
    bool operator()(const block& a, const block& b) const {
        return Block::Compare(a, b);
    }
};

struct FastPSITestcase{
    uint64_t N_item; // PSI集合元素个数
    uint64_t okvssize; // OKVS大小
    std::vector<block> elem_hashes; // 集合元素的哈希值
    std::vector<block> intersection_result; // 交集结果
    block delta; // VOLE中的delta值
}; 

// 将block转换为oc::block
oc::block BlockToOcBlock(const block& b) {
    // 直接访问block的内部数据
    const uint64_t* data = reinterpret_cast<const uint64_t*>(&b);
    return oc::block(data[0], data[1]);
}

// 将oc::block转换为block
block OcBlockToBlock(const oc::block& b) {
    // 使用block的构造函数或赋值
    uint64_t low = b.get<uint64_t>(0);
    uint64_t high = b.get<uint64_t>(1);
    block result;
    uint64_t* result_data = reinterpret_cast<uint64_t*>(&result);
    result_data[0] = low;
    result_data[1] = high;
    return result;
}

// 创建范围内的测试项目
std::vector<block> CreateRangeItems(size_t begin, size_t size) {
    std::vector<block> ret;
    PRG::Seed seed = PRG::SetSeed();
    for (size_t i = 0; i < size; ++i) {
        // 使用PRG生成随机block作为集合元素
        PRG::Seed current_seed = seed;
        block item = PRG::GenRandomBlocks(current_seed, 1)[0];
        // 简单混合begin和i来保证不同范围产生不同元素
        block mix_value;
        uint64_t* mix_data = reinterpret_cast<uint64_t*>(&mix_value);
        mix_data[0] = begin + i;
        mix_data[1] = 0;
        item = item ^ mix_value;
        ret.push_back(item);
    }
    return ret;
}

FastPSITestcase GenTestCase(uint64_t N_item)
{	
    FastPSITestcase testcase; 
    testcase.N_item = N_item;
    testcase.okvssize = N_item * 1.27; // OKVS通常需要比输入大27%
    
    return testcase;
}

// 保存测试用例
void SaveTestCase(FastPSITestcase &testcase, std::string testcase_filename)
{
    std::ofstream fout;
    fout.open(testcase_filename, std::ios::binary);
    if (!fout)
    {
        std::cerr << testcase_filename << " open error" << std::endl;
        exit(1);
    }
    fout << testcase.N_item;
    fout << testcase.okvssize;
    fout << testcase.delta;
    
    // 保存elem_hashes
    size_t elem_size = testcase.elem_hashes.size();
    fout << elem_size;
    for(const auto& elem : testcase.elem_hashes) {
        fout << elem;
    }
    
    // 保存intersection_result
    size_t result_size = testcase.intersection_result.size();
    fout << result_size;
    for(const auto& result : testcase.intersection_result) {
        fout << result;
    }
    
    fout.close();
}

// 读取测试用例
void FetchTestCase(FastPSITestcase &testcase, std::string testcase_filename)
{
    std::ifstream fin;
    fin.open(testcase_filename, std::ios::binary);
    if (!fin)
    {
        std::cerr << testcase_filename << " open error" << std::endl;
        exit(1);
    }
    fin >> testcase.N_item;
    fin >> testcase.okvssize;
    fin >> testcase.delta;
    
    // 读取elem_hashes
    size_t elem_size;
    fin >> elem_size;
    testcase.elem_hashes.resize(elem_size);
    for(auto& elem : testcase.elem_hashes) {
        fin >> elem;
    }
    
    // 读取intersection_result
    size_t result_size;
    fin >> result_size;
    testcase.intersection_result.resize(result_size);
    for(auto& result : testcase.intersection_result) {
        fin >> result;
    }
    
    fin.close();
}

// FastPSI接收方实现
std::vector<block> FastPsiRecv(NetIO& io, std::vector<block>& elem_hashes, 
                               uint64_t okvssize, uint64_t band_length, uint64_t t) {
    
    // 1. 初始化VOLE接收方，获取向量A和C
    std::vector<block> vec_A;
    std::vector<block> vec_C;
    vec_A = VOLE::VOLE_A(io, okvssize, vec_C, t);
    
    // 2. 初始化BandOkvs并进行编码
    BandOkvs okvs;
    okvs.Init(elem_hashes.size(), okvssize, band_length);
    
    // 准备OKVS编码的输入：转换为oc::block格式
    std::vector<oc::block> okvs_keys(elem_hashes.size());
    std::vector<oc::block> okvs_values(elem_hashes.size());
    std::vector<oc::block> okvs_output(okvssize);
    
    for(size_t i = 0; i < elem_hashes.size(); i++) {
        okvs_keys[i] = BlockToOcBlock(elem_hashes[i]);
        okvs_values[i] = BlockToOcBlock(elem_hashes[i]);
    }
    
    // OKVS编码，得到P向量
    bool encode_success = okvs.Encode(okvs_keys.data(), okvs_values.data(), okvs_output.data());
    if (!encode_success) {
        std::cerr << "OKVS encoding failed!" << std::endl;
        exit(1);
    }
    
    // 3. 计算A' = A ⊕ P，并发送给发送方
    std::vector<block> vec_A_prime(okvssize);
    for(size_t i = 0; i < okvssize; i++) {
        vec_A_prime[i] = vec_A[i] ^ OcBlockToBlock(okvs_output[i]);
    }
    
    // 发送A'
    io.SendBlocks(vec_A_prime.data(), okvssize);
    
    // 4. 使用OKVS解码计算接收方masks
    std::vector<oc::block> vec_C_oc(okvssize);
    for(size_t i = 0; i < okvssize; i++) {
        vec_C_oc[i] = BlockToOcBlock(vec_C[i]);
    }
    
    std::vector<oc::block> receivermasks_oc(elem_hashes.size());
    okvs.Decode(okvs_keys.data(), vec_C_oc.data(), receivermasks_oc.data(), elem_hashes.size());
    
    // 转换回block格式
    std::vector<block> receivermasks(elem_hashes.size());
    for(size_t i = 0; i < elem_hashes.size(); i++) {
        receivermasks[i] = OcBlockToBlock(receivermasks_oc[i]);
    }
    
    // 5. 接收发送方的masks
    std::vector<block> sendermasks(elem_hashes.size());
    io.ReceiveBlocks(sendermasks.data(), elem_hashes.size());
    
    // 6. 计算交集 - 比较相同位置的masks是否相等
    std::vector<block> intersection_elements;
    
    for(size_t i = 0; i < elem_hashes.size(); i++) {
        // FastPSI协议中，如果两个masks相等，说明对应的元素在交集中
        if(Block::Compare(receivermasks[i], sendermasks[i])) {
            intersection_elements.push_back(elem_hashes[i]);
        }
    }
    
    return intersection_elements;
}

// FastPSI发送方实现
void FastPsiSend(NetIO& io, std::vector<block>& elem_hashes, 
                 uint64_t okvssize, uint64_t band_length, uint64_t t) {
    
    // 1. 初始化VOLE发送方，获取向量B和delta
    std::vector<block> vec_B;
    PRG::Seed seed = PRG::SetSeed();
    block delta = PRG::GenRandomBlocks(seed, 1)[0];
    VOLE::VOLE_B(io, okvssize, vec_B, delta, t);
    
    // 2. 接收A'
    std::vector<block> vec_A_prime(okvssize);
    io.ReceiveBlocks(vec_A_prime.data(), okvssize);
    
    // 3. 计算k = B ⊕ (delta * A')
    std::vector<block> vec_K(okvssize);
    for(size_t i = 0; i < okvssize; i++) {
        vec_K[i] = vec_B[i] ^ VOLE::gf128_mul(delta, vec_A_prime[i]);
    }
    
    // 4. 初始化BandOkvs并解码
    BandOkvs okvs;
    okvs.Init(elem_hashes.size(), okvssize, band_length);
    
    // 转换为oc::block格式
    std::vector<oc::block> okvs_keys(elem_hashes.size());
    std::vector<oc::block> vec_K_oc(okvssize);
    
    for(size_t i = 0; i < elem_hashes.size(); i++) {
        okvs_keys[i] = BlockToOcBlock(elem_hashes[i]);
    }
    
    for(size_t i = 0; i < okvssize; i++) {
        vec_K_oc[i] = BlockToOcBlock(vec_K[i]);
    }
    
    // 使用k向量解码获取发送方masks
    std::vector<oc::block> sendermasks_oc(elem_hashes.size());
    okvs.Decode(okvs_keys.data(), vec_K_oc.data(), sendermasks_oc.data(), elem_hashes.size());
    
    // 转换回block格式并进行最终计算
    std::vector<block> sendermasks(elem_hashes.size());
    for(size_t i = 0; i < elem_hashes.size(); i++) {
        sendermasks[i] = OcBlockToBlock(sendermasks_oc[i]) ^ VOLE::gf128_mul(delta, elem_hashes[i]);
    }
    
    // 6. 发送masks给接收方
    io.SendBlocks(sendermasks.data(), elem_hashes.size());
}

int main()
{
    CRYPTO_Initialize();

    PrintSplitLine('-'); 
    std::cout << "FastPSI VOLE test begins >>>" << std::endl; 
    PrintSplitLine('-'); 
 
    // 设置测试参数
    uint64_t N_item = uint64_t(pow(2, 16)); // PSI集合大小
    uint64_t okvssize = N_item * 1.27; // OKVS大小
    uint64_t band_length = 512; // Band长度
    uint64_t t = 397; // VOLE参数
    
    std::string testcase_filename = "fastpsi_vole.testcase"; 
    std::string party;
    std::cout << "please select your role between sender and receiver (hint: first start receiver, then start sender) ==> ";
    std::getline(std::cin, party);

    if (party == "receiver")
    {
        // 创建网络连接 - 接收方作为服务器
        NetIO receiver_io("server", "", 8080);
        
        // 生成接收方的集合元素
        std::vector<block> receiver_elements = CreateRangeItems(0, N_item);
        
        std::cout << "Generated " << receiver_elements.size() << " receiver elements" << std::endl;
        
        auto start_time = std::chrono::steady_clock::now();
        
        // 执行FastPSI接收方协议
        std::vector<block> intersection = FastPsiRecv(receiver_io, receiver_elements, 
                                                      okvssize, band_length, t);
        
        auto end_time = std::chrono::steady_clock::now();
        
        std::cout << "Item_num = " << N_item << std::endl; 
        std::cout << "OKVS_size = " << okvssize << std::endl;
        std::cout << "FastPSI Receiver takes: " 
                  << std::chrono::duration<double, std::milli>(end_time - start_time).count() 
                  << " ms" << std::endl;
        std::cout << "Intersection size: " << intersection.size() << std::endl;
        
        // 保存测试用例用于验证
        FastPSITestcase testcase = GenTestCase(N_item);
        testcase.elem_hashes = receiver_elements;
        testcase.intersection_result = intersection;
        SaveTestCase(testcase, testcase_filename);
        
        PrintSplitLine('-');
        std::cout << "FastPSI Receiver test completes" << std::endl;
    }

    if (party == "sender")
    {
        // 创建网络连接 - 发送方作为客户端
        NetIO sender_io("client", "127.0.0.1", 8080);
        
        // 生成发送方的集合元素（与接收方有100个重叠元素）
        // 前100个元素与接收方重叠，其余元素不重叠
        std::vector<block> sender_elements;
        
        // 添加100个重叠元素（与接收方的前100个元素相同）
        std::vector<block> overlap_elements = CreateRangeItems(0, 100);
        sender_elements.insert(sender_elements.end(), overlap_elements.begin(), overlap_elements.end());
        
        // 添加其余不重叠的元素
        std::vector<block> non_overlap_elements = CreateRangeItems(N_item + 1000, N_item - 100);
        sender_elements.insert(sender_elements.end(), non_overlap_elements.begin(), non_overlap_elements.end());
        
        std::cout << "Generated " << sender_elements.size() << " sender elements" << std::endl;
        
        auto start_time = std::chrono::steady_clock::now();
        
        // 执行FastPSI发送方协议
        FastPsiSend(sender_io, sender_elements, okvssize, band_length, t);
        
        auto end_time = std::chrono::steady_clock::now();
        
        std::cout << "Item_num = " << N_item << std::endl; 
        std::cout << "OKVS_size = " << okvssize << std::endl;
        std::cout << "FastPSI Sender takes: " 
                  << std::chrono::duration<double, std::milli>(end_time - start_time).count() 
                  << " ms" << std::endl;
        
        PrintSplitLine('-');
        std::cout << "FastPSI Sender test completes" << std::endl;
        
        // 验证交集结果
        FastPSITestcase testcase;
        FetchTestCase(testcase, testcase_filename);
        
        // 预期交集大小应该是100个元素
        size_t expected_intersection_size = 100;
        
        if (testcase.intersection_result.size() == expected_intersection_size) {
            PrintSplitLine('-');
            std::cout << "FastPSI test succeeds! Intersection size matches expected: " 
                      << expected_intersection_size << std::endl;
        } else {
            PrintSplitLine('-');
            std::cout << "FastPSI test fails! Expected: " << expected_intersection_size 
                      << ", Got: " << testcase.intersection_result.size() << std::endl;
        }
    }

    PrintSplitLine('-'); 
    std::cout << "FastPSI VOLE test ends >>>" << std::endl; 
    PrintSplitLine('-'); 
    
    CRYPTO_Finalize();       
    return 0; 
}