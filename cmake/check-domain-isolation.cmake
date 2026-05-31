cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED TPMKIT_SOURCE_DIR)
    message(FATAL_ERROR "TPMKIT_SOURCE_DIR is required.")
endif()

file(GLOB_RECURSE isolated_sources
    "${TPMKIT_SOURCE_DIR}/include/tpmkit/*.h"
    "${TPMKIT_SOURCE_DIR}/src/domain/*.h"
    "${TPMKIT_SOURCE_DIR}/src/domain/*.cpp"
)

set(policy_failures "")

foreach(source IN LISTS isolated_sources)
    file(STRINGS "${source}" third_party_includes
        REGEX "^[ \t]*#[ \t]*include[ \t]*[<\"](openssl|tss2|spdlog)/"
    )

    if(third_party_includes)
        file(RELATIVE_PATH relative_source "${TPMKIT_SOURCE_DIR}" "${source}")
        list(APPEND policy_failures
            "${relative_source}: domain/public headers must not include third-party backend headers")
    endif()
endforeach()

if(policy_failures)
    list(JOIN policy_failures "\n" formatted_failures)
    message(FATAL_ERROR "Domain isolation check failed:\n${formatted_failures}")
endif()
