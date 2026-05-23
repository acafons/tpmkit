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

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
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

if(MSVC)
    add_compile_options(/W4 /permissive-)

    if(tpmkit_WARNINGS_AS_ERRORS)
        add_compile_options(/WX)
    endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wconversion
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wnull-dereference
        -Wdouble-promotion
        -Wformat=2
    )

    if(tpmkit_WARNINGS_AS_ERRORS)
        add_compile_options(-Werror)
    endif()

    add_compile_options(
        $<$<CONFIG:Release>:-D_FORTIFY_SOURCE=2>
        $<$<CONFIG:Release>:-fstack-protector-strong>
        $<$<CONFIG:Release>:-fstack-clash-protection>
        $<$<CONFIG:Release>:-fPIE>
    )
    add_link_options(
        $<$<CONFIG:Release>:-pie>
        $<$<CONFIG:Release>:-Wl,-z,relro,-z,now,-z,noexecstack>
    )

    if(tpmkit_USE_ASAN)
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
    endif()

    if(tpmkit_USE_UBSAN)
        add_compile_options(-fsanitize=undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=undefined)
    endif()

    if(tpmkit_USE_TSAN)
        add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
        add_link_options(-fsanitize=thread)
    endif()

    if(TPMKIT_COVERAGE)
        add_compile_options(--coverage -O0 -g)
        add_link_options(--coverage)
    endif()
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
find_package(PkgConfig REQUIRED)

find_package(Microsoft.GSL CONFIG REQUIRED)
find_package(tl-expected CONFIG QUIET)
if(NOT TARGET tl::expected)
    find_path(TL_EXPECTED_INCLUDE_DIR NAMES tl/expected.hpp REQUIRED)
    add_library(tl_expected_headers INTERFACE)
    target_include_directories(tl_expected_headers INTERFACE "${TL_EXPECTED_INCLUDE_DIR}")
    add_library(tl::expected ALIAS tl_expected_headers)
endif()
set(TPMKIT_TSS2_REQUIRED_VERSION "4.1.3" CACHE STRING "Required TPM2 TSS package version.")
pkg_check_modules(TSS2_ESYS REQUIRED IMPORTED_TARGET "tss2-esys = ${TPMKIT_TSS2_REQUIRED_VERSION}")
pkg_check_modules(TSS2_TCTILDR REQUIRED IMPORTED_TARGET
    "tss2-tctildr = ${TPMKIT_TSS2_REQUIRED_VERSION}"
)
if(TPMKIT_LOG_ADAPTER STREQUAL "spdlog")
    find_package(spdlog CONFIG REQUIRED)
    if(NOT TARGET spdlog::spdlog)
        message(FATAL_ERROR "TPMKIT_LOG_ADAPTER=spdlog requires the spdlog::spdlog CMake target.")
    endif()
endif()

set(tpmkit_placeholder_source "${CMAKE_CURRENT_BINARY_DIR}/generated/tpmkit_empty.cpp")
file(WRITE "${tpmkit_placeholder_source}" "")
set(tpmkit_testing_placeholder_source "${CMAKE_CURRENT_BINARY_DIR}/generated/tpmkit_testing_empty.cpp")
file(WRITE "${tpmkit_testing_placeholder_source}" "")

