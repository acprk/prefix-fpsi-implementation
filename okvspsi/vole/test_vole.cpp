#include "../mpc/vole/vole.hpp"

struct VOLETestcase{
    uint64_t N_item; // the item num of VOLE output N_item 代表输入VOLE的测试例子的数据个数
    // uint64_t t;    
    std::vector<block> vec_B; //block类型的向量，储存B方的输入
    block delta; //用作乘法的常数值
}; 

VOLETestcase GenTestCase(uint64_t N_item)
{	
    VOLETestcase testcase; 
    testcase.N_item = N_item;  //设置N_item的值
    // testcase.t = 397; 以前测试是397？
    
    return testcase;
}
//写入对N_item, delta,vec_B的值
void SaveTestCase(VOLETestcase &testcase, std::string testcase_filename)
{
    std::ofstream fout;
    fout.open(testcase_filename, std::ios::binary);
    if (!fout)
    {
        std::cerr << testcase_filename << " open error" << std::endl;
        exit(1);
    }
    fout << testcase.N_item;
    // fout << testcase.t;
    fout << testcase.delta;
    fout << testcase.vec_B;
    fout.close();
}
//依次读取N_item, delta,vec_B的值
void FetchTestCase(VOLETestcase &testcase, std::string testcase_filename)
{
    std::ifstream fin;
    fin.open(testcase_filename, std::ios::binary);
    if (!fin)
    {
        std::cerr << testcase_filename << " open error" << std::endl;
        exit(1);
    }
    fin >> testcase.N_item;
    // fin >> testcase.t;
    fin >> testcase.delta;
    testcase.vec_B.resize(testcase.N_item);
    fin >> testcase.vec_B;
    fin.close();
}
//首先明确VOLE的设置，服务器拥有向量A以及秘密值delta,客户端拥有向量B，并且两方分别计算C = A + delta * B，可以理解成都是128比特的block中的数据

int main()
{
    
	CRYPTO_Initialize(); //初始化print一些值出来

	PrintSplitLine('-'); 
    std::cout << "VOLE test begins >>>" << std::endl; 
    PrintSplitLine('-'); 
 
    // set instance size 设置初始的值，这里面delta作为一个常量，没有考虑它的值
    uint64_t N_item = uint64_t(pow(2, 20)); //这里面测试的就是VOLE的例子的个数
    uint64_t t = 397; //一个特定参数，值为397，可能关系到安全性或效率，决定了FPP以及有限域的大小
    
    //选择其中一方作为角色
    std::string testcase_filename = "vole.testcase"; 
    std::string party;
    std::cout << "please select your role between server and receiver (hint: first start server, then start client) ==> ";
    std::getline(std::cin, party);

   
    if (party == "server")
    {
        NetIO server_io("server", "", 8080);
        
        // generate vec_A, vec_C 创建两个Vec_A以及Vec_C
        std::vector<block> vec_A;
        std::vector<block> vec_C;
        auto start1 = std::chrono::steady_clock::now();//开始计算时间
        vec_A = VOLE::VOLE_A(server_io, N_item, vec_C, t);//调用VOLE_A函数执行服务器生成Vec_A
        auto end1 = std::chrono::steady_clock::now();
        
        // get delta and vec_B from testcase
        VOLETestcase testcase; 
        FetchTestCase(testcase, testcase_filename);
	block delta = testcase.delta;
        std::vector<block> vec_B = testcase.vec_B;//从客户端存的数据中读取客户端向量B
 	std::cout << "Item_num = " << N_item << std::endl; 
 	std::cout << "VOLE takes A"
             << ":" << std::chrono::duration<double, std::milli>(end1 - start1).count() << " ms" << std::endl;  
 	
 	
 	// calculate vec_C + vec_A*delta 客户端的计算量，本地计算Vec_C的值
        for (auto i = 0; i < N_item; ++i)
        {
        	vec_C[i] ^= VOLE::gf128_mul(delta,vec_A[i]);
        }
        
        // test if vec_B == vec_C + vec_A*delta
        if(Block::Compare(vec_B,vec_C)==true){
        	PrintSplitLine('-');
        	std::cout << "VOLE test succeeds" << std::endl; 
        }
        else
        {
        	PrintSplitLine('-');
        	std::cout << "VOLE test fails" << std::endl; 
        }
       
    }

    if (party == "client")
    {
        NetIO client_io("client", "127.0.0.1", 8080);
        
        // generate delta and vec_B
        std::vector<block> vec_B;
        PRG::Seed seed = PRG::SetSeed();
        block delta = PRG::GenRandomBlocks(seed, 1)[0];
        auto start1 = std::chrono::steady_clock::now();
        VOLE::VOLE_B(client_io ,N_item ,vec_B, delta, t);//执行客户端生成Vec_B
        auto end1 = std::chrono::steady_clock::now();
        std::cout << "Item_num = " << N_item << std::endl; 
        std::cout << "VOLE takes B"
            << ":" << std::chrono::duration<double, std::milli>(end1 - start1).count() << " ms" << std::endl;
        
        // save testcase for test
        VOLETestcase testcase; 
        testcase = GenTestCase(N_item); 
        testcase.delta = delta;
        testcase.vec_B = vec_B;
        SaveTestCase(testcase, testcase_filename);
    }

    PrintSplitLine('-'); 
    std::cout << "VOLE test ends >>>" << std::endl; 
    PrintSplitLine('-'); 
    
     CRYPTO_Finalize();       
	return 0; 
}
