add_library(tpmkit)
add_library(tpmkit::tpmkit ALIAS tpmkit)

target_sources(tpmkit
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/tpmkit/api.h>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/tpmkit/exception.h>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/tpmkit/logging/logger.h>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/tpmkit/logging/noop_logger.h>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/tpmkit/result.h>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/tpmkit/tpm_context.h>
    PRIVATE
        error_translation.cpp
        error_translation.h
        impl.h
        log_events.h
        tcti_loader.cpp
        tcti_loader.h
        tpm_context.cpp
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
        PkgConfig::TSS2_ESYS
        PkgConfig::TSS2_TCTILDR
)
tpmkit_configure_library_target(tpmkit)
set_target_properties(tpmkit PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# TPM2 TSS libraries stay PRIVATE implementation dependencies. Static-library
# consumers may still see them as link-only requirements, but TSS headers do
# not become part of the public include surface.
