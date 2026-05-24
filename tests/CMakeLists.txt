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
    unit/public_api/exception_header_smoke.cpp
    unit/public_api/foundational_headers_smoke.cpp
    unit/public_api/foundational_types_test.cpp
    unit/public_api/logger_header_smoke.cpp
    unit/public_api/noop_logger_header_smoke.cpp
    unit/public_api/result_header_smoke.cpp
    unit/public_api/tpm_context_config_test.cpp
    unit/public_api/tpm_context_header_self_contained.cpp
    unit/public_api/tpm_context_header_smoke.cpp
)
if(TARGET tpmkit_spdlog)
    list(APPEND tpmkit_unit_public_api_sources
        unit/public_api/spdlog_logger_header_self_contained.cpp
    )
endif()
if(TARGET tpmkit_stdio)
    list(APPEND tpmkit_unit_public_api_sources
        unit/public_api/stdio_logger_header_self_contained.cpp
    )
endif()

add_executable(tpmkit_unit_public_api_tests
    ${tpmkit_unit_public_api_sources}
)
tpmkit_configure_executable_target(tpmkit_unit_public_api_tests)
target_link_libraries(tpmkit_unit_public_api_tests
    PRIVATE
        GTest::gtest_main
        tpmkit::tpmkit
)
tpmkit_discover_gtests(tpmkit_unit_public_api_tests Unit PublicApi)

add_executable(tpmkit_unit_common_tests
    unit/common/escape_value_test.cpp
)
tpmkit_configure_executable_target(tpmkit_unit_common_tests)
target_include_directories(tpmkit_unit_common_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src"
)
target_link_libraries(tpmkit_unit_common_tests
    PRIVATE
        GTest::gtest_main
)
tpmkit_discover_gtests(tpmkit_unit_common_tests Unit Common)

if(TARGET tpmkit_spdlog)
    add_executable(tpmkit_unit_spdlog_tests
        unit/logging/spdlog/spdlog_logger_test.cpp
    )
    tpmkit_configure_executable_target(tpmkit_unit_spdlog_tests)
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
        unit/logging/stdio/stdio_logger_test.cpp
    )
    tpmkit_configure_executable_target(tpmkit_unit_stdio_tests)
    target_link_libraries(tpmkit_unit_stdio_tests
        PRIVATE
            GTest::gtest_main
            tpmkit::tpmkit_stdio
    )
    tpmkit_discover_gtests(tpmkit_unit_stdio_tests Unit Adapter_stdio)
endif()

add_executable(tpmkit_unit_testing_tests
    unit/public_api/fake_tpm_context_header_self_contained.cpp
    unit/public_api/recording_logger_header_self_contained.cpp
    unit/testing/fake_tpm_context_test.cpp
    unit/testing/recording_logger_test.cpp
    unit/testing/tpm2_esys/fake_tcti_test.cpp
)
tpmkit_configure_executable_target(tpmkit_unit_testing_tests)
target_link_libraries(tpmkit_unit_testing_tests
    PRIVATE
        GTest::gtest_main
        tpmkit::tpmkit
        tpmkit::tpmkit_testing
        PkgConfig::TSS2_ESYS
)
tpmkit_discover_gtests(tpmkit_unit_testing_tests Unit Testing)

add_executable(tpmkit_unit_tpm2_esys_tests
    unit/tpm2_esys/error_translation_test.cpp
    unit/tpm2_esys/log_events_test.cpp
    unit/tpm2_esys/tcti_loader_test.cpp
    unit/tpm2_esys/tpm_context_lifecycle_test.cpp
)
tpmkit_configure_executable_target(tpmkit_unit_tpm2_esys_tests)
target_include_directories(tpmkit_unit_tpm2_esys_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}"
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
    contract/logger_port_test.cpp
)
tpmkit_configure_executable_target(tpmkit_contract_logger_tests)
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
    integration/tpm2_esys/tcti_loader_test.cpp
    integration/tpm2_esys/tpm_context_swtpm_test.cpp
)
tpmkit_configure_executable_target(tpmkit_integration_tpm2_esys_tests)
target_include_directories(tpmkit_integration_tpm2_esys_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}"
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
    COMMAND ${PROJECT_SOURCE_DIR}/scripts/tpm-start.sh
)
set_tests_properties(tpm_stack_start
    PROPERTIES
        FIXTURES_SETUP tpm_stack
        LABELS "Integration;Integration_tpm2_esys"
)

