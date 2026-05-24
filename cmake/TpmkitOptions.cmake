include_guard(GLOBAL)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(tpmkit_IS_TOP_LEVEL ON)
else()
    set(tpmkit_IS_TOP_LEVEL OFF)
endif()

option(tpmkit_USE_ASAN "Enable AddressSanitizer instrumentation." OFF)
option(tpmkit_USE_UBSAN "Enable UndefinedBehaviorSanitizer instrumentation." OFF)
option(tpmkit_USE_TSAN "Enable ThreadSanitizer instrumentation." OFF)
option(tpmkit_WARNINGS_AS_ERRORS "Treat compiler warnings as errors." OFF)
option(tpmkit_BUILD_TESTS "Build tpmkit tests." "${tpmkit_IS_TOP_LEVEL}")
option(TPMKIT_COVERAGE "Enable coverage instrumentation for Debug builds." OFF)
option(TPMKIT_BUILD_DOCS "Build API reference documentation with Doxygen." OFF)
option(TPMKIT_INSTALL_TESTING "Install the tpmkit_testing target export for downstream test utilities." OFF)

set(TPMKIT_LOG_ADAPTER "none" CACHE STRING "Logger adapter to build (none, spdlog, stdio).")
string(TOLOWER "${TPMKIT_LOG_ADAPTER}" TPMKIT_LOG_ADAPTER)
set(TPMKIT_LOG_ADAPTER "${TPMKIT_LOG_ADAPTER}" CACHE STRING "Logger adapter to build (none, spdlog, stdio)." FORCE)
set_property(CACHE TPMKIT_LOG_ADAPTER PROPERTY STRINGS none spdlog stdio)
if(NOT TPMKIT_LOG_ADAPTER MATCHES "^(none|spdlog|stdio)$")
    message(FATAL_ERROR "TPMKIT_LOG_ADAPTER=${TPMKIT_LOG_ADAPTER} is invalid. Valid: none, spdlog, stdio.")
endif()

set(tpmkit_BUILD_EXAMPLES_DEFAULT ON)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(tpmkit_BUILD_EXAMPLES_DEFAULT OFF)
endif()
option(TPMKIT_BUILD_EXAMPLES "Build tpmkit example programs." "${tpmkit_BUILD_EXAMPLES_DEFAULT}")

if(tpmkit_USE_TSAN AND (tpmkit_USE_ASAN OR tpmkit_USE_UBSAN))
    message(FATAL_ERROR "tpmkit_USE_TSAN is mutually exclusive with tpmkit_USE_ASAN and tpmkit_USE_UBSAN.")
endif()

if(TPMKIT_COVERAGE AND (tpmkit_USE_ASAN OR tpmkit_USE_UBSAN OR tpmkit_USE_TSAN))
    message(FATAL_ERROR "TPMKIT_COVERAGE is mutually exclusive with sanitizer instrumentation.")
endif()

if(TPMKIT_COVERAGE AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(FATAL_ERROR "TPMKIT_COVERAGE is supported for Debug builds only.")
endif()
