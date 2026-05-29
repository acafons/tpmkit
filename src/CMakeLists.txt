add_library(tpmkit)
add_library(tpmkit::tpmkit ALIAS tpmkit)

target_sources(tpmkit
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/tpmkit/api.h>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/tpmkit/encoding/hex.h>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/tpmkit/exception.h>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/tpmkit/hash_algorithm.h>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/tpmkit/logging/logger.h>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/tpmkit/logging/noop_logger.h>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/tpmkit/pcr/bank.h>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/tpmkit/pcr/digest_value.h>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/tpmkit/pcr/index.h>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/tpmkit/pcr/observer.h>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/tpmkit/pcr/provider.h>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/tpmkit/pcr/result_types.h>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/tpmkit/pcr/selection.h>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/tpmkit/result.h>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/tpmkit/secret_buffer.h>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/tpmkit/tpm_context.h>
    PRIVATE
        domain/encoding/hex.cpp
        domain/hash_algorithm.cpp
        domain/pcr/bank.cpp
        domain/pcr/digest_value.cpp
        domain/pcr/index.cpp
        domain/pcr/result_types.cpp
        domain/pcr/selection.cpp
        domain/result.cpp
        domain/secret_buffer.cpp
        domain/secret_buffer_testing.h
)
target_include_directories(tpmkit
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(tpmkit
    PUBLIC
        Microsoft.GSL::GSL
        tl::expected
    PRIVATE
        OpenSSL::Crypto
)

add_subdirectory(adapters/tpm2_esys)

tpmkit_configure_library_target(tpmkit)
set_target_properties(tpmkit PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

add_subdirectory(adapters/mock)

if(TPMKIT_LOG_ADAPTER STREQUAL "spdlog")
    add_subdirectory(adapters/logging/spdlog)
endif()

if(TPMKIT_LOG_ADAPTER STREQUAL "stdio")
    add_subdirectory(adapters/logging/stdio)
endif()
