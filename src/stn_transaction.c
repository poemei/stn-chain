/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_transaction.h"
#include "stn_wire_internal.h"
#include <string.h>

static const uint8_t magic[4] = {0x53, 0x54, 0x4e, 0x54};

static size_t lifecycle_size(uint16_t type)
{
    switch(type) {
    case STN_TX_AUTHORITY_GRANT: return STN_TX_AUTHORITY_GRANT_SIZE;
    case STN_TX_AUTHORITY_REVOKE: case STN_TX_IDENTITY_ROTATE: return STN_TX_AUTHORITY_REVOKE_SIZE;
    default: return 0;
    }
}

stn_data_status stn_transaction_decode(const uint8_t *bytes, size_t length,
    stn_transaction *out)
{
    stn_transaction t = {0};
    stn_record record;
    if (bytes == NULL || out == NULL) { return STN_DATA_ARGUMENT; }
    if (length < STN_TX_HEADER_SIZE || length > STN_TX_MAX_SIZE) { return STN_DATA_LENGTH; }
    if (memcmp(bytes, magic, 4) != 0) { return STN_DATA_MAGIC; }
    t.version = (uint16_t)stn_wire_read(bytes + 4, 2);
    t.type = (uint16_t)stn_wire_read(bytes + 6, 2);
    if (t.version != 1) { return STN_DATA_VERSION; }
    if (t.type < STN_TX_PUBLICATION || t.type == STN_TX_RESERVED || t.type > STN_TX_IDENTITY_ROTATE) { return STN_DATA_TYPE; }
    t.record_length = (uint32_t)stn_wire_read(bytes + 8, 4);
    if ((size_t)t.record_length != length - STN_TX_HEADER_SIZE) { return STN_DATA_LENGTH; }
    t.record_bytes = bytes + STN_TX_HEADER_SIZE;
    if (t.type == STN_TX_PUBLICATION) {
        if (stn_record_decode(t.record_bytes, t.record_length, &record) != STN_RECORD_OK) { return STN_DATA_CONTENT; }
    } else if ((size_t)t.record_length != lifecycle_size(t.type)) { return STN_DATA_LENGTH; }
    *out = t;
    return STN_DATA_OK;
}

stn_data_status stn_transaction_validate_structure(const uint8_t *bytes, size_t length)
{
    stn_transaction t;
    return stn_transaction_decode(bytes, length, &t);
}

stn_data_status stn_transaction_encode(const stn_transaction *tx,
    uint8_t *output, size_t capacity, size_t *written)
{
    stn_record r;
    size_t total;
    if (written != NULL) { *written = 0; }
    if (tx == NULL || output == NULL || written == NULL || tx->record_bytes == NULL) {
        return STN_DATA_ARGUMENT;
    }
    if (tx->version != 1) { return STN_DATA_VERSION; }
    if (tx->type < STN_TX_PUBLICATION || tx->type == STN_TX_RESERVED || tx->type > STN_TX_IDENTITY_ROTATE) { return STN_DATA_TYPE; }
    if (tx->type == STN_TX_PUBLICATION && (tx->record_length < STN_RECORD_OVERHEAD || tx->record_length > STN_RECORD_MAX_SIZE)) return STN_DATA_LENGTH;
    if (tx->type != STN_TX_PUBLICATION && (size_t)tx->record_length != lifecycle_size(tx->type)) return STN_DATA_LENGTH;
    if (tx->type == STN_TX_PUBLICATION) {
        if (stn_record_decode(tx->record_bytes, tx->record_length, &r) != STN_RECORD_OK) { return STN_DATA_CONTENT; }
    } else if ((size_t)tx->record_length != lifecycle_size(tx->type)) { return STN_DATA_LENGTH; }
    total = STN_TX_HEADER_SIZE + (size_t)tx->record_length;
    if (capacity < total) { return STN_DATA_CAPACITY; }
    memcpy(output, magic, 4);
    stn_wire_write(output + 4, 2, tx->version);
    stn_wire_write(output + 6, 2, tx->type);
    stn_wire_write(output + 8, 4, tx->record_length);
    memcpy(output + STN_TX_HEADER_SIZE, tx->record_bytes, tx->record_length);
    *written = total;
    return STN_DATA_OK;
}

stn_data_status stn_transaction_id(const uint8_t *bytes, size_t length,
    const stn_hash_provider *provider, uint8_t digest[32])
{
    static const uint8_t domain[] = "STN-CHAIN:TX:ID:1";
    uint8_t temporary[32] = {0};
    stn_data_status status;
    if (digest == NULL) { return STN_DATA_ARGUMENT; }
    status = stn_transaction_validate_structure(bytes, length);
    if (status != STN_DATA_OK) { return status; }
    if (provider == NULL || provider->hash == NULL) { return STN_DATA_UNRESOLVED; }
    status = provider->hash(provider->user, domain, sizeof(domain), bytes, length, temporary);
    if (status == STN_DATA_OK) { memcpy(digest, temporary, 32); }
    else if (status != STN_DATA_UNRESOLVED) { status = STN_DATA_PROVIDER_ERROR; }
    return status;
}
