include_guard(GLOBAL)

find_package(PkgConfig REQUIRED)

set(TPMKIT_OPENSSL_REQUIRED_VERSION "3.5.5" CACHE STRING "Required OpenSSL package version.")
set(TPMKIT_SPDLOG_REQUIRED_VERSION "1.15.3" CACHE STRING "Required spdlog package version.")
pkg_check_modules(OPENSSL_PKG REQUIRED IMPORTED_TARGET
    "openssl = ${TPMKIT_OPENSSL_REQUIRED_VERSION}"
)
find_package(Microsoft.GSL CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
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
    pkg_check_modules(SPDLOG_PKG REQUIRED "spdlog = ${TPMKIT_SPDLOG_REQUIRED_VERSION}")
    find_package(spdlog ${TPMKIT_SPDLOG_REQUIRED_VERSION} CONFIG REQUIRED)
    if(NOT TARGET spdlog::spdlog)
        message(FATAL_ERROR "TPMKIT_LOG_ADAPTER=spdlog requires the spdlog::spdlog CMake target.")
    endif()
endif()
