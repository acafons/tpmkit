add_subdirectory(adapters/tpm2_esys)
add_subdirectory(adapters/mock)

if(TPMKIT_LOG_ADAPTER STREQUAL "spdlog")
    add_subdirectory(adapters/logging/spdlog)
endif()

if(TPMKIT_LOG_ADAPTER STREQUAL "stdio")
    add_subdirectory(adapters/logging/stdio)
endif()
