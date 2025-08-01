cmake_minimum_required(VERSION 3.20)
project(Kunlun)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()

# YACL configuration
set(YACL_ROOT "/home/luck/xzy/rb-okvs-psi/Ultra/yacl")

# OpenSSL configuration
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(OPENSSL_LIBRARIES 
        /home/luck/xzy/OPRF/Kunlun/lib/lib64/libcrypto.a
        /home/luck/xzy/OPRF/Kunlun/lib/lib64/libssl.a)
    set(OPENSSL_INCLUDE_DIR /home/luck/xzy/OPRF/Kunlun/lib/include)
    set(OPENSSL_DIR /home/luck/xzy/OPRF/Kunlun/lib/lib64)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(OPENSSL_LIBRARIES 
        /usr/local/lib/libcrypto.a
        /usr/local/lib/libssl.a)
    set(OPENSSL_INCLUDE_DIR /usr/local/include)
    set(OPENSSL_DIR /usr/local/lib)
endif()

# Find OpenMP
find_package(OpenMP REQUIRED)

# Find required dependencies for YACL
find_package(fmt QUIET)
if(NOT fmt_FOUND)
    message(STATUS "fmt not found, trying to find manually...")
    find_path(FMT_INCLUDE_DIR fmt/format.h
        PATHS /usr/include /usr/local/include
        ${YACL_ROOT}/third_party/fmt/include
        NO_DEFAULT_PATH)
    find_library(FMT_LIBRARY
        NAMES fmt libfmt
        PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu
        ${YACL_ROOT}/third_party/fmt/lib
        NO_DEFAULT_PATH)
    if(FMT_INCLUDE_DIR AND FMT_LIBRARY)
        message(STATUS "Found fmt manually: ${FMT_LIBRARY}")
        set(fmt_FOUND TRUE)
    endif()
endif()

# Try to find YACL package
find_package(yacl QUIET PATHS ${YACL_ROOT} ${YACL_ROOT}/build)

# If YACL package not found, try to find libraries manually
if(NOT yacl_FOUND)
    message(STATUS "YACL package not found, trying manual configuration...")
    
    # Look for YACL libraries
    find_library(YACL_BASE_LIB 
        NAMES yacl_base libyacl_base
        PATHS ${YACL_ROOT}/lib ${YACL_ROOT}/build/lib 
        ${YACL_ROOT}/bazel-bin/yacl/base
        NO_DEFAULT_PATH)
        
    find_library(YACL_CRYPTO_LIB 
        NAMES yacl_crypto libyacl_crypto
        PATHS ${YACL_ROOT}/lib ${YACL_ROOT}/build/lib 
        ${YACL_ROOT}/bazel-bin/yacl/crypto
        NO_DEFAULT_PATH)
        
    find_library(YACL_KERNEL_LIB 
        NAMES yacl_kernel libyacl_kernel
        PATHS ${YACL_ROOT}/lib ${YACL_ROOT}/build/lib 
        ${YACL_ROOT}/bazel-bin/yacl/kernel
        NO_DEFAULT_PATH)
        
    # Check if we found any YACL libraries
    if(YACL_BASE_LIB OR YACL_CRYPTO_LIB OR YACL_KERNEL_LIB)
        message(STATUS "Found YACL libraries manually")
        if(YACL_BASE_LIB)
            message(STATUS "  YACL Base: ${YACL_BASE_LIB}")
        endif()
        if(YACL_CRYPTO_LIB)
            message(STATUS "  YACL Crypto: ${YACL_CRYPTO_LIB}")
        endif()
        if(YACL_KERNEL_LIB)
            message(STATUS "  YACL Kernel: ${YACL_KERNEL_LIB}")
        endif()
    else()
        message(WARNING "YACL libraries not found. Some functionality may be limited.")
    endif()
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -mpclmul -msse4.1 -fpic -Wno-deprecated-declarations -Wno-unused-result")

# Include directories
include_directories(
    ${OPENSSL_INCLUDE_DIR}
    /home/luck/xzy/OPRF/Kunlun/crypto
    /home/luck/xzy/OPRF/Kunlun  # Add Kunlun root for mpc includes
    /home/luck/Nolen/crx1/preLibrary/include
    /home/luck/Nolen/crx1/preLibrary/include/bandokvs
    /home/luck/Nolen/crx1/HashPSI/extra
    # Add YACL include directories
    ${YACL_ROOT}
    ${YACL_ROOT}/yacl
    ${YACL_ROOT}/bazel-bin  # For generated headers if using Bazel
)

# Add fmt include directory if found manually
if(FMT_INCLUDE_DIR)
    include_directories(${FMT_INCLUDE_DIR})
endif()

# Library directories
link_directories(
    ${OPENSSL_DIR}
    /home/luck/Nolen/lib
    /home/luck/xzy/OPRF/Kunlun/lib/lib64
)

