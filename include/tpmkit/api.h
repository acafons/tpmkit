#pragma once

/**
 * @file tpmkit/api.h
 * @brief Public symbol visibility macros.
 */

#if defined(TPMKIT_STATIC_DEFINE)
#define TPMKIT_API
#elif defined(_WIN32)
#if defined(tpmkit_EXPORTS) || defined(tpmkit_testing_EXPORTS)
#define TPMKIT_API __declspec(dllexport)
#else
#define TPMKIT_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define TPMKIT_API __attribute__((visibility("default")))
#else
#define TPMKIT_API
#endif
