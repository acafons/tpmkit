add_library(tpmkit_stdio)
add_library(tpmkit::tpmkit_stdio ALIAS tpmkit_stdio)

target_sources(tpmkit_stdio
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/tpmkit/logging/stdio_api.h>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/tpmkit/logging/stdio_logger.h>
    PRIVATE
        stdio_logger.cpp
)
target_include_directories(tpmkit_stdio
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src"
)
target_link_libraries(tpmkit_stdio
    PUBLIC
        tpmkit::tpmkit
)
tpmkit_configure_library_target(tpmkit_stdio)
