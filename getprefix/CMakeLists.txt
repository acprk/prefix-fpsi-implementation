cmake_minimum_required(VERSION 3.12)
project(PrefixExtraction)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O3")

# 创建可执行文件
add_executable(prefix_extraction prefix.cpp)
add_executable(ip_gen ip_gen.cpp)
add_executable(ip_prefix ip_prefix.cpp)
add_executable(ip_gendisjoint ip_gendisjoint.cpp)
add_executable(ipv6_gen ipv6_gen.cpp)

# 链接线程库（用于std::chrono等）
find_package(Threads REQUIRED)
target_link_libraries(prefix_extraction PRIVATE Threads::Threads)
target_link_libraries(ip_gen PRIVATE Threads::Threads)
target_link_libraries(ip_prefix PRIVATE Threads::Threads)
target_link_libraries(ip_gendisjoint PRIVATE Threads::Threads)
target_link_libraries(ipv6_gen PRIVATE Threads::Threads)