add_library(tpmkit_testing)
add_library(tpmkit::tpmkit_testing ALIAS tpmkit_testing)

target_sources(tpmkit_testing
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/tpmkit/testing/fake_tcti.h>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/tpmkit/testing/fake_tpm_context.h>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/tpmkit/testing/recording_logger.h>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/tpmkit/testing/testing_api.h>
    PRIVATE
        fake_tcti.cpp
        fake_tpm_context.cpp
        recording_logger.cpp
)
target_include_directories(tpmkit_testing
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${TSS2_TCTILDR_INCLUDE_DIRS}
)
target_link_libraries(tpmkit_testing
    PUBLIC
        Microsoft.GSL::GSL
        tl::expected
)
tpmkit_configure_library_target(tpmkit_testing)
