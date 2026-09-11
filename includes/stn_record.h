/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_RECORD_H
#define STN_RECORD_H

#include "stn_platform.h"
#include <stddef.h>

#define STN_RECORD_HEADER_SIZE 116u
#define STN_RECORD_SIGNATURE_SIZE 64u
#define STN_RECORD_OVERHEAD 180u
#define STN_RECORD_MAX_PAYLOAD 65536u
#define STN_RECORD_MAX_SIZE (STN_RECORD_OVERHEAD + STN_RECORD_MAX_PAYLOAD)
#define STN_RECORD_VERSION 1u
#define STN_RECORD_INTELLIGENCE 1u

typedef enum stn_record_status {
    STN_RECORD_OK = 0,
    STN_RECORD_ARGUMENT,
    STN_RECORD_TRUNCATED,
    STN_RECORD_MAGIC,
    STN_RECORD_UNSUPPORTED,
    STN_RECORD_LIMIT,
    STN_RECORD_LENGTH,
    STN_RECORD_NONCE,
    STN_RECORD_CAPACITY
} stn_record_status;

typedef struct stn_record {
    uint16_t version;
    uint16_t type;
    uint8_t network_id[32];
    uint8_t signer_public_key[32];
    uint8_t nonce[32];
    uint64_t issued_at;
    const uint8_t *payload;
    uint32_t payload_length;
    uint8_t signature[STN_RECORD_SIGNATURE_SIZE];
} stn_record;

/* Structural codec only: success does NOT establish payload validity,
 * signature validity, network membership, authorization, or replay safety.
 * No allocation, I/O, cryptography, or global state.
 *
 * Decode: input must designate input_length readable bytes. On success,
 * payload borrows that input; keep it alive and unchanged for the view's
 * lifetime. All fixed arrays are copied. Output is unchanged on failure.
 * Input and output objects must not overlap.
 */
stn_record_status stn_record_decode(const uint8_t *input, size_t input_length,
                                    stn_record *record);

/* Encode: output must designate capacity writable bytes. Record, payload,
 * output and written storage must not overlap. NULL payload is permitted
 * only when payload_length is zero. On failure output is unchanged and
 * *written is zero (if written is non-NULL). Output need only have room for
 * the actual record, not STN_RECORD_MAX_SIZE. Signature bytes are copied,
 * not generated or verified. */
stn_record_status stn_record_encode(const stn_record *record, uint8_t *output,
                                    size_t capacity, size_t *written);

#endif


