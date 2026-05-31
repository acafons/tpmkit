if(NOT DEFINED TPMKIT_SOURCE_DIR)
    message(FATAL_ERROR "TPMKIT_SOURCE_DIR is required")
endif()

if(NOT DEFINED TPMKIT_BINARY_ROOT)
    message(FATAL_ERROR "TPMKIT_BINARY_ROOT is required")
endif()

set(smoke_source_dir "${TPMKIT_BINARY_ROOT}/project")
set(good_pkg_root "${TPMKIT_BINARY_ROOT}/pkg-good")
set(bad_pkg_root "${TPMKIT_BINARY_ROOT}/pkg-bad")
set(good_build_dir "${TPMKIT_BINARY_ROOT}/build-good")
set(bad_build_dir "${TPMKIT_BINARY_ROOT}/build-bad")

file(REMOVE_RECURSE "${TPMKIT_BINARY_ROOT}")
file(MAKE_DIRECTORY
    "${smoke_source_dir}"
    "${good_pkg_root}/include/tss2"
    "${good_pkg_root}/pkgconfig"
    "${bad_pkg_root}/include/tss2"
    "${bad_pkg_root}/pkgconfig"
)

function(write_tss2_pc_files root version)
    file(WRITE "${root}/pkgconfig/tss2-esys.pc"
"prefix=${root}
includedir=${root}/include

Name: tss2-esys
Description: Fake TPM2 ESYS package for tpmkit discovery smoke tests
Version: ${version}
Cflags: -I${root}/include
Libs:
")

    file(WRITE "${root}/pkgconfig/tss2-tctildr.pc"
"prefix=${root}
includedir=${root}/include

Name: tss2-tctildr
Description: Fake TPM2 TCTI loader package for tpmkit discovery smoke tests
Version: ${version}
Cflags: -I${root}/include
Libs:
")
endfunction()

function(write_openssl_pc_file root version)
    file(WRITE "${root}/pkgconfig/openssl.pc"
"prefix=${root}
includedir=${root}/include

Name: openssl
Description: Fake OpenSSL package for tpmkit discovery smoke tests
Version: ${version}
Cflags: -I${root}/include
Libs:
")
endfunction()

function(write_tss2_headers include_root)
    file(WRITE "${include_root}/tss2/tss2_common.h"
"#pragma once
#include <stdint.h>

typedef uint32_t TSS2_RC;

#define TSS2_RC_SUCCESS ((TSS2_RC)0x0000U)
#define TSS2_TCTI_RC_GENERAL_FAILURE ((TSS2_RC)0x0A0001U)
#define TSS2_TCTI_RC_BAD_REFERENCE ((TSS2_RC)0x0A0002U)
#define TSS2_TCTI_RC_INSUFFICIENT_BUFFER ((TSS2_RC)0x0A0003U)
#define TSS2_TCTI_RC_BAD_SEQUENCE ((TSS2_RC)0x0A0004U)
#define TSS2_TCTI_RC_IO_ERROR ((TSS2_RC)0x0A0005U)
")

    file(WRITE "${include_root}/tss2/tss2_tcti.h"
"#pragma once
#include <stddef.h>
#include <stdint.h>
#include <tss2/tss2_common.h>

typedef struct TSS2_TCTI_OPAQUE_CONTEXT_BLOB TSS2_TCTI_CONTEXT;

typedef struct TSS2_TCTI_POLL_HANDLE {
    int fd;
    uint32_t events;
} TSS2_TCTI_POLL_HANDLE;

typedef TSS2_RC (*TSS2_TCTI_TRANSMIT_FCN)(TSS2_TCTI_CONTEXT*, size_t, const uint8_t*);
typedef TSS2_RC (*TSS2_TCTI_RECEIVE_FCN)(TSS2_TCTI_CONTEXT*, size_t*, uint8_t*, int32_t);
typedef void (*TSS2_TCTI_FINALIZE_FCN)(TSS2_TCTI_CONTEXT*);
typedef TSS2_RC (*TSS2_TCTI_CANCEL_FCN)(TSS2_TCTI_CONTEXT*);
typedef TSS2_RC (*TSS2_TCTI_GET_POLL_HANDLES_FCN)(TSS2_TCTI_CONTEXT*,
                                                  TSS2_TCTI_POLL_HANDLE*,
                                                  size_t*);
typedef TSS2_RC (*TSS2_TCTI_SET_LOCALITY_FCN)(TSS2_TCTI_CONTEXT*, uint8_t);

typedef struct TSS2_TCTI_CONTEXT_COMMON_V1 {
    uint64_t magic;
    uint32_t version;
    TSS2_TCTI_TRANSMIT_FCN transmit;
    TSS2_TCTI_RECEIVE_FCN receive;
    TSS2_TCTI_FINALIZE_FCN finalize;
    TSS2_TCTI_CANCEL_FCN cancel;
    TSS2_TCTI_GET_POLL_HANDLES_FCN getPollHandles;
    TSS2_TCTI_SET_LOCALITY_FCN setLocality;
} TSS2_TCTI_CONTEXT_COMMON_V1;

#define TSS2_TCTI_TIMEOUT_NONE (-1)
")
endfunction()

write_tss2_pc_files("${good_pkg_root}" "4.1.3")
write_tss2_pc_files("${bad_pkg_root}" "4.1.2")
write_openssl_pc_file("${good_pkg_root}" "3.5.5")
write_openssl_pc_file("${bad_pkg_root}" "3.5.5")
write_tss2_headers("${good_pkg_root}/include")
write_tss2_headers("${bad_pkg_root}/include")

string(REGEX REPLACE "([][+.*()^$?{}|\\\\])" "\\\\\\1" good_include_regex
       "${good_pkg_root}/include")

file(WRITE "${smoke_source_dir}/CMakeLists.txt"
"cmake_minimum_required(VERSION 3.20)
project(tpmkit_tss_discovery_guard LANGUAGES CXX)

set(tpmkit_BUILD_TESTS OFF CACHE BOOL \"\" FORCE)
set(TPMKIT_BUILD_EXAMPLES OFF CACHE BOOL \"\" FORCE)

add_subdirectory(\"\${TPMKIT_SOURCE_DIR}\" tpmkit-src)

get_target_property(tpmkit_testing_includes tpmkit_testing INCLUDE_DIRECTORIES)
if(NOT tpmkit_testing_includes MATCHES \"\${TPMKIT_EXPECTED_TSS2_INCLUDE_REGEX}\")
    message(FATAL_ERROR
        \"tpmkit_testing does not include the pkg-config TSS2 include path privately\")
endif()
")

execute_process(
    COMMAND
        "${CMAKE_COMMAND}" -E env
        "PKG_CONFIG_LIBDIR=${good_pkg_root}/pkgconfig"
        "${CMAKE_COMMAND}"
        -S "${smoke_source_dir}"
        -B "${good_build_dir}"
        -DTPMKIT_SOURCE_DIR=${TPMKIT_SOURCE_DIR}
        -DTPMKIT_EXPECTED_TSS2_INCLUDE_REGEX=${good_include_regex}
    RESULT_VARIABLE result
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Configuring with fake TPM2 TSS 4.1.3 modules failed")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${good_build_dir}" --target tpmkit_testing --parallel 2
    RESULT_VARIABLE result
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Building tpmkit_testing with non-system TSS headers failed")
endif()

execute_process(
    COMMAND
        "${CMAKE_COMMAND}" -E env
        "PKG_CONFIG_LIBDIR=${bad_pkg_root}/pkgconfig"
        "${CMAKE_COMMAND}"
        -S "${smoke_source_dir}"
        -B "${bad_build_dir}"
        -DTPMKIT_SOURCE_DIR=${TPMKIT_SOURCE_DIR}
        -DTPMKIT_EXPECTED_TSS2_INCLUDE_REGEX=${good_include_regex}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
if(result EQUAL 0)
    message(FATAL_ERROR "Configuring with fake TPM2 TSS 4.1.2 modules succeeded unexpectedly")
endif()

string(CONCAT combined_output "${output}" "${error}")
if(NOT combined_output MATCHES "tss2-esys")
    message(FATAL_ERROR "Unsupported-version configure failure did not name tss2-esys")
endif()
