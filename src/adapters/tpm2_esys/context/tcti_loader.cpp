#include "tcti_loader.h"

#include "../support/error_translation.h"

#include <tpmkit/logging/logger.h>

#include <tss2/tss2_common.h>
#include <tss2/tss2_tcti.h>
#include <tss2/tss2_tctildr.h>

#include <cctype>
#include <cstdlib>
#include <string>
#include <string_view>

namespace tpmkit::detail::esys {
namespace {

constexpr std::string_view tcti_invalid_message = "TCTI configuration must use name:args format";

using pending_tcti_ptr = std::unique_ptr<TSS2_TCTI_CONTEXT, void (*)(TSS2_TCTI_CONTEXT*)>;

struct pending_tcti_context {
    pending_tcti_ptr context;
    std::size_t size;
};

[[nodiscard]] bool is_space(const char value) noexcept
{
    return std::isspace(static_cast<unsigned char>(value)) != 0;
}

[[nodiscard]] std::string_view trim(const std::string_view value) noexcept
{
    std::size_t first = 0;
    std::size_t last = value.size();

    while (first < last && is_space(value[first])) {
        ++first;
    }

    while (last > first && is_space(value[last - 1])) {
        --last;
    }

    return value.substr(first, last - first);
}

[[nodiscard]] bool has_name_args_shape(const std::string_view value) noexcept
{
    const std::size_t colon = value.find(':');

    return colon != std::string_view::npos && colon > 0;
}

[[nodiscard]] std::string_view tcti_name(const std::string_view value) noexcept
{
    return value.substr(0, value.find(':'));
}

[[nodiscard]] outcome<std::string> validate_config(const tcti_string_config& config)
{
    const std::string_view trimmed = trim(config.config);

    if (trimmed.empty()) {
        return tl::unexpected(
            error{error_category::input_error, std::string{tcti_invalid_message}});
    }

    if (trimmed.size() != config.config.size()) {
        return tl::unexpected(
            error{error_category::input_error, std::string{tcti_invalid_message}});
    }

    if (!has_name_args_shape(trimmed)) {
        return tl::unexpected(
            error{error_category::input_error, std::string{tcti_invalid_message}});
    }

    return std::string{trimmed};
}

void finalize_loaded_tcti(TSS2_TCTI_CONTEXT* context) noexcept
{
    Tss2_TctiLdr_Finalize(&context);
}

void release_failed_tcti_init(TSS2_TCTI_CONTEXT* const context) noexcept
{
    if (context == nullptr) {
        return;
    }

    TSS2_TCTI_CONTEXT* cleanup_context = context;
    Tss2_TctiLdr_Finalize(&cleanup_context);
    if (cleanup_context != nullptr) {
        std::free(cleanup_context);
    }
}

void free_tcti_info(TSS2_TCTI_INFO* info) noexcept
{
    Tss2_TctiLdr_FreeInfo(&info);
}

[[nodiscard]] outcome<pending_tcti_context> allocate_tcti_loader_context(logger* const log)
{
    std::size_t context_size = 0U;
    const TSS2_RC rc = Tss2_Tcti_TctiLdr_Init(nullptr, &context_size, nullptr);
    if (rc != TSS2_RC_SUCCESS) {
        auto translated = translate_tss_rc(rc, "tcti_init", log);
        return tl::unexpected(translated.error());
    }

    if (context_size == 0U) {
        return tl::unexpected(
            error{error_category::backend_error, "TCTI loader returned invalid context size"});
    }

    void* const storage = std::calloc(1U, context_size);
    if (storage == nullptr) {
        return tl::unexpected(
            error{error_category::resource_error, "TCTI loader context allocation failed"});
    }

    return pending_tcti_context{
        pending_tcti_ptr{static_cast<TSS2_TCTI_CONTEXT*>(storage), &release_failed_tcti_init},
        context_size,
    };
}

} // namespace

outcome<unique_tcti_ptr> load_tcti(const tcti_string_config& config, logger* const log)
{
    auto validated = validate_config(config);
    if (!validated.has_value()) {
        return tl::unexpected(validated.error());
    }

    TSS2_TCTI_INFO* info = nullptr;
    const std::string name{tcti_name(*validated)};
    const TSS2_RC info_rc = Tss2_TctiLdr_GetInfo(name.c_str(), &info);
    free_tcti_info(info);
    if (info_rc != TSS2_RC_SUCCESS) {
        auto translated = translate_tss_rc(info_rc, "tcti_init", log);
        static_cast<void>(translated);
        return tl::unexpected(error{error_category::input_error, "Unknown TCTI name"});
    }

    auto allocated = allocate_tcti_loader_context(log);
    if (!allocated.has_value()) {
        return tl::unexpected(allocated.error());
    }

    auto pending_context = *std::move(allocated);
    const TSS2_RC rc = Tss2_Tcti_TctiLdr_Init(pending_context.context.get(), &pending_context.size,
                                              validated->c_str());
    if (rc != TSS2_RC_SUCCESS) {
        auto translated = translate_tss_rc(rc, "tcti_init", log);
        return tl::unexpected(translated.error());
    }

    return unique_tcti_ptr{pending_context.context.release(), &finalize_loaded_tcti};
}

} // namespace tpmkit::detail::esys
