cmake_minimum_required(VERSION 3.20)

project(tpmkit_downstream_smoke LANGUAGES CXX)

set(_tpmkit_components)
if(TPMKIT_SMOKE_TESTING_ONLY)
    list(APPEND _tpmkit_components testing)
endif()
if(TPMKIT_SMOKE_REQUIRE_SPDLOG)
    list(APPEND _tpmkit_components spdlog)
endif()

if(_tpmkit_components)
    set(_tpmkit_find_components COMPONENTS ${_tpmkit_components})
endif()

if(DEFINED TPMKIT_SMOKE_PACKAGE_PATH)
    find_package(tpmkit CONFIG REQUIRED
        ${_tpmkit_find_components}
        PATHS "${TPMKIT_SMOKE_PACKAGE_PATH}"
        NO_DEFAULT_PATH
    )
else()
    find_package(tpmkit CONFIG REQUIRED ${_tpmkit_find_components})
endif()

if(NOT TPMKIT_SMOKE_TESTING_ONLY)
    add_executable(tpmkit_downstream_production production_smoke.cpp)
    target_link_libraries(tpmkit_downstream_production PRIVATE tpmkit::tpmkit)
endif()

if(TPMKIT_SMOKE_REQUIRE_TESTING OR TPMKIT_SMOKE_TESTING_ONLY)
    if(NOT TARGET tpmkit::tpmkit_testing)
        message(FATAL_ERROR "tpmkit::tpmkit_testing is required but was not exported")
    endif()

    add_executable(tpmkit_downstream_testing testing_smoke.cpp)
    target_link_libraries(tpmkit_downstream_testing PRIVATE tpmkit::tpmkit_testing)
elseif(TARGET tpmkit::tpmkit_testing)
    message(FATAL_ERROR "tpmkit::tpmkit_testing was exported unexpectedly")
endif()

if(TPMKIT_SMOKE_REQUIRE_SPDLOG)
    if(NOT TARGET tpmkit::tpmkit_spdlog)
        message(FATAL_ERROR "tpmkit::tpmkit_spdlog is required but was not exported")
    endif()
    if(NOT TARGET spdlog::spdlog)
        message(FATAL_ERROR "spdlog::spdlog was not found for the spdlog component")
    endif()

    add_executable(tpmkit_downstream_spdlog spdlog_smoke.cpp)
    target_link_libraries(tpmkit_downstream_spdlog
        PRIVATE
            tpmkit::tpmkit_spdlog
            spdlog::spdlog
    )
endif()

enable_testing()

if(TARGET tpmkit_downstream_production)
    add_test(NAME production_smoke COMMAND tpmkit_downstream_production)
endif()

if(TARGET tpmkit_downstream_testing)
    add_test(NAME testing_smoke COMMAND tpmkit_downstream_testing)
endif()

if(TARGET tpmkit_downstream_spdlog)
    add_test(NAME spdlog_smoke COMMAND tpmkit_downstream_spdlog)
endif()
