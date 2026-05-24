add_library(tpmkit_spdlog)
add_library(tpmkit::tpmkit_spdlog ALIAS tpmkit_spdlog)

target_sources(tpmkit_spdlog
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/tpmkit/logging/spdlog_api.h>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/tpmkit/logging/spdlog_logger.h>
    PRIVATE
        spdlog_logger.cpp
)
target_include_directories(tpmkit_spdlog
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src"
)
target_link_libraries(tpmkit_spdlog
    PUBLIC
        tpmkit::tpmkit
    PRIVATE
        spdlog::spdlog
)
tpmkit_configure_library_target(tpmkit_spdlog)
