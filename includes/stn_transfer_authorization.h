/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_TRANSFER_AUTHORIZATION_H
#define STN_TRANSFER_AUTHORIZATION_H

#include "stn_transfer.h"
#include "stn_identity.h"

#define STN_TRANSFER_AUTHORIZATION_VERSION 1u
#define STN_TRANSFER_AUTHORIZATION_DOMAIN "STN-CHAIN:TRANSFER:AUTHORIZE:1"
#define STN_TRANSFER_AUTHORIZATION_DOMAIN_SIZE 31u
#define STN_TRANSFER_AUTHORIZATION_STATEMENT_SIZE \
    (STN_TRANSFER_AUTHORIZATION_DOMAIN_SIZE + 1u + STN_IDENTITY_PUBLIC_KEY_SIZE + STN_TRANSFER_CANONICAL_SIZE)

/* Build the exact statement a wallet controller signs:
 * domain || version || controller public key || canonical transfer.
 * The controller key must derive to the transfer source stnw0_ wallet using the
 * existing wallet rule: SHA-256 over the exact 32 public-key/identity bytes.
 * No text address, timestamp, session or platform data participates. */
stn_data_status stn_transfer_authorization_statement(
    const stn_transfer *transfer,
    const uint8_t controller[STN_IDENTITY_PUBLIC_KEY_SIZE],
    uint8_t statement[STN_TRANSFER_AUTHORIZATION_STATEMENT_SIZE]);

/* Verify control of the source wallet for this exact canonical transfer.
 * This authenticates source-wallet control only. It does not decide balance,
 * replay, consensus acceptance or economic-state mutation. */
stn_data_status stn_transfer_authorization_verify(
    const stn_transfer *transfer,
    const uint8_t controller[STN_IDENTITY_PUBLIC_KEY_SIZE],
    const uint8_t signature[STN_IDENTITY_SIGNATURE_SIZE]);

#endif
