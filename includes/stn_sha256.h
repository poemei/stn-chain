/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_SHA256_H
#define STN_SHA256_H
#include "stn_transaction.h"
/* Real SHA-256 provider. Windows CNG backend supplied; portable callers use
 * this existing provider signature. Hashes domain || bytes, no implicit NUL.
 * NULL spans allowed only at length zero. Output unchanged on failure.
 * Inputs/output must not overlap. user is unused. */
stn_data_status stn_sha256(void *user,const uint8_t *domain,size_t domain_length,
    const uint8_t *bytes,size_t length,uint8_t digest[32]);
#endif
