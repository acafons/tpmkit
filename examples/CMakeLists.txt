function(tpmkit_add_example target)
    add_executable(${target}
        ${ARGN}
    )
    tpmkit_configure_executable_target(${target})
    set_target_properties(${target}
        PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${tpmkit_EXAMPLES_RUNTIME_DIR}"
    )
    target_link_libraries(${target}
        PRIVATE
            tpmkit::tpmkit
    )
endfunction()

set(tpmkit_EXAMPLES_RUNTIME_DIR "${CMAKE_CURRENT_BINARY_DIR}")

tpmkit_add_example(tpm_context_basics
    tpm_context_basics.cpp
)

if(TARGET tpmkit_stdio)
    target_compile_definitions(tpm_context_basics
        PRIVATE TPMKIT_EXAMPLE_HAS_STDIO
    )
    target_link_libraries(tpm_context_basics
        PRIVATE
            tpmkit::tpmkit_stdio
    )
endif()

add_subdirectory(pcr)
