/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_TRANSFER_ENVELOPE_H
#define STN_TRANSFER_ENVELOPE_H

#include "stn_transfer.h"
#include "stn_identity.h"

#define STN_TRANSFER_ENVELOPE_VERSION 1u
#define STN_TRANSFER_NONCE_SIZE 32u
#define STN_TRANSFER_ENVELOPE_CANONICAL_SIZE \
    (1u + STN_IDENTITY_PUBLIC_KEY_SIZE + STN_TRANSFER_NONCE_SIZE + STN_TRANSFER_CANONICAL_SIZE + STN_IDENTITY_SIGNATURE_SIZE)

typedef struct stn_transfer_envelope {
    uint8_t controller[STN_IDENTITY_PUBLIC_KEY_SIZE];
    uint8_t nonce[STN_TRANSFER_NONCE_SIZE];
    stn_transfer transfer;
    uint8_t signature[STN_IDENTITY_SIGNATURE_SIZE];
} stn_transfer_envelope;

/* Canonical accepted-transfer evidence:
 * version || controller[32] || nonce[32] || transfer[73] || signature[64].
 * The nonzero nonce is selected by the controller and provides deterministic
 * uniqueness for otherwise identical legitimate transfers. */
stn_data_status stn_transfer_envelope_encode(const stn_transfer_envelope *envelope,
    uint8_t canonical[STN_TRANSFER_ENVELOPE_CANONICAL_SIZE]);
stn_data_status stn_transfer_envelope_decode(const uint8_t *canonical,size_t length,
    stn_transfer_envelope *out);

#endif
