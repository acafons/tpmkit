cmake_minimum_required(VERSION 3.20)

project(tpmkit_cmake_smoke LANGUAGES CXX)

add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../.." tpmkit-src)

add_executable(tpmkit_cmake_smoke smoke.cpp)
target_link_libraries(tpmkit_cmake_smoke PRIVATE tpmkit::tpmkit_testing)
