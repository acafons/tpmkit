#pragma once

/**
 * @file tpmkit/testing/testing_api.h
 * @brief Public symbol visibility macro for the tpmkit_testing library.
 */

#if defined(TPMKIT_STATIC_DEFINE)
#define TPMKIT_TESTING_API
#elif defined(_WIN32)
#if defined(tpmkit_testing_EXPORTS)
#define TPMKIT_TESTING_API __declspec(dllexport)
#else
#define TPMKIT_TESTING_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define TPMKIT_TESTING_API __attribute__((visibility("default")))
#else
#define TPMKIT_TESTING_API
#endif
