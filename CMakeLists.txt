# Supported local invocations:
#   cmake --preset debug
#   cmake --preset release
#   cmake --preset asan
#   cmake --preset ubsan
#   cmake --preset tsan
#   cmake --preset coverage
# Equivalent direct configure examples:
#   cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug
#   cmake -S . -B build/asan -DCMAKE_BUILD_TYPE=Debug -Dtpmkit_USE_ASAN=ON
#   cmake -S . -B build/ubsan -DCMAKE_BUILD_TYPE=Debug -Dtpmkit_USE_UBSAN=ON
#   cmake -S . -B build/tsan -DCMAKE_BUILD_TYPE=Debug -Dtpmkit_USE_TSAN=ON
#   cmake -S . -B build/coverage -DCMAKE_BUILD_TYPE=Debug -DTPMKIT_COVERAGE=ON

cmake_minimum_required(VERSION 3.20)

project(
    tpmkit
    VERSION 0.1.0
    DESCRIPTION "C++17 TPM2 TSS wrapper library"
    LANGUAGES CXX
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(GNUInstallDirs)
include(TpmkitOptions)
include(TpmkitBuildOptions)
include(TpmkitDependencies)

add_subdirectory(src)

include(TpmkitDocs)

if(TPMKIT_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

include(TpmkitInstall)

if(tpmkit_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
