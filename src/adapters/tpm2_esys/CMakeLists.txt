target_sources(tpmkit
    PRIVATE
        context/impl.h
        context/tcti_loader.cpp
        context/tcti_loader.h
        context/tpm_context.cpp
        pcr/esys_pcr_provider.cpp
        pcr/esys_pcr_provider.h
        pcr/pcr_logging.cpp
        pcr/pcr_logging.h
        pcr/pcr_marshalling.cpp
        pcr/pcr_marshalling.h
        pcr/pcr_validation.cpp
        pcr/pcr_validation.h
        support/error_translation.cpp
        support/error_translation.h
        support/log_events.h
)
target_link_libraries(tpmkit
    PRIVATE
        PkgConfig::TSS2_ESYS
        PkgConfig::TSS2_TCTILDR
        PkgConfig::TSS2_RC
)

# TPM2 TSS libraries stay PRIVATE implementation dependencies. Static-library
# consumers may still see them as link-only requirements, but TSS headers do
# not become part of the public include surface.
