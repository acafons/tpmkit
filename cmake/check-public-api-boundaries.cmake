cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED TPMKIT_SOURCE_DIR)
    message(FATAL_ERROR "TPMKIT_SOURCE_DIR is required.")
endif()

file(GLOB_RECURSE public_headers
    "${TPMKIT_SOURCE_DIR}/include/tpmkit/*.h"
)

set(policy_failures "")

foreach(header IN LISTS public_headers)
    file(RELATIVE_PATH relative_header "${TPMKIT_SOURCE_DIR}" "${header}")

    if(relative_header MATCHES "^include/tpmkit/tpm2_esys/" OR
       relative_header MATCHES "^include/tpmkit/testing/" OR
       relative_header MATCHES "^include/tpmkit/logging/spdlog(_api|_logger)\\.h$")
        continue()
    endif()

    file(STRINGS "${header}" backend_references
        REGEX "(TSS2_|ESYS_|FAPI|#[ \t]*include[ \t]*[<\"](openssl|tss2|spdlog)/)"
    )

    if(backend_references)
        list(APPEND policy_failures
            "${relative_header}: backend-neutral public API must not expose backend types or includes")
    endif()
endforeach()

if(policy_failures)
    list(JOIN policy_failures "\n" formatted_failures)
    message(FATAL_ERROR "Public API boundary check failed:\n${formatted_failures}")
endif()
