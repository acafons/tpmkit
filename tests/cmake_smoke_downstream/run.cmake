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

set(tpmkit_build_dir "${TPMKIT_BINARY_ROOT}/tpmkit-build")
set(downstream_build_dir "${TPMKIT_BINARY_ROOT}/downstream-build")
set(testing_failure_build_dir "${TPMKIT_BINARY_ROOT}/downstream-testing-required-build")
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
        -Dtpmkit_BUILD_TESTS=OFF
        -DTPMKIT_INSTALL_TESTING=${TPMKIT_INSTALL_TESTING}
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
