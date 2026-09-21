/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_ADDRESS_H
#define STN_ADDRESS_H
#include "stn_sha256.h"

#define STN_ADDRESS_ID_SIZE 32u
#define STN_ADDRESS_TEXT_CAPACITY 71u
#define STN_ADDRESS_SHORT_CAPACITY 18u

typedef enum stn_address_type {
    STN_ADDRESS_IDENTITY = 1,
    STN_ADDRESS_CONTRACT = 2,
    STN_ADDRESS_WALLET = 3
} stn_address_type;

typedef struct stn_address {
    stn_address_type type;
    uint8_t identifier[STN_ADDRESS_ID_SIZE];
} stn_address;

/* identifier = SHA256(exact canonical source bytes). No prefix, domain or
 * implicit NUL is hashed. Caller owns source canonicalization; native structs
 * are not canonical input. NULL source is allowed only for length zero.
 * Input limited to UINT32_MAX on every platform, matching the existing CNG
 * provider. A namespace identifies type, never ownership/authority.
 * All inputs/outputs/written must be disjoint. Outputs unchanged on failure,
 * except text methods set *written=0. No allocation or global mutable state. */
stn_data_status stn_address_derive(stn_address_type type,
    const uint8_t *source, size_t length, stn_address *out);
/* Exact span excludes terminator. Lowercase hex only; no trimming, embedded
 * NUL, abbreviated form or unknown prefix is accepted. */
stn_data_status stn_address_decode(const char *text, size_t length, stn_address *out);
stn_data_status stn_address_validate(const char *text, size_t length);
/* Output is NUL-terminated. Capacity includes terminator; written excludes it.
 * Full lengths: 69 (identity), 70 (contract/wallet). Short: 16 or 17.
 * Abbreviation is prefix + five dots + final six hex digits; display only. */
stn_data_status stn_address_encode(const stn_address *address,
    char *text, size_t capacity, size_t *written);
stn_data_status stn_address_abbreviate(const stn_address *address,
    char *text, size_t capacity, size_t *written);
#endif
