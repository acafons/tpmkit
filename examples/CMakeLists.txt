add_executable(tpm_context_basics
    tpm_context_basics.cpp
)

target_link_libraries(tpm_context_basics
    PRIVATE
        tpmkit::tpmkit
)
