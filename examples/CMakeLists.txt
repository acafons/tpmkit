add_executable(tpm_context_basics
    tpm_context_basics.cpp
)

target_link_libraries(tpm_context_basics
    PRIVATE
        tpmkit::tpmkit
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
