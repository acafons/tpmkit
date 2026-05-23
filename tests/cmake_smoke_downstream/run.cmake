if(NOT DEFINED TPMKIT_SOURCE_DIR)
    message(FATAL_ERROR "TPMKIT_SOURCE_DIR is required")
endif()

if(NOT DEFINED TPMKIT_BINARY_ROOT)
    message(FATAL_ERROR "TPMKIT_BINARY_ROOT is required")
endif()

if(NOT DEFINED TPMKIT_INSTALL_PREFIX)
    message(FATAL_ERROR "TPMKIT_INSTALL_PREFIX is required")
endif()

if(NOT DEFINED TPMKIT_INSTALL_TESTING)
    message(FATAL_ERROR "TPMKIT_INSTALL_TESTING is required")
endif()

if(NOT DEFINED TPMKIT_BUILD_SHARED_LIBS)
    set(TPMKIT_BUILD_SHARED_LIBS OFF)
endif()
if(NOT DEFINED TPMKIT_LOG_ADAPTER)
    set(TPMKIT_LOG_ADAPTER none)
endif()
string(TOLOWER "${TPMKIT_LOG_ADAPTER}" tpmkit_log_adapter)
set(tpmkit_builds_spdlog OFF)
if(tpmkit_log_adapter STREQUAL "spdlog")
    set(tpmkit_builds_spdlog ON)
endif()

set(tpmkit_build_dir "${TPMKIT_BINARY_ROOT}/tpmkit-build")
set(downstream_build_dir "${TPMKIT_BINARY_ROOT}/downstream-build")
set(testing_only_build_dir "${TPMKIT_BINARY_ROOT}/downstream-testing-only-build")
set(testing_failure_build_dir "${TPMKIT_BINARY_ROOT}/downstream-testing-required-build")
set(spdlog_failure_build_dir "${TPMKIT_BINARY_ROOT}/downstream-spdlog-required-build")
set(tpmkit_config_dir "${TPMKIT_INSTALL_PREFIX}/lib/cmake/tpmkit")

file(REMOVE_RECURSE
    "${TPMKIT_BINARY_ROOT}"
    "${TPMKIT_INSTALL_PREFIX}"
)