add_test(
    NAME tpm_stack_stop
    COMMAND ${PROJECT_SOURCE_DIR}/scripts/tpm-stop.sh
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
        -DTPMKIT_SOURCE_DIR=${PROJECT_SOURCE_DIR}
        -P ${PROJECT_SOURCE_DIR}/cmake/check-test-policy.cmake
)
set_tests_properties(test_policy_guard
    PROPERTIES
        LABELS "Policy;Testing"
)

add_test(
    NAME cmake_tss_discovery_guard
    COMMAND
        ${CMAKE_COMMAND}
        -DTPMKIT_SOURCE_DIR=${PROJECT_SOURCE_DIR}
        -DTPMKIT_BINARY_ROOT=${PROJECT_BINARY_DIR}/cmake_tss_discovery_guard
        -P ${PROJECT_SOURCE_DIR}/tests/cmake_tss_discovery_guard/run.cmake
)
set_tests_properties(cmake_tss_discovery_guard
    PROPERTIES
        LABELS "Policy;Testing"
)

add_test(
    NAME cmake_smoke_downstream_install_without_testing
    COMMAND
        ${CMAKE_COMMAND}
        -DTPMKIT_SOURCE_DIR=${PROJECT_SOURCE_DIR}
        -DTPMKIT_BINARY_ROOT=${PROJECT_BINARY_DIR}/cmake_smoke_downstream/off
        -DTPMKIT_INSTALL_PREFIX=${PROJECT_BINARY_DIR}/cmake_smoke_downstream/install-off
        -DTPMKIT_INSTALL_TESTING=OFF
        -DTPMKIT_LOG_ADAPTER=${TPMKIT_LOG_ADAPTER}
        -P ${PROJECT_SOURCE_DIR}/tests/cmake_smoke_downstream/run.cmake
)

add_test(
    NAME cmake_smoke_downstream_install_with_testing
    COMMAND
        ${CMAKE_COMMAND}
        -DTPMKIT_SOURCE_DIR=${PROJECT_SOURCE_DIR}
        -DTPMKIT_BINARY_ROOT=${PROJECT_BINARY_DIR}/cmake_smoke_downstream/on
        -DTPMKIT_INSTALL_PREFIX=${PROJECT_BINARY_DIR}/cmake_smoke_downstream/install-on
        -DTPMKIT_INSTALL_TESTING=ON
        -DTPMKIT_LOG_ADAPTER=${TPMKIT_LOG_ADAPTER}
        -P ${PROJECT_SOURCE_DIR}/tests/cmake_smoke_downstream/run.cmake
)

add_test(
    NAME cmake_smoke_downstream_shared_install_without_testing
    COMMAND
        ${CMAKE_COMMAND}
        -DTPMKIT_SOURCE_DIR=${PROJECT_SOURCE_DIR}
        -DTPMKIT_BINARY_ROOT=${PROJECT_BINARY_DIR}/cmake_smoke_downstream/shared-off
        -DTPMKIT_INSTALL_PREFIX=${PROJECT_BINARY_DIR}/cmake_smoke_downstream/install-shared-off
        -DTPMKIT_INSTALL_TESTING=OFF
        -DTPMKIT_BUILD_SHARED_LIBS=ON
        -DTPMKIT_LOG_ADAPTER=${TPMKIT_LOG_ADAPTER}
        -P ${PROJECT_SOURCE_DIR}/tests/cmake_smoke_downstream/run.cmake
)

add_test(
    NAME cmake_smoke_downstream_install_without_spdlog
    COMMAND
        ${CMAKE_COMMAND}
        -DTPMKIT_SOURCE_DIR=${PROJECT_SOURCE_DIR}
        -DTPMKIT_BINARY_ROOT=${PROJECT_BINARY_DIR}/cmake_smoke_downstream/spdlog-off
        -DTPMKIT_INSTALL_PREFIX=${PROJECT_BINARY_DIR}/cmake_smoke_downstream/install-spdlog-off
        -DTPMKIT_INSTALL_TESTING=OFF
        -DTPMKIT_LOG_ADAPTER=none
        -P ${PROJECT_SOURCE_DIR}/tests/cmake_smoke_downstream/run.cmake
)