# Production source files are attached by later PRD tasks with
# target_sources(tpmkit PRIVATE ...). The generated empty translation unit keeps
# this a normal STATIC/SHARED-capable library without adding repository sources.
add_library(tpmkit "${tpmkit_placeholder_source}")
add_library(tpmkit::tpmkit ALIAS tpmkit)
target_compile_features(tpmkit PUBLIC cxx_std_17)
target_sources(tpmkit
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/tpmkit/api.h>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/tpmkit/exception.h>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/tpmkit/logging/logger.h>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/tpmkit/logging/noop_logger.h>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/tpmkit/result.h>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/tpmkit/tpm_context.h>
    PRIVATE
        src/adapters/tpm2_esys/error_translation.cpp
        src/adapters/tpm2_esys/error_translation.h
        src/adapters/tpm2_esys/impl.h
        src/adapters/tpm2_esys/log_events.h
        src/adapters/tpm2_esys/tcti_loader.cpp
        src/adapters/tpm2_esys/tcti_loader.h
        src/adapters/tpm2_esys/tpm_context.cpp
)
target_include_directories(tpmkit
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(tpmkit
    PUBLIC
        Microsoft.GSL::GSL
        tl::expected
    PRIVATE
        PkgConfig::TSS2_ESYS
        PkgConfig::TSS2_TCTILDR
)
set_target_properties(tpmkit PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)
if(NOT BUILD_SHARED_LIBS)
    target_compile_definitions(tpmkit PUBLIC TPMKIT_STATIC_DEFINE)
endif()

# TPM2 TSS libraries stay PRIVATE implementation dependencies. Static-library
# consumers may still see them as link-only requirements, but TSS headers do
# not become part of the public include surface.

add_library(tpmkit_testing "${tpmkit_testing_placeholder_source}")
add_library(tpmkit::tpmkit_testing ALIAS tpmkit_testing)
target_compile_features(tpmkit_testing PUBLIC cxx_std_17)
target_sources(tpmkit_testing
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/tpmkit/testing/fake_tcti.h>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/tpmkit/testing/fake_tpm_context.h>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/tpmkit/testing/recording_logger.h>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/tpmkit/testing/testing_api.h>
    PRIVATE
        src/adapters/mock/fake_tcti.cpp
        src/adapters/mock/fake_tpm_context.cpp
        src/adapters/mock/recording_logger.cpp
)
target_include_directories(tpmkit_testing
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${TSS2_TCTILDR_INCLUDE_DIRS}
)
target_link_libraries(tpmkit_testing
    PUBLIC
        Microsoft.GSL::GSL
        tl::expected
)
set_target_properties(tpmkit_testing PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
if(NOT BUILD_SHARED_LIBS)
    target_compile_definitions(tpmkit_testing PUBLIC TPMKIT_STATIC_DEFINE)
endif()

if(TPMKIT_LOG_ADAPTER STREQUAL "spdlog")
    set(tpmkit_spdlog_placeholder_source "${CMAKE_CURRENT_BINARY_DIR}/generated/tpmkit_spdlog_empty.cpp")
    file(WRITE "${tpmkit_spdlog_placeholder_source}" "")
    add_library(tpmkit_spdlog "${tpmkit_spdlog_placeholder_source}")
    add_library(tpmkit::tpmkit_spdlog ALIAS tpmkit_spdlog)
    target_compile_features(tpmkit_spdlog PUBLIC cxx_std_17)
    target_sources(tpmkit_spdlog
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/tpmkit/logging/spdlog_api.h>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/tpmkit/logging/spdlog_logger.h>
        PRIVATE
            src/adapters/spdlog/spdlog_logger.cpp
    )
    target_include_directories(tpmkit_spdlog
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_link_libraries(tpmkit_spdlog
        PUBLIC
            tpmkit::tpmkit
        PRIVATE
            spdlog::spdlog
    )
    set_target_properties(tpmkit_spdlog PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )
    if(NOT BUILD_SHARED_LIBS)
        target_compile_definitions(tpmkit_spdlog PUBLIC TPMKIT_STATIC_DEFINE)
    endif()
endif()

if(TPMKIT_LOG_ADAPTER STREQUAL "stdio")
    set(tpmkit_stdio_placeholder_source "${CMAKE_CURRENT_BINARY_DIR}/generated/tpmkit_stdio_empty.cpp")
    file(WRITE "${tpmkit_stdio_placeholder_source}" "")
    add_library(tpmkit_stdio "${tpmkit_stdio_placeholder_source}")
    add_library(tpmkit::tpmkit_stdio ALIAS tpmkit_stdio)
    target_compile_features(tpmkit_stdio PUBLIC cxx_std_17)
    target_sources(tpmkit_stdio
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/tpmkit/logging/stdio_api.h>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/tpmkit/logging/stdio_logger.h>
        PRIVATE
            src/adapters/stdio/stdio_logger.cpp
    )
    target_include_directories(tpmkit_stdio
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_link_libraries(tpmkit_stdio
        PUBLIC
            tpmkit::tpmkit
    )
    set_target_properties(tpmkit_stdio PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )
    if(NOT BUILD_SHARED_LIBS)
        target_compile_definitions(tpmkit_stdio PUBLIC TPMKIT_STATIC_DEFINE)
    endif()
endif()

if(TPMKIT_BUILD_DOCS)
    find_package(Doxygen REQUIRED)

    set(tpmkit_doxyfile "${CMAKE_CURRENT_BINARY_DIR}/docs/Doxyfile")
    configure_file(
        docs/Doxyfile.in
        "${tpmkit_doxyfile}"
        @ONLY
    )

    set(DOXYGEN_PROJECT_NAME "${PROJECT_NAME}")
    set(DOXYGEN_PROJECT_NUMBER "${PROJECT_VERSION}")
    set(DOXYGEN_PROJECT_BRIEF "${PROJECT_DESCRIPTION}")
    set(DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/docs")
    set(DOXYGEN_RECURSIVE YES)
    set(DOXYGEN_FILE_PATTERNS "*.h")
    set(DOXYGEN_STRIP_FROM_PATH "${CMAKE_CURRENT_SOURCE_DIR}/include")
    set(DOXYGEN_GENERATE_HTML YES)
    set(DOXYGEN_HTML_OUTPUT html)
    set(DOXYGEN_GENERATE_LATEX NO)
    set(DOXYGEN_GENERATE_XML NO)
    set(DOXYGEN_HAVE_DOT NO)
    set(DOXYGEN_CLASS_GRAPH NO)
    set(DOXYGEN_CALL_GRAPH NO)
    set(DOXYGEN_CALLER_GRAPH NO)
    set(DOXYGEN_COLLABORATION_GRAPH NO)
    set(DOXYGEN_INCLUDE_GRAPH NO)
    set(DOXYGEN_INCLUDED_BY_GRAPH NO)
    set(DOXYGEN_EXTRACT_ALL NO)
    set(DOXYGEN_EXTRACT_PRIVATE NO)
    set(DOXYGEN_EXTRACT_STATIC NO)
    set(DOXYGEN_HIDE_UNDOC_MEMBERS NO)
    set(DOXYGEN_HIDE_UNDOC_CLASSES NO)
    set(DOXYGEN_WARNINGS YES)
    set(DOXYGEN_WARN_IF_UNDOCUMENTED YES)
    set(DOXYGEN_WARN_IF_DOC_ERROR YES)
    set(DOXYGEN_WARN_AS_ERROR YES)
    set(DOXYGEN_WARN_FORMAT "$file:$line: $text")
    set(DOXYGEN_MARKDOWN_SUPPORT YES)
    set(DOXYGEN_AUTOLINK_SUPPORT YES)
    set(DOXYGEN_BUILTIN_STL_SUPPORT YES)
    set(DOXYGEN_ENABLE_PREPROCESSING YES)
    set(DOXYGEN_MACRO_EXPANSION NO)
    set(DOXYGEN_SKIP_FUNCTION_MACROS YES)
    set(DOXYGEN_ALIASES
        "thread_safety=\\par Thread Safety"
        "exception_safety=\\par Exception Safety"
    )

    doxygen_add_docs(docs
        "${CMAKE_CURRENT_SOURCE_DIR}/include/tpmkit"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Generate tpmkit API reference"
    )
endif()

if(TPMKIT_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

install(TARGETS tpmkit
    EXPORT tpmkitTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(TARGET tpmkit_spdlog)
    install(TARGETS tpmkit_spdlog
        EXPORT tpmkitSpdlogTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

if(TARGET tpmkit_stdio)
    install(TARGETS tpmkit_stdio
        EXPORT tpmkitStdioTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

if(TPMKIT_INSTALL_TESTING)
    install(TARGETS tpmkit_testing
        EXPORT tpmkitTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

install(
    DIRECTORY include/tpmkit
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
    PATTERN "testing" EXCLUDE
    PATTERN "spdlog_api.h" EXCLUDE
    PATTERN "spdlog_logger.h" EXCLUDE
    PATTERN "stdio_api.h" EXCLUDE
    PATTERN "stdio_logger.h" EXCLUDE
)

if(TARGET tpmkit_spdlog)
    install(FILES
        include/tpmkit/logging/spdlog_api.h
        include/tpmkit/logging/spdlog_logger.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/tpmkit/logging
    )
endif()

if(TARGET tpmkit_stdio)
    install(FILES
        include/tpmkit/logging/stdio_api.h
        include/tpmkit/logging/stdio_logger.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/tpmkit/logging
    )
endif()

if(TPMKIT_INSTALL_TESTING)
    install(DIRECTORY include/tpmkit/testing
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/tpmkit
        FILES_MATCHING PATTERN "*.h"
    )
endif()

install(EXPORT tpmkitTargets
    FILE tpmkitTargets.cmake
    NAMESPACE tpmkit::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tpmkit
)

if(TARGET tpmkit_spdlog)
    install(EXPORT tpmkitSpdlogTargets
        FILE tpmkitSpdlogTargets.cmake
        NAMESPACE tpmkit::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tpmkit
    )
endif()

if(TARGET tpmkit_stdio)
    install(EXPORT tpmkitStdioTargets
        FILE tpmkitStdioTargets.cmake
        NAMESPACE tpmkit::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tpmkit
    )
endif()

configure_package_config_file(
    cmake/tpmkitConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/tpmkitConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tpmkit
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/tpmkitConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/tpmkitConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/tpmkitConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tpmkit
)

if(tpmkit_BUILD_TESTS)
    enable_testing()

    find_package(GTest CONFIG REQUIRED)
    include(GoogleTest)

    function(tpmkit_discover_gtests target)
        set(test_labels "${ARGN}")
        string(REPLACE ";" "\\;" escaped_test_labels "${test_labels}")
        gtest_discover_tests(${target}
            PROPERTIES
                TIMEOUT 30
                LABELS "${escaped_test_labels}"
        )
    endfunction()

    set(tpmkit_unit_public_api_sources
        tests/unit/public_api/exception_header_smoke.cpp
        tests/unit/public_api/foundational_headers_smoke.cpp
        tests/unit/public_api/foundational_types_test.cpp
        tests/unit/public_api/logger_header_smoke.cpp
        tests/unit/public_api/noop_logger_header_smoke.cpp
        tests/unit/public_api/result_header_smoke.cpp
        tests/unit/public_api/tpm_context_config_test.cpp
        tests/unit/public_api/tpm_context_header_self_contained.cpp
        tests/unit/public_api/tpm_context_header_smoke.cpp
    )
    if(TARGET tpmkit_spdlog)
        list(APPEND tpmkit_unit_public_api_sources
            tests/unit/public_api/spdlog_logger_header_self_contained.cpp
        )
    endif()
    if(TARGET tpmkit_stdio)
        list(APPEND tpmkit_unit_public_api_sources
            tests/unit/public_api/stdio_logger_header_self_contained.cpp
        )
    endif()

    add_executable(tpmkit_unit_public_api_tests
        ${tpmkit_unit_public_api_sources}
    )
    target_link_libraries(tpmkit_unit_public_api_tests
        PRIVATE
            GTest::gtest_main
            tpmkit::tpmkit
    )
    tpmkit_discover_gtests(tpmkit_unit_public_api_tests Unit PublicApi)

    add_executable(tpmkit_unit_common_tests
        tests/unit/common/escape_value_test.cpp
    )
    target_include_directories(tpmkit_unit_common_tests
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_link_libraries(tpmkit_unit_common_tests
        PRIVATE
            GTest::gtest_main
    )
    tpmkit_discover_gtests(tpmkit_unit_common_tests Unit Common)

    if(TARGET tpmkit_spdlog)
        add_executable(tpmkit_unit_spdlog_tests
            tests/unit/spdlog/spdlog_logger_test.cpp
        )
        target_link_libraries(tpmkit_unit_spdlog_tests
            PRIVATE
                GTest::gtest_main
                tpmkit::tpmkit_spdlog
                spdlog::spdlog
        )
        tpmkit_discover_gtests(tpmkit_unit_spdlog_tests Unit Adapter_spdlog)
    endif()

    if(TARGET tpmkit_stdio)
        add_executable(tpmkit_unit_stdio_tests
            tests/unit/stdio/stdio_logger_test.cpp
        )
        target_link_libraries(tpmkit_unit_stdio_tests
            PRIVATE
                GTest::gtest_main
                tpmkit::tpmkit_stdio
        )
        tpmkit_discover_gtests(tpmkit_unit_stdio_tests Unit Adapter_stdio)
    endif()

    add_executable(tpmkit_unit_testing_tests
        tests/unit/public_api/fake_tpm_context_header_self_contained.cpp
        tests/unit/public_api/recording_logger_header_self_contained.cpp
        tests/unit/testing/fake_tpm_context_test.cpp
        tests/unit/testing/recording_logger_test.cpp
        tests/unit/testing/tpm2_esys/fake_tcti_test.cpp
    )
    target_link_libraries(tpmkit_unit_testing_tests
        PRIVATE
            GTest::gtest_main
            tpmkit::tpmkit
            tpmkit::tpmkit_testing
            PkgConfig::TSS2_ESYS
    )
    tpmkit_discover_gtests(tpmkit_unit_testing_tests Unit Testing)

    add_executable(tpmkit_unit_tpm2_esys_tests
        tests/unit/tpm2_esys/error_translation_test.cpp
        tests/unit/tpm2_esys/log_events_test.cpp
        tests/unit/tpm2_esys/tcti_loader_test.cpp
        tests/unit/tpm2_esys/tpm_context_lifecycle_test.cpp
    )
    target_include_directories(tpmkit_unit_tpm2_esys_tests
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_link_libraries(tpmkit_unit_tpm2_esys_tests
        PRIVATE
            GTest::gtest_main
            tpmkit::tpmkit
            tpmkit::tpmkit_testing
            PkgConfig::TSS2_ESYS
            PkgConfig::TSS2_TCTILDR
    )
    tpmkit_discover_gtests(tpmkit_unit_tpm2_esys_tests Unit Adapter_tpm2_esys Security)

    add_executable(tpmkit_contract_logger_tests
        tests/contract/logger_port_test.cpp
    )
    target_link_libraries(tpmkit_contract_logger_tests
        PRIVATE
            GTest::gtest_main
            tpmkit::tpmkit
            tpmkit::tpmkit_testing
    )
    if(TARGET tpmkit_spdlog)
        target_compile_definitions(tpmkit_contract_logger_tests
            PRIVATE TPMKIT_HAS_SPDLOG_ADAPTER
        )
        target_link_libraries(tpmkit_contract_logger_tests
            PRIVATE tpmkit::tpmkit_spdlog spdlog::spdlog
        )
    endif()
    if(TARGET tpmkit_stdio)
        target_compile_definitions(tpmkit_contract_logger_tests
            PRIVATE TPMKIT_HAS_STDIO_ADAPTER
        )
        target_link_libraries(tpmkit_contract_logger_tests
            PRIVATE tpmkit::tpmkit_stdio
        )
    endif()
    tpmkit_discover_gtests(tpmkit_contract_logger_tests Contract)

    add_executable(tpmkit_integration_tpm2_esys_tests
        tests/integration/tpm2_esys/tcti_loader_test.cpp
        tests/integration/tpm2_esys/tpm_context_swtpm_test.cpp
    )
    target_include_directories(tpmkit_integration_tpm2_esys_tests
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_link_libraries(tpmkit_integration_tpm2_esys_tests
        PRIVATE
            GTest::gtest_main
            tpmkit::tpmkit
            tpmkit::tpmkit_testing
            PkgConfig::TSS2_ESYS
            PkgConfig::TSS2_TCTILDR
    )
    if(TARGET tpmkit_spdlog)
        target_compile_definitions(tpmkit_integration_tpm2_esys_tests
            PRIVATE TPMKIT_HAS_SPDLOG
        )
        target_link_libraries(tpmkit_integration_tpm2_esys_tests
            PRIVATE tpmkit::tpmkit_spdlog spdlog::spdlog
        )
    endif()
    if(TARGET tpmkit_stdio)
        target_compile_definitions(tpmkit_integration_tpm2_esys_tests
            PRIVATE TPMKIT_HAS_STDIO
        )
        target_link_libraries(tpmkit_integration_tpm2_esys_tests
            PRIVATE tpmkit::tpmkit_stdio
        )
    endif()
    gtest_discover_tests(tpmkit_integration_tpm2_esys_tests
        PROPERTIES
            TIMEOUT 30
            LABELS "Integration\\;Integration_tpm2_esys\\;Security"
            FIXTURES_REQUIRED tpm_stack
    )

    add_test(
        NAME tpm_stack_start
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/tpm-start.sh
    )
    set_tests_properties(tpm_stack_start
        PROPERTIES
            FIXTURES_SETUP tpm_stack
            LABELS "Integration;Integration_tpm2_esys"
    )

    add_test(
        NAME tpm_stack_stop
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/tpm-stop.sh
    )
    set_tests_properties(tpm_stack_stop
        PROPERTIES
            FIXTURES_CLEANUP tpm_stack
            LABELS "Integration;Integration_tpm2_esys"
    )

    add_test(
        NAME test_policy_guard
        COMMAND
            ${CMAKE_COMMAND}
            -DTPMKIT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check-test-policy.cmake
    )
    set_tests_properties(test_policy_guard
        PROPERTIES
            LABELS "Policy;Testing"
    )

    add_test(
        NAME cmake_tss_discovery_guard
        COMMAND
            ${CMAKE_COMMAND}
            -DTPMKIT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -DTPMKIT_BINARY_ROOT=${CMAKE_CURRENT_BINARY_DIR}/cmake_tss_discovery_guard
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake_tss_discovery_guard/run.cmake
    )
    set_tests_properties(cmake_tss_discovery_guard
        PROPERTIES
            LABELS "Policy;Testing"
    )

    add_test(
        NAME cmake_smoke_downstream_install_without_testing
        COMMAND
            ${CMAKE_COMMAND}
            -DTPMKIT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -DTPMKIT_BINARY_ROOT=${CMAKE_CURRENT_BINARY_DIR}/cmake_smoke_downstream/off
            -DTPMKIT_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/cmake_smoke_downstream/install-off
            -DTPMKIT_INSTALL_TESTING=OFF
            -DTPMKIT_LOG_ADAPTER=${TPMKIT_LOG_ADAPTER}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake_smoke_downstream/run.cmake
    )

    add_test(
        NAME cmake_smoke_downstream_install_with_testing
        COMMAND
            ${CMAKE_COMMAND}
            -DTPMKIT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -DTPMKIT_BINARY_ROOT=${CMAKE_CURRENT_BINARY_DIR}/cmake_smoke_downstream/on
            -DTPMKIT_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/cmake_smoke_downstream/install-on
            -DTPMKIT_INSTALL_TESTING=ON
            -DTPMKIT_LOG_ADAPTER=${TPMKIT_LOG_ADAPTER}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake_smoke_downstream/run.cmake
    )

    add_test(
        NAME cmake_smoke_downstream_shared_install_without_testing
        COMMAND
            ${CMAKE_COMMAND}
            -DTPMKIT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -DTPMKIT_BINARY_ROOT=${CMAKE_CURRENT_BINARY_DIR}/cmake_smoke_downstream/shared-off
            -DTPMKIT_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/cmake_smoke_downstream/install-shared-off
            -DTPMKIT_INSTALL_TESTING=OFF
            -DTPMKIT_BUILD_SHARED_LIBS=ON
            -DTPMKIT_LOG_ADAPTER=${TPMKIT_LOG_ADAPTER}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake_smoke_downstream/run.cmake
    )

    add_test(
        NAME cmake_smoke_downstream_install_without_spdlog
        COMMAND
            ${CMAKE_COMMAND}
            -DTPMKIT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -DTPMKIT_BINARY_ROOT=${CMAKE_CURRENT_BINARY_DIR}/cmake_smoke_downstream/spdlog-off
            -DTPMKIT_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/cmake_smoke_downstream/install-spdlog-off
            -DTPMKIT_INSTALL_TESTING=OFF
            -DTPMKIT_LOG_ADAPTER=none
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake_smoke_downstream/run.cmake
    )
endif()