execute_process(
    COMMAND
        "${CMAKE_COMMAND}"
        -S "${TPMKIT_SOURCE_DIR}"
        -B "${tpmkit_build_dir}"
        -DCMAKE_BUILD_TYPE=Debug
        -DBUILD_SHARED_LIBS=${TPMKIT_BUILD_SHARED_LIBS}
        -Dtpmkit_BUILD_TESTS=OFF
        -DTPMKIT_INSTALL_TESTING=${TPMKIT_INSTALL_TESTING}
        -DTPMKIT_LOG_ADAPTER=${tpmkit_log_adapter}
    RESULT_VARIABLE result
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Configuring tpmkit for install failed")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${tpmkit_build_dir}" --parallel 2
    RESULT_VARIABLE result
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Building tpmkit for install failed")
endif()

execute_process(
    COMMAND
        "${CMAKE_COMMAND}"
        --install "${tpmkit_build_dir}"
        --prefix "${TPMKIT_INSTALL_PREFIX}"
    RESULT_VARIABLE result
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Installing tpmkit failed")
endif()

if(NOT EXISTS "${TPMKIT_INSTALL_PREFIX}/lib/cmake/tpmkit/tpmkitConfig.cmake")
    message(FATAL_ERROR "Installed package config is missing")
endif()

file(READ "${TPMKIT_INSTALL_PREFIX}/lib/cmake/tpmkit/tpmkitTargets.cmake" tpmkit_targets)
if(tpmkit_targets MATCHES "/usr/lib/libtss2-")
    message(FATAL_ERROR "Installed targets export container-local TSS library paths")
endif()
if(tpmkit_targets MATCHES "tpmkit_spdlog")
    message(FATAL_ERROR "Main installed targets export the optional spdlog adapter")
endif()

if(NOT EXISTS "${TPMKIT_INSTALL_PREFIX}/include/tpmkit/logging/logger.h")
    message(FATAL_ERROR "logger header was not installed under logging/")
endif()
if(NOT EXISTS "${TPMKIT_INSTALL_PREFIX}/include/tpmkit/logging/noop_logger.h")
    message(FATAL_ERROR "noop logger header was not installed under logging/")
endif()
set(tpmkit_legacy_include_dir "${TPMKIT_INSTALL_PREFIX}/include/tpmkit")
if(EXISTS "${tpmkit_legacy_include_dir}/logger.h")
    message(FATAL_ERROR "logger header was installed at the old top-level path")
endif()
if(EXISTS "${tpmkit_legacy_include_dir}/noop_logger.h")
    message(FATAL_ERROR "noop logger header was installed at the old top-level path")
endif()
if(EXISTS "${tpmkit_legacy_include_dir}/spdlog_api.h")
    message(FATAL_ERROR "spdlog API header was installed at the old top-level path")
endif()
if(EXISTS "${tpmkit_legacy_include_dir}/spdlog_logger.h")
    message(FATAL_ERROR "spdlog logger header was installed at the old top-level path")
endif()

if(tpmkit_builds_spdlog)
    if(NOT EXISTS "${TPMKIT_INSTALL_PREFIX}/include/tpmkit/logging/spdlog_api.h")
        message(FATAL_ERROR "spdlog API header was not installed")
    endif()
    if(NOT EXISTS "${TPMKIT_INSTALL_PREFIX}/include/tpmkit/logging/spdlog_logger.h")
        message(FATAL_ERROR "spdlog logger header was not installed")
    endif()
    if(NOT EXISTS "${TPMKIT_INSTALL_PREFIX}/lib/cmake/tpmkit/tpmkitSpdlogTargets.cmake")
        message(FATAL_ERROR "spdlog CMake targets were not installed")
    endif()
else()
    if(EXISTS "${TPMKIT_INSTALL_PREFIX}/include/tpmkit/logging/spdlog_api.h")
        message(FATAL_ERROR "spdlog API header was installed unexpectedly")
    endif()
    if(EXISTS "${TPMKIT_INSTALL_PREFIX}/include/tpmkit/logging/spdlog_logger.h")
        message(FATAL_ERROR "spdlog logger header was installed unexpectedly")
    endif()
    if(EXISTS "${TPMKIT_INSTALL_PREFIX}/lib/cmake/tpmkit/tpmkitSpdlogTargets.cmake")
        message(FATAL_ERROR "spdlog CMake targets were installed unexpectedly")
    endif()
endif()

if(TPMKIT_INSTALL_TESTING)
    if(NOT EXISTS "${TPMKIT_INSTALL_PREFIX}/include/tpmkit/testing/fake_tpm_context.h")
        message(FATAL_ERROR "Testing headers were not installed")
    endif()
else()
    if(EXISTS "${TPMKIT_INSTALL_PREFIX}/include/tpmkit/testing/fake_tpm_context.h")
        message(FATAL_ERROR "Testing headers were installed unexpectedly")
    endif()
endif()

execute_process(
    COMMAND
        "${CMAKE_COMMAND}"
        -S "${TPMKIT_SOURCE_DIR}/tests/cmake_smoke_downstream"
        -B "${downstream_build_dir}"
        -DTPMKIT_SMOKE_PACKAGE_PATH=${tpmkit_config_dir}
        -DTPMKIT_SMOKE_REQUIRE_TESTING=${TPMKIT_INSTALL_TESTING}
        -DTPMKIT_SMOKE_REQUIRE_SPDLOG=${tpmkit_builds_spdlog}
    RESULT_VARIABLE result
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Configuring downstream smoke project failed")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${downstream_build_dir}" --parallel 2
    RESULT_VARIABLE result
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Building downstream smoke project failed")
endif()

execute_process(
    COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${downstream_build_dir}" --output-on-failure
    RESULT_VARIABLE result
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Running downstream smoke project failed")
endif()

if(TPMKIT_INSTALL_TESTING)
    execute_process(
        COMMAND
            "${CMAKE_COMMAND}"
            -S "${TPMKIT_SOURCE_DIR}/tests/cmake_smoke_downstream"
            -B "${testing_only_build_dir}"
            -DTPMKIT_SMOKE_PACKAGE_PATH=${tpmkit_config_dir}
            -DTPMKIT_SMOKE_TESTING_ONLY=ON
            -DTPMKIT_SMOKE_REQUIRE_TESTING=ON
        RESULT_VARIABLE result
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Configuring testing-only downstream smoke project failed")
    endif()

    execute_process(
        COMMAND "${CMAKE_COMMAND}" --build "${testing_only_build_dir}" --parallel 2
        RESULT_VARIABLE result
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Building testing-only downstream smoke project failed")
    endif()

    execute_process(
        COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${testing_only_build_dir}" --output-on-failure
        RESULT_VARIABLE result
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Running testing-only downstream smoke project failed")
    endif()
endif()

if(NOT tpmkit_builds_spdlog)
    execute_process(
        COMMAND
            "${CMAKE_COMMAND}"
            -S "${TPMKIT_SOURCE_DIR}/tests/cmake_smoke_downstream"
            -B "${spdlog_failure_build_dir}"
            -DTPMKIT_SMOKE_PACKAGE_PATH=${tpmkit_config_dir}
            -DTPMKIT_SMOKE_REQUIRE_SPDLOG=ON
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(result EQUAL 0)
        message(FATAL_ERROR "Downstream spdlog-target configure succeeded unexpectedly")
    endif()

    string(CONCAT combined_output "${output}" "${error}")
    if(NOT combined_output MATCHES "spdlog")
        message(FATAL_ERROR "Missing-spdlog-target failure did not name spdlog")
    endif()
endif()

if(NOT TPMKIT_INSTALL_TESTING)
    execute_process(
        COMMAND
            "${CMAKE_COMMAND}"
            -S "${TPMKIT_SOURCE_DIR}/tests/cmake_smoke_downstream"
            -B "${testing_failure_build_dir}"
            -DTPMKIT_SMOKE_PACKAGE_PATH=${tpmkit_config_dir}
            -DTPMKIT_SMOKE_REQUIRE_TESTING=ON
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(result EQUAL 0)
        message(FATAL_ERROR "Downstream testing-target configure succeeded unexpectedly")
    endif()

    string(CONCAT combined_output "${output}" "${error}")
    if(NOT combined_output MATCHES "tpmkit::tpmkit_testing")
        message(FATAL_ERROR "Missing-testing-target failure did not name tpmkit::tpmkit_testing")
    endif()
endif()
