/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_block.h"
#include "stn_pow.h"
#include "stn_wire_internal.h"
#include <string.h>

static const uint8_t magic[4] = {0x53, 0x54, 0x4e, 0x42};

static int nonzero(const uint8_t *p, size_t n)
{
    size_t i;
    for (i = 0; i < n; ++i) { if (p[i] != 0) { return 1; } }
    return 0;
}

static stn_data_status header_valid(const stn_block_header *h)
{
    if (h->version != 1 && h->version != STN_POW_BLOCK_VERSION) { return STN_DATA_VERSION; }
    if (h->flags != 0 || (h->version==1 && (h->reserved_work_nonce != 0 || nonzero(h->reserved_target, 32)))) {
        return STN_DATA_CONTENT;
    }
    if(h->version==STN_POW_BLOCK_VERSION && stn_target_validate(h->reserved_target,32)!=STN_DATA_OK) { return STN_DATA_TARGET; }
    if ((h->height == 0) == nonzero(h->previous_hash, 32)) { return STN_DATA_CONTENT; }
    if (h->transaction_count == 0 || h->transaction_count > STN_BLOCK_MAX_TRANSACTIONS ||
        h->body_length < h->transaction_count * STN_BLOCK_MIN_BODY ||
        h->body_length > h->transaction_count * (4u + STN_TX_MAX_SIZE)) {
        return STN_DATA_LENGTH;
    }
    return STN_DATA_OK;
}

stn_data_status stn_block_header_decode(const uint8_t *bytes, size_t length, stn_block_header *out)
{
    stn_block_header h = {0};
    stn_data_status status;
    if (bytes == NULL || out == NULL) { return STN_DATA_ARGUMENT; }
    if (length != STN_BLOCK_HEADER_SIZE) { return STN_DATA_LENGTH; }
    if (memcmp(bytes, magic, 4) != 0) { return STN_DATA_MAGIC; }
    h.version = (uint16_t)stn_wire_read(bytes + 4, 2);
    h.flags = (uint16_t)stn_wire_read(bytes + 6, 2);
    memcpy(h.network_id, bytes + 8, 32); memcpy(h.previous_hash, bytes + 40, 32);
    h.height = stn_wire_read(bytes + 72, 8); h.timestamp = stn_wire_read(bytes + 80, 8);
    memcpy(h.transaction_commitment, bytes + 88, 32);
    memcpy(h.reserved_target, bytes + 120, 32);
    h.reserved_work_nonce = stn_wire_read(bytes + 152, 8);
    h.transaction_count = (uint32_t)stn_wire_read(bytes + 160, 4);
    h.body_length = (uint32_t)stn_wire_read(bytes + 164, 4);
    status = header_valid(&h);
    if (status == STN_DATA_OK) { *out = h; }
    return status;
}

stn_data_status stn_block_header_validate_structure(const uint8_t *bytes, size_t length)
{
    stn_block_header h;
    return stn_block_header_decode(bytes, length, &h);
}

stn_data_status stn_block_header_encode(const stn_block_header *h,
    uint8_t *output, size_t capacity, size_t *written)
{
    stn_data_status status;
    if (written != NULL) { *written = 0; }
    if (h == NULL || output == NULL || written == NULL) { return STN_DATA_ARGUMENT; }
    status = header_valid(h);
    if (status != STN_DATA_OK) { return status; }
    if (capacity < STN_BLOCK_HEADER_SIZE) { return STN_DATA_CAPACITY; }
    memcpy(output, magic, 4);
    stn_wire_write(output + 4, 2, h->version); stn_wire_write(output + 6, 2, h->flags);
    memcpy(output + 8, h->network_id, 32); memcpy(output + 40, h->previous_hash, 32);
    stn_wire_write(output + 72, 8, h->height); stn_wire_write(output + 80, 8, h->timestamp);
    memcpy(output + 88, h->transaction_commitment, 32);
    memcpy(output + 120, h->reserved_target, 32);
    stn_wire_write(output + 152, 8, h->reserved_work_nonce);
    stn_wire_write(output + 160, 4, h->transaction_count);
    stn_wire_write(output + 164, 4, h->body_length);
    *written = STN_BLOCK_HEADER_SIZE;
    return STN_DATA_OK;
}

stn_data_status stn_block_body_validate_structure(const uint8_t *bytes, size_t length, uint32_t count)
{
    size_t offset = 0;
    uint32_t i;
    if (bytes == NULL) { return STN_DATA_ARGUMENT; }
    if (count == 0 || count > STN_BLOCK_MAX_TRANSACTIONS ||
        length > STN_BLOCK_MAX_BODY || length < (size_t)count * STN_BLOCK_MIN_BODY) {
        return STN_DATA_LENGTH;
    }
    for (i = 0; i < count; ++i) {
        uint32_t n;
        if (length - offset < 4) { return STN_DATA_LENGTH; }
        n = (uint32_t)stn_wire_read(bytes + offset, 4); offset += 4;
        if (n < STN_TX_MIN_SIZE || n > STN_TX_MAX_SIZE || n > length - offset) {
            return STN_DATA_LENGTH;
        }
        if (stn_transaction_validate_structure(bytes + offset, n) != STN_DATA_OK) {
            return STN_DATA_CONTENT;
        }
        offset += n;
    }
    return offset == length ? STN_DATA_OK : STN_DATA_LENGTH;
}

