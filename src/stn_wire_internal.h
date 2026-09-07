/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. Internal bounded helpers. */
#ifndef STN_WIRE_INTERNAL_H
#define STN_WIRE_INTERNAL_H
#include "stn_platform.h"
#include <stddef.h>
static inline uint64_t stn_wire_read(const uint8_t *p, size_t n)
{
    uint64_t v = 0;
    size_t i;
    for (i = 0; i < n; ++i) { v = (v << 8) | p[i]; }
    return v;
}
static inline void stn_wire_write(uint8_t *p, size_t n, uint64_t v)
{
    while (n != 0) { p[--n] = (uint8_t)(v & 255u); v >>= 8; }
}
#endif
