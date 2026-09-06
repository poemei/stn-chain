/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_PLATFORM_H
#define STN_PLATFORM_H

#include <limits.h>
#include <stdint.h>

#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 201710L
#error STN Chain requires ISO C17 or later.
#endif

_Static_assert(CHAR_BIT == 8, "STN Chain requires 8-bit bytes.");
_Static_assert(sizeof(uint8_t) == 1, "STN Chain requires uint8_t.");
_Static_assert(sizeof(uint16_t) == 2, "STN Chain requires uint16_t.");
_Static_assert(sizeof(uint32_t) == 4, "STN Chain requires uint32_t.");
_Static_assert(sizeof(uint64_t) == 8, "STN Chain requires uint64_t.");
_Static_assert(SIZE_MAX >= UINT32_MAX, "STN Chain requires at least 32-bit size_t.");

#endif
