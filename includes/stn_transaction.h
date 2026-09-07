/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_TRANSACTION_H
#define STN_TRANSACTION_H
#include "stn_record.h"

#define STN_TX_HEADER_SIZE 12u
#define STN_TX_MIN_SIZE (STN_TX_HEADER_SIZE + STN_RECORD_OVERHEAD)
#define STN_TX_MAX_SIZE (STN_TX_HEADER_SIZE + STN_RECORD_MAX_SIZE)
#define STN_TX_PUBLICATION 1u
/* Reserved classes, rejected until their formats and activation are defined. */
#define STN_TX_COIN_TRANSFER 2u
#define STN_TX_CONTRACT_ACTION 3u
#define STN_TX_ORGANIZATION_ACTION 4u
#define STN_TX_TREASURY_ACTION 5u

typedef enum stn_data_status {
    STN_DATA_OK = 0, STN_DATA_ARGUMENT, STN_DATA_LENGTH,
    STN_DATA_MAGIC, STN_DATA_VERSION, STN_DATA_TYPE,
    STN_DATA_CONTENT, STN_DATA_CAPACITY, STN_DATA_UNRESOLVED,
    STN_DATA_PROVIDER_ERROR, STN_DATA_DUPLICATE, STN_DATA_COMMITMENT,
    STN_DATA_TARGET, STN_DATA_WORK, STN_DATA_OVERFLOW
} stn_data_status;

typedef struct stn_transaction {
    uint16_t version;
    uint16_t type;
    const uint8_t *record_bytes;
    uint32_t record_length;
} stn_transaction;

/* Provider must calculate SHA-256(domain || bytes), including domain's NUL.
 * Return OK, UNRESOLVED, or PROVIDER_ERROR. Other returns become errors.
 * Windows supplies CNG behind this contract; other providers must reproduce
 * identical SHA-256 bytes. Test doubles provide no cryptography.
 * Provider must be deterministic and must not change input or global state. */
typedef stn_data_status (*stn_hash_fn)(void *user, const uint8_t *domain,
    size_t domain_length, const uint8_t *bytes, size_t length, uint8_t digest[32]);
typedef struct stn_hash_provider { stn_hash_fn hash; void *user; } stn_hash_provider;

/* All codecs are structural only. No signature, authority or consensus checks.
 * Decode borrows immutable input. Output is unchanged on failure.
 * Encode inputs/output/written must not overlap; failure leaves output intact
 * and *written zero. All spans must designate the stated byte ranges. */
stn_data_status stn_transaction_decode(const uint8_t *bytes, size_t length,
    stn_transaction *out);
stn_data_status stn_transaction_validate_structure(const uint8_t *bytes, size_t length);
stn_data_status stn_transaction_encode(const stn_transaction *tx,
    uint8_t *output, size_t capacity, size_t *written);
/* Hash complete canonical transaction including the record signature.
 * digest must not overlap inputs; unchanged unless successful. */
stn_data_status stn_transaction_id(const uint8_t *bytes, size_t length,
    const stn_hash_provider *provider, uint8_t digest[32]);
#endif
