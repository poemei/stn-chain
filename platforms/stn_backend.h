/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_BACKEND_H
#define STN_BACKEND_H
#include "stn_build_config.h"
/* Detection is not implementation or qualification. Fail deliberately before
 * attempting to use unavailable OS services. Core-only builds do not include
 * this gate and require no OS backend merely to compile pure validation. */
#if STN_HOST_OS == STN_OS_LINUX
#error STN_BACKEND_LINUX_NOT_IMPLEMENTED
#elif STN_HOST_OS == STN_OS_MACOS
#error STN_BACKEND_MACOS_NOT_IMPLEMENTED
#elif STN_HOST_ARCH != STN_ARCH_X64
#error STN_BACKEND_WINDOWS_ARCH_NOT_QUALIFIED
#endif
#endif