stn_data_status stn_block_body_encode(const stn_transaction_span *transactions, uint32_t count,
    uint8_t *output, size_t capacity, size_t *written)
{
    size_t total = 0;
    size_t offset = 0;
    uint32_t i;
    if (written != NULL) { *written = 0; }
    if (transactions == NULL || output == NULL || written == NULL) { return STN_DATA_ARGUMENT; }
    if (count == 0 || count > STN_BLOCK_MAX_TRANSACTIONS) { return STN_DATA_LENGTH; }
    for (i = 0; i < count; ++i) {
        stn_data_status status = stn_transaction_validate_structure(transactions[i].bytes, transactions[i].length);
        if (status != STN_DATA_OK) { return status; }
        total += 4u + (size_t)transactions[i].length;
    }
    if (capacity < total) { return STN_DATA_CAPACITY; }
    for (i = 0; i < count; ++i) {
        stn_wire_write(output + offset, 4, transactions[i].length); offset += 4;
        memcpy(output + offset, transactions[i].bytes, transactions[i].length);
        offset += transactions[i].length;
    }
    *written = total;
    return STN_DATA_OK;
}

stn_data_status stn_block_decode(const uint8_t *bytes, size_t length, stn_block *out)
{
    stn_block b = {0};
    stn_data_status status;
    if (bytes == NULL || out == NULL) { return STN_DATA_ARGUMENT; }
    if (length < STN_BLOCK_HEADER_SIZE || length > STN_BLOCK_MAX_SIZE) { return STN_DATA_LENGTH; }
    status = stn_block_header_decode(bytes, STN_BLOCK_HEADER_SIZE, &b.header);
    if (status != STN_DATA_OK) { return status; }
    if (length - STN_BLOCK_HEADER_SIZE != b.header.body_length) { return STN_DATA_LENGTH; }
    b.body = bytes + STN_BLOCK_HEADER_SIZE;
    status = stn_block_body_validate_structure(b.body, b.header.body_length, b.header.transaction_count);
    if (status == STN_DATA_OK) { *out = b; }
    return status;
}

stn_data_status stn_block_validate_structure(const uint8_t *bytes, size_t length)
{
    stn_block b;
    return stn_block_decode(bytes, length, &b);
}

stn_data_status stn_block_encode(const stn_block *b, uint8_t *output, size_t capacity, size_t *written)
{
    stn_data_status status;
    size_t ignored;
    size_t total;
    if (written != NULL) { *written = 0; }
    if (b == NULL || output == NULL || written == NULL) { return STN_DATA_ARGUMENT; }
    status = header_valid(&b->header);
    if (status != STN_DATA_OK) { return status; }
    status = stn_block_body_validate_structure(b->body, b->header.body_length, b->header.transaction_count);
    if (status != STN_DATA_OK) { return status; }
    total = STN_BLOCK_HEADER_SIZE + (size_t)b->header.body_length;
    if (capacity < total) { return STN_DATA_CAPACITY; }
    status = stn_block_header_encode(&b->header, output, capacity, &ignored);
    if (status != STN_DATA_OK) { return status; }
    memcpy(output + STN_BLOCK_HEADER_SIZE, b->body, b->header.body_length);
    *written = total;
    return STN_DATA_OK;
}

stn_data_status stn_block_body_commitment(const uint8_t *body, size_t length, uint32_t count,
    const stn_hash_provider *provider, uint8_t digest[32])
{
    static const uint8_t domain[] = "STN-CHAIN:BLOCK:BODY:1";
    uint8_t temporary[32] = {0};
    stn_data_status status;
    if (digest == NULL) { return STN_DATA_ARGUMENT; }
    status = stn_block_body_validate_structure(body, length, count);
    if (status != STN_DATA_OK) { return status; }
    if (provider == NULL || provider->hash == NULL) { return STN_DATA_UNRESOLVED; }
    status = provider->hash(provider->user, domain, sizeof(domain), body, length, temporary);
    if (status == STN_DATA_OK) { memcpy(digest, temporary, 32); }
    else if (status != STN_DATA_UNRESOLVED) { status = STN_DATA_PROVIDER_ERROR; }
    return status;
}

stn_data_status stn_block_check_integrity(const uint8_t *bytes, size_t length,
    const stn_hash_provider *provider)
{
    stn_block b;
    uint8_t ids[STN_BLOCK_MAX_TRANSACTIONS][32];
    uint8_t commitment[32];
    uint32_t i, j;
    size_t offset = 0;
    stn_data_status status = stn_block_decode(bytes, length, &b);
    if (status != STN_DATA_OK) { return status; }
    for (i = 0; i < b.header.transaction_count; ++i) {
        uint32_t n = (uint32_t)stn_wire_read(b.body + offset, 4); offset += 4;
        status = stn_transaction_id(b.body + offset, n, provider, ids[i]);
        if (status != STN_DATA_OK) { return status; }
        for (j = 0; j < i; ++j) {
            if (memcmp(ids[i], ids[j], 32) == 0) { return STN_DATA_DUPLICATE; }
        }
        offset += n;
    }
    status = stn_block_body_commitment(b.body, b.header.body_length, b.header.transaction_count,
                                     provider, commitment);
    if (status != STN_DATA_OK) { return status; }
    return memcmp(commitment, b.header.transaction_commitment, 32) == 0 ?
        STN_DATA_OK : STN_DATA_COMMITMENT;
}
