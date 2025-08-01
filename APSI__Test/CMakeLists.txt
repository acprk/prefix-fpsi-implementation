cmake_minimum_required(VERSION 3.13)
project(APSI_Distance_PSI VERSION 1.0 LANGUAGES CXX)

# 设置 C++ 标准，与 APSI 保持一致
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # 禁用编译器特定扩展，确保标准兼容

# 设置编译选项，添加 -pthread 以支持多线程
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O3 -pthread")

# 设置 APSI 库路径
set(APSI_ROOT_DIR "/home/luck/xzy/intPSI/APSI/Slib")
set(APSI_INCLUDE_DIR "${APSI_ROOT_DIR}/include")
set(APSI_LIB_DIR "${APSI_ROOT_DIR}/lib")

# 检查 APSI 库是否存在
if(NOT EXISTS "${APSI_INCLUDE_DIR}")
    message(FATAL_ERROR "APSI include directory not found: ${APSI_INCLUDE_DIR}")
endif()
if(NOT EXISTS "${APSI_LIB_DIR}")
    message(FATAL_ERROR "APSI library directory not found: ${APSI_LIB_DIR}")
endif()

# 设置包含目录，与 APSI 的 target_include_directories 一致
include_directories(${APSI_INCLUDE_DIR})
include_directories(${APSI_INCLUDE_DIR}/APSI-0.12)
include_directories(/home/luck/xzy/intPSI/APSI/Prelib/SEALinstall/include)
include_directories(/home/luck/xzy/intPSI/APSI/Prelib/Kukuinstall/include)
include_directories(/home/luck/xzy/intPSI/APSI/Prelib/jsoncppinstall/include)

# 查找必要的库
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

# 显示OpenSSL信息
message(STATUS "Found OpenSSL: ${OPENSSL_VERSION}")
message(STATUS "OpenSSL include dir: ${OPENSSL_INCLUDE_DIR}")
message(STATUS "OpenSSL libraries: ${OPENSSL_LIBRARIES}")

# 查找 libstdc++，优先使用 GCC 9.x 的版本
find_library(STDCXX_LIBRARY NAMES stdc++ libstdc++ PATHS /usr/lib/gcc/x86_64-linux-gnu/9 NO_DEFAULT_PATH)
if(NOT STDCXX_LIBRARY)
    message(FATAL_ERROR "libstdc++ not found in /usr/lib/gcc/x86_64-linux-gnu/9")
endif()
message(STATUS "Found libstdc++: ${STDCXX_LIBRARY}")

# 查找 libpthread
find_library(PTHREAD_LIBRARY NAMES pthread libpthread)
if(NOT PTHREAD_LIBRARY)
    message(FATAL_ERROR "libpthread not found")
endif()
message(STATUS "Found libpthread: ${PTHREAD_LIBRARY}")

# 设置库链接列表，遵循 APSI 的链接顺序，添加 OpenSSL
set(APSI_LIBRARIES
    ${APSI_LIB_DIR}/libapsi-0.12.a
    /home/luck/xzy/intPSI/APSI/Prelib/jsoncppinstall/lib/libjsoncpp.a
    /home/luck/xzy/intPSI/APSI/Prelib/Kukuinstall/lib/libkuku-2.1.a
    /home/luck/xzy/intPSI/APSI/Prelib/SEALinstall/lib/libseal-4.1.a
    /home/luck/xzy/intPSI/APSI/Prelib/SEAL/build/lib/libzstd.a
    /home/luck/xzy/intPSI/APSI/Prelib/SEAL/build/lib/libz.a
    ${OPENSSL_LIBRARIES}  # 添加 OpenSSL 库
    ${PTHREAD_LIBRARY}
    ${STDCXX_LIBRARY}
)

# 数据生成器
add_executable(generate_datasets src/ip_gen.cpp)
target_compile_features(generate_datasets PRIVATE cxx_std_17)

# ipv4数据生成器
add_executable(ipv4_generator src/ipv4_generator.cpp)
target_compile_features(generate_datasets PRIVATE cxx_std_17)

# 数据编码器
add_executable(encode_data src/encode.cpp)
target_compile_features(encode_data PRIVATE cxx_std_17)

# 数据编码器
add_executable(prefixencode src/prefixencode.cpp)
target_compile_features(encode_data PRIVATE cxx_std_17)

# APSI 求交程序
add_executable(apsi_intersection src/psi.cpp)
target_include_directories(apsi_intersection PRIVATE ${OPENSSL_INCLUDE_DIR})  # 添加 OpenSSL 头文件路径
target_link_libraries(apsi_intersection PRIVATE ${APSI_LIBRARIES})
target_compile_features(apsi_intersection PRIVATE cxx_std_17)
target_compile_definitions(apsi_intersection PRIVATE
    APSI_USE_LOG4CPLUS=0
    APSI_USE_ZMQ=0
)

# 创建数据和结果目录
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/data)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/results)

# 安装规则
install(TARGETS generate_datasets encode_data apsi_intersection
    RUNTIME DESTINATION bin)

# 自定义目标：运行完整流程
add_custom_target(run_all
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/generate_datasets
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/encode_data  
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/apsi_intersection
    DEPENDS generate_datasets encode_data apsi_intersection
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running complete APSI distance PSI pipeline"
)

# 自定义目标：清理数据
add_custom_target(clean_data
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_CURRENT_BINARY_DIR}/data
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_CURRENT_BINARY_DIR}/results
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/data
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/results
    COMMENT "Cleaning data and results directories"
)

# 显示配置信息
message(STATUS "APSI Distance PSI Configuration:")
message(STATUS "  APSI Include Dir: ${APSI_INCLUDE_DIR}")
message(STATUS "  APSI Library Dir: ${APSI_LIB_DIR}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  OpenSSL Version: ${OPENSSL_VERSION}")
message(STATUS "  Found libraries: ${APSI_LIBRARIES}")