/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_IDENTITY_H
#define STN_IDENTITY_H
#include <stddef.h>
#include <stdint.h>

#define STN_IDENTITY_PUBLIC_KEY_SIZE 32u
#define STN_IDENTITY_SIGNATURE_SIZE 64u
#define STN_IDENTITY_DOMAIN "STN-CHAIN:RECORD:SIGN:1"
#define STN_IDENTITY_DOMAIN_SIZE 24u

typedef enum stn_identity_result {
    STN_IDENTITY_VALID=0, STN_IDENTITY_INVALID, STN_IDENTITY_MALFORMED
} stn_identity_result;

/* Identity is the canonical 32-byte public key. It proves key control only. */
stn_identity_result stn_identity_derive(const uint8_t public_key[STN_IDENTITY_PUBLIC_KEY_SIZE],
    uint8_t identity[STN_IDENTITY_PUBLIC_KEY_SIZE]);
stn_identity_result stn_identity_statement(const uint8_t *unsigned_bytes,size_t unsigned_length,
    uint8_t *statement,size_t statement_capacity,size_t *statement_length);
stn_identity_result stn_identity_verify(const uint8_t public_key[STN_IDENTITY_PUBLIC_KEY_SIZE],
    const uint8_t *statement,size_t statement_length,const uint8_t *signature,size_t signature_length);
#endif