# System information
include(CMakePrintHelpers)
foreach(key
  IN ITEMS
    NUMBER_OF_LOGICAL_CORES
    NUMBER_OF_PHYSICAL_CORES
    IS_64BIT
    HAS_SSE2
    OS_NAME
    OS_RELEASE
  )
  cmake_host_system_information(RESULT _${key} QUERY ${key})
  cmake_print_variables(_${key})
endforeach()

# Platform definitions
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(IS_LINUX 1)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(IS_MACOS 1)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(IS_WINDOWS 1)
endif()

# Configure header
configure_file(${PROJECT_SOURCE_DIR}/config/config.h.in ${PROJECT_SOURCE_DIR}/config/config.h @ONLY)

# Set output directory
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/build)

# Original PSI test executable
add_executable(psi_test psi_test.cpp)
target_link_libraries(psi_test 
    ${OPENSSL_LIBRARIES}
    OpenMP::OpenMP_CXX
    /home/luck/Nolen/crx1/preLibrary/lib/libgmssl.so
    /home/luck/Nolen/crx1/preLibrary/lib/libbandokvs.a
    /home/luck/Nolen/crx1/preLibrary/lib/libcryptoTools.a
)

# New test_okvs executable with YACL support
# First, let's define BandOKVS sources if they exist in the vole directory
file(GLOB BANDOKVS_SOURCES 
    "vole/bandokvs/*.cpp" 
    "vole/bandokvs/*.cc"
)

# If no sources found locally, we'll use the precompiled library
if(NOT BANDOKVS_SOURCES)
    message(STATUS "Using precompiled BandOKVS library")
    set(USE_PRECOMPILED_BANDOKVS TRUE)
endif()

add_executable(test_okvs test_silentOT.cpp ${BANDOKVS_SOURCES})

# Link libraries for test_okvs
target_link_libraries(test_okvs 
    ${OPENSSL_LIBRARIES}
    OpenMP::OpenMP_CXX
)

# Add BandOKVS library if using precompiled version
if(USE_PRECOMPILED_BANDOKVS)
    target_link_libraries(test_okvs 
        /home/luck/Nolen/crx1/preLibrary/lib/libbandokvs.a
        /home/luck/Nolen/crx1/preLibrary/lib/libcryptoTools.a
    )
endif()

# Link YACL libraries if found
if(yacl_FOUND)
    target_link_libraries(test_okvs yacl::yacl)
    message(STATUS "Linking with YACL package")
else()
    # Link manually found YACL libraries
    if(YACL_KERNEL_LIB)
        target_link_libraries(test_okvs ${YACL_KERNEL_LIB})
    endif()
    if(YACL_CRYPTO_LIB)
        target_link_libraries(test_okvs ${YACL_CRYPTO_LIB})
    endif()
    if(YACL_BASE_LIB)
        target_link_libraries(test_okvs ${YACL_BASE_LIB})
    endif()
    
    # Add fmt library
    if(fmt_FOUND)
        if(TARGET fmt::fmt)
            target_link_libraries(test_okvs fmt::fmt)
        elseif(FMT_LIBRARY)
            target_link_libraries(test_okvs ${FMT_LIBRARY})
        endif()
        message(STATUS "Linking with fmt")
    endif()
    
    # Add potential additional dependencies for YACL
    find_package(Protobuf QUIET)
    if(Protobuf_FOUND)
        target_link_libraries(test_okvs protobuf::libprotobuf)
        message(STATUS "Linking with Protobuf")
    endif()
    
    # Try to find and link absl (Abseil)
    find_package(absl QUIET)
    if(absl_FOUND)
        target_link_libraries(test_okvs 
            absl::base 
            absl::strings
            absl::synchronization
        )
        message(STATUS "Linking with Abseil")
    endif()
endif()

# Add standard system libraries
target_link_libraries(test_okvs 
    pthread
    m
    dl
    rt
)

# Compiler definitions for test_okvs
target_compile_definitions(test_okvs PRIVATE
    -DWITH_OPENSSL
    -D_GNU_SOURCE
)

# Print configuration summary
message(STATUS "=== Configuration Summary ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "YACL root: ${YACL_ROOT}")
message(STATUS "YACL found: ${yacl_FOUND}")
message(STATUS "OpenSSL include: ${OPENSSL_INCLUDE_DIR}")
message(STATUS "OpenSSL libraries: ${OPENSSL_LIBRARIES}")
message(STATUS "==============================")

# Library with PSI functionality (commented out as in original)
# add_library(psi_functions STATIC
#     /home/luck/Nolen/crx1/HashPSI/psi.cc
#     /home/luck/Nolen/crx1/HashPSI/main.cc
# )
# file(GLOB EXTRA_SOURCES "/home/luck/Nolen/crx1/HashPSI/extra/*.cpp")
# target_sources(psi_functions PRIVATE ${EXTRA_SOURCES})
# target_link_libraries(psi_functions ${OPENSSL_LIBRARIES} OpenMP::OpenMP_CXX)