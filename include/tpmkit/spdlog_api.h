#pragma once

/**
 * @file tpmkit/spdlog_api.h
 * @brief Public symbol visibility macro for the tpmkit_spdlog library.
 */

#if defined(TPMKIT_STATIC_DEFINE)
#define TPMKIT_SPDLOG_API
#elif defined(_WIN32)
#if defined(tpmkit_spdlog_EXPORTS)
#define TPMKIT_SPDLOG_API __declspec(dllexport)
#else
#define TPMKIT_SPDLOG_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define TPMKIT_SPDLOG_API __attribute__((visibility("default")))
#else
#define TPMKIT_SPDLOG_API
#endif
