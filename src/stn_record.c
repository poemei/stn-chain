/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_record.h"
#include <string.h>

static const uint8_t record_magic[4] = {0x53, 0x54, 0x4e, 0x52};

static uint64_t read_be(const uint8_t *bytes, size_t count)
{
    uint64_t value = 0;
    size_t i;
    for (i = 0; i < count; ++i) {
        value = (value << 8) | bytes[i];
    }
    return value;
}

static void write_be(uint8_t *bytes, size_t count, uint64_t value)
{
    while (count != 0) {
        bytes[--count] = (uint8_t)(value & UINT64_C(255));
        value >>= 8;
    }
}

static int nonce_present(const uint8_t nonce[32])
{
    size_t i;
    for (i = 0; i < 32; ++i) {
        if (nonce[i] != 0) {
            return 1;
        }
    }
    return 0;
}

stn_record_status stn_record_decode(const uint8_t *input, size_t input_length,
                                    stn_record *record)
{
    uint16_t version;
    uint16_t type;
    uint32_t payload_length;
    stn_record decoded = {0};

    if (input == NULL || record == NULL) {
        return STN_RECORD_ARGUMENT;
    }
    if (input_length < STN_RECORD_OVERHEAD) {
        return STN_RECORD_TRUNCATED;
    }
    if (input_length > STN_RECORD_MAX_SIZE) {
        return STN_RECORD_LIMIT;
    }
    if (memcmp(input, record_magic, sizeof(record_magic)) != 0) {
        return STN_RECORD_MAGIC;
    }
    version = (uint16_t)read_be(input + 4, 2);
    type = (uint16_t)read_be(input + 6, 2);
    if (version != STN_RECORD_VERSION || type != STN_RECORD_INTELLIGENCE) {
        return STN_RECORD_UNSUPPORTED;
    }
    payload_length = (uint32_t)read_be(input + 112, 4);
    if (payload_length > STN_RECORD_MAX_PAYLOAD) {
        return STN_RECORD_LIMIT;
    }
    if ((size_t)payload_length != input_length - STN_RECORD_OVERHEAD) {
        return STN_RECORD_LENGTH;
    }
    if (!nonce_present(input + 72)) {
        return STN_RECORD_NONCE;
    }

    decoded.version = version;
    decoded.type = type;
    memcpy(decoded.network_id, input + 8, 32);
    memcpy(decoded.signer_public_key, input + 40, 32);
    memcpy(decoded.nonce, input + 72, 32);
    decoded.issued_at = read_be(input + 104, 8);
    decoded.payload_length = payload_length;
    decoded.payload = input + STN_RECORD_HEADER_SIZE;
    memcpy(decoded.signature, input + STN_RECORD_HEADER_SIZE + payload_length,
           STN_RECORD_SIGNATURE_SIZE);
    *record = decoded;
    return STN_RECORD_OK;
}

stn_record_status stn_record_encode(const stn_record *record, uint8_t *output,
                                    size_t capacity, size_t *written)
{
    size_t total;
    if (written != NULL) {
        *written = 0;
    }
    if (record == NULL || output == NULL || written == NULL) {
        return STN_RECORD_ARGUMENT;
    }
    if (record->version != STN_RECORD_VERSION ||
        record->type != STN_RECORD_INTELLIGENCE) {
        return STN_RECORD_UNSUPPORTED;
    }
    if (record->payload_length > STN_RECORD_MAX_PAYLOAD) {
        return STN_RECORD_LIMIT;
    }
    if (record->payload_length != 0 && record->payload == NULL) {
        return STN_RECORD_ARGUMENT;
    }
    if (!nonce_present(record->nonce)) {
        return STN_RECORD_NONCE;
    }
    /* The bounded payload ensures this addition fits the platform profile. */
    total = (size_t)record->payload_length + STN_RECORD_OVERHEAD;
    if (capacity < total) {
        return STN_RECORD_CAPACITY;
    }
    memcpy(output, record_magic, sizeof(record_magic));
    write_be(output + 4, 2, record->version);
    write_be(output + 6, 2, record->type);
    memcpy(output + 8, record->network_id, 32);
    memcpy(output + 40, record->signer_public_key, 32);
    memcpy(output + 72, record->nonce, 32);
    write_be(output + 104, 8, record->issued_at);
    write_be(output + 112, 4, record->payload_length);
    if (record->payload_length != 0) {
        memcpy(output + STN_RECORD_HEADER_SIZE, record->payload,
               record->payload_length);
    }
    memcpy(output + STN_RECORD_HEADER_SIZE + record->payload_length,
           record->signature, STN_RECORD_SIGNATURE_SIZE);
    *written = total;
    return STN_RECORD_OK;
}
