include_guard(GLOBAL)

include(CMakePackageConfigHelpers)

install(TARGETS tpmkit
    EXPORT tpmkitTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(TARGET tpmkit_spdlog)
    install(TARGETS tpmkit_spdlog
        EXPORT tpmkitSpdlogTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

if(TARGET tpmkit_stdio)
    install(TARGETS tpmkit_stdio
        EXPORT tpmkitStdioTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

if(TPMKIT_INSTALL_TESTING)
    install(TARGETS tpmkit_testing
        EXPORT tpmkitTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

install(
    DIRECTORY include/tpmkit
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
    PATTERN "testing" EXCLUDE
    PATTERN "spdlog_api.h" EXCLUDE
    PATTERN "spdlog_logger.h" EXCLUDE
    PATTERN "stdio_api.h" EXCLUDE
    PATTERN "stdio_logger.h" EXCLUDE
)

if(TARGET tpmkit_spdlog)
    install(FILES
        include/tpmkit/logging/spdlog_api.h
        include/tpmkit/logging/spdlog_logger.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/tpmkit/logging
    )
endif()

if(TARGET tpmkit_stdio)
    install(FILES
        include/tpmkit/logging/stdio_api.h
        include/tpmkit/logging/stdio_logger.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/tpmkit/logging
    )
endif()

if(TPMKIT_INSTALL_TESTING)
    install(DIRECTORY include/tpmkit/testing
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/tpmkit
        FILES_MATCHING PATTERN "*.h"
    )
endif()

install(EXPORT tpmkitTargets
    FILE tpmkitTargets.cmake
    NAMESPACE tpmkit::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tpmkit
)

if(TARGET tpmkit_spdlog)
    install(EXPORT tpmkitSpdlogTargets
        FILE tpmkitSpdlogTargets.cmake
        NAMESPACE tpmkit::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tpmkit
    )
endif()

if(TARGET tpmkit_stdio)
    install(EXPORT tpmkitStdioTargets
        FILE tpmkitStdioTargets.cmake
        NAMESPACE tpmkit::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tpmkit
    )
endif()

configure_package_config_file(
    cmake/tpmkitConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/tpmkitConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tpmkit
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/tpmkitConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/tpmkitConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/tpmkitConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tpmkit
)
