/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md.
 * Host selection only. Never include this from deterministic core sources. */
#ifndef STN_BUILD_CONFIG_H
#define STN_BUILD_CONFIG_H
#define STN_OS_WINDOWS 1
#define STN_OS_LINUX 2
#define STN_OS_MACOS 3
#define STN_ARCH_X86 1
#define STN_ARCH_X64 2
#define STN_ARCH_ARM32 3
#define STN_ARCH_ARM64 4

#if (defined(_WIN32) + defined(__linux__) + (defined(__APPLE__) && defined(__MACH__))) != 1
#error STN_CONFIG_UNKNOWN_OR_AMBIGUOUS_OS
#endif
#if defined(_WIN32)
#define STN_HOST_OS STN_OS_WINDOWS
#elif defined(__linux__)
#define STN_HOST_OS STN_OS_LINUX
#else
#define STN_HOST_OS STN_OS_MACOS
#endif

#if defined(_M_ARM64EC)
#error STN_CONFIG_UNSUPPORTED_ARM64EC_ABI
#endif
#if ((defined(_M_IX86) || defined(__i386__)) + (defined(_M_X64) || defined(_M_AMD64) || defined(__x86_64__)) + (defined(_M_ARM) || defined(__arm__)) + (defined(_M_ARM64) || defined(__aarch64__))) != 1
#error STN_CONFIG_UNKNOWN_OR_AMBIGUOUS_ARCH
#endif
#if defined(_M_IX86) || defined(__i386__)
#define STN_HOST_ARCH STN_ARCH_X86
#elif defined(_M_X64) || defined(_M_AMD64) || defined(__x86_64__)
#define STN_HOST_ARCH STN_ARCH_X64
#elif defined(_M_ARM) || defined(__arm__)
#define STN_HOST_ARCH STN_ARCH_ARM32
#else
#define STN_HOST_ARCH STN_ARCH_ARM64
#endif
#endif
