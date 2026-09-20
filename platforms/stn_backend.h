/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_BACKEND_H
#define STN_BACKEND_H

#include "stn_build_config.h"

/*
 * Detection is not implementation or qualification.
 *
 * Core-only builds do not include this gate and require no OS backend merely
 * to compile pure validation.
 *
 * Runnable application backends currently exist for:
 *   - Windows x64
 *   - Linux x64
 *
 * Architecture detection alone does not imply backend availability or
 * qualification.
 */

#if STN_HOST_OS == STN_OS_WINDOWS

#if STN_HOST_ARCH != STN_ARCH_X64
#error STN_BACKEND_WINDOWS_ARCH_NOT_QUALIFIED
#endif

#elif STN_HOST_OS == STN_OS_LINUX

#if STN_HOST_ARCH != STN_ARCH_X64
#error STN_BACKEND_LINUX_ARCH_NOT_IMPLEMENTED
#endif

#elif STN_HOST_OS == STN_OS_MACOS

#error STN_BACKEND_MACOS_NOT_IMPLEMENTED

#else

#error STN_BACKEND_OS_NOT_IMPLEMENTED

#endif

#endif