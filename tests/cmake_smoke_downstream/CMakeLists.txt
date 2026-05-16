cmake_minimum_required(VERSION 3.20)

project(tpmkit_downstream_smoke LANGUAGES CXX)

if(DEFINED TPMKIT_SMOKE_PACKAGE_PATH)
    find_package(tpmkit CONFIG REQUIRED
        PATHS "${TPMKIT_SMOKE_PACKAGE_PATH}"
        NO_DEFAULT_PATH
    )
else()
    find_package(tpmkit CONFIG REQUIRED)
endif()

add_executable(tpmkit_downstream_production production_smoke.cpp)
target_link_libraries(tpmkit_downstream_production PRIVATE tpmkit::tpmkit)

if(TPMKIT_SMOKE_REQUIRE_TESTING)
    if(NOT TARGET tpmkit::tpmkit_testing)
        message(FATAL_ERROR "tpmkit::tpmkit_testing is required but was not exported")
    endif()

    add_executable(tpmkit_downstream_testing testing_smoke.cpp)
    target_link_libraries(tpmkit_downstream_testing PRIVATE tpmkit::tpmkit_testing)
elseif(TARGET tpmkit::tpmkit_testing)
    message(FATAL_ERROR "tpmkit::tpmkit_testing was exported unexpectedly")
endif()

enable_testing()
add_test(NAME production_smoke COMMAND tpmkit_downstream_production)

if(TARGET tpmkit_downstream_testing)
    add_test(NAME testing_smoke COMMAND tpmkit_downstream_testing)
endif()
