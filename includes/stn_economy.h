/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_ECONOMY_H
#define STN_ECONOMY_H

#include "stn_pow.h"

#define STN_ECONOMIC_UNIT_CENTISTNC 1u
#define STN_SHARE_FACTOR 10u

/* Derive the qualifying-share target from a valid Chain target:
 *     min(MAX_TARGET, chain_target * STN_SHARE_FACTOR)
 * Targets are exact unsigned 256-bit big-endian integers.
 * MAX_TARGET is the existing PoW maximum 2^255-1.
 * Integer arithmetic only. Output is unchanged on failure; aliasing allowed. */
stn_data_status stn_economy_share_target(
    const uint8_t chain_target[32],
    uint8_t share_target[32]);

#endif
