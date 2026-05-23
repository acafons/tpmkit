#pragma once

/**
 * @file tpmkit/logging/stdio_api.h
 * @brief Public symbol visibility macro for the tpmkit_stdio library.
 */

#if defined(TPMKIT_STATIC_DEFINE)
#define TPMKIT_STDIO_API
#elif defined(_WIN32)
#if defined(tpmkit_stdio_EXPORTS)
#define TPMKIT_STDIO_API __declspec(dllexport)
#else
#define TPMKIT_STDIO_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define TPMKIT_STDIO_API __attribute__((visibility("default")))
#else
#define TPMKIT_STDIO_API
#endif
