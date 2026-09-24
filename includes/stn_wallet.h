/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_WALLET_H
#define STN_WALLET_H

#include "stn_address.h"

#define STN_WALLET_VERSION 1u
#define STN_WALLET_BINDING_CANONICAL_SIZE 65u

typedef struct stn_wallet_binding {
    stn_address identity;
    stn_address wallet;
} stn_wallet_binding;

/* Derive the deterministic stnw0_ wallet paired with one canonical stn0_
 * identity identifier. The wallet source is exactly the 32 identity identifier
 * bytes; the textual prefix and native struct representation are never hashed.
 * This establishes a deterministic address relationship only. It does not
 * prove control, authorize spending, create a balance or imply compensation. */
stn_data_status stn_wallet_derive(
    const stn_address *identity,
    stn_address *wallet);

/* Canonical relationship:
 * version[1] || stn0_ identifier[32] || stnw0_ identifier[32].
 * Decode validates that wallet is the deterministic derivation of identity.
 * Outputs remain unchanged on failure. */
stn_data_status stn_wallet_binding_encode(
    const stn_wallet_binding *binding,
    uint8_t canonical[STN_WALLET_BINDING_CANONICAL_SIZE]);

stn_data_status stn_wallet_binding_decode(
    const uint8_t *canonical,
    size_t length,
    stn_wallet_binding *out);

#endif
