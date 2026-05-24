include_guard(GLOBAL)

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
