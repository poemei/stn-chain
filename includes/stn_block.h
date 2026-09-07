/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_BLOCK_H
#define STN_BLOCK_H
#include "stn_transaction.h"

#define STN_BLOCK_HEADER_SIZE 168u
#define STN_BLOCK_MAX_TRANSACTIONS 16u
#define STN_BLOCK_MIN_BODY (4u + STN_TX_MIN_SIZE)
#define STN_BLOCK_MAX_BODY (STN_BLOCK_MAX_TRANSACTIONS * (4u + STN_TX_MAX_SIZE))
#define STN_BLOCK_MAX_SIZE (STN_BLOCK_HEADER_SIZE + STN_BLOCK_MAX_BODY)

typedef struct stn_block_header {
    uint16_t version;
    uint16_t flags;
    uint8_t network_id[32];
    uint8_t previous_hash[32];
    uint64_t height;
    uint64_t timestamp;
    uint8_t transaction_commitment[32];
    /* Zero in v1; canonical target and work nonce in v3. */
    uint8_t reserved_target[32];
    uint64_t reserved_work_nonce;
    uint32_t transaction_count;
    uint32_t body_length;
} stn_block_header;

typedef struct stn_block {
    stn_block_header header;
    const uint8_t *body;
} stn_block;

typedef struct stn_transaction_span { const uint8_t *bytes; uint32_t length; } stn_transaction_span;

/* Allocation-free structural checks. Borrowed input spans must remain alive
 * and immutable. All pointers must designate their stated byte ranges.
 * Decode outputs remain unchanged on failure. Encode inputs and outputs must
 * not overlap; failure preserves output and sets *written to zero if supplied.
 * No network identity, timestamp policy, signature, authority, hash or PoW
 * validity is established by these structural routines. */
stn_data_status stn_block_header_decode(const uint8_t *bytes, size_t length, stn_block_header *out);
stn_data_status stn_block_header_validate_structure(const uint8_t *bytes, size_t length);
stn_data_status stn_block_header_encode(const stn_block_header *header,
    uint8_t *output, size_t capacity, size_t *written);
stn_data_status stn_block_body_validate_structure(const uint8_t *bytes, size_t length, uint32_t count);
stn_data_status stn_block_body_encode(const stn_transaction_span *transactions, uint32_t count,
    uint8_t *output, size_t capacity, size_t *written);
stn_data_status stn_block_decode(const uint8_t *bytes, size_t length, stn_block *out);
stn_data_status stn_block_validate_structure(const uint8_t *bytes, size_t length);
stn_data_status stn_block_encode(const stn_block *block,
    uint8_t *output, size_t capacity, size_t *written);

/* Hash commitment to the ordered length-prefixed transaction body.
 * Supply a hash provider explicitly. Output unchanged on failure. */
stn_data_status stn_block_body_commitment(const uint8_t *body, size_t length, uint32_t count,
    const stn_hash_provider *provider, uint8_t digest[32]);
/* Separate provider-dependent checks: duplicate transaction IDs, then body
 * commitment equality. OK is integrity under that provider, NOT consensus.
 * NULL provider remains UNRESOLVED even for a structurally valid block. */
stn_data_status stn_block_check_integrity(const uint8_t *bytes, size_t length,
    const stn_hash_provider *provider);
#endif
