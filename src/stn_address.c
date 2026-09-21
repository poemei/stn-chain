/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_address.h"
#include <string.h>

static const char *prefix(stn_address_type type)
{
    switch(type) {
    case STN_ADDRESS_IDENTITY: return "stn0_";
    case STN_ADDRESS_CONTRACT: return "stnc0_";
    case STN_ADDRESS_WALLET: return "stnw0_";
    default: return NULL;
    }
}

stn_data_status stn_address_derive(stn_address_type type,
    const uint8_t *source, size_t length, stn_address *out)
{
    stn_address address = {0};
    stn_data_status status;
    if(out == NULL || (source == NULL && length != 0)) { return STN_DATA_ARGUMENT; }
    if(prefix(type) == NULL) { return STN_DATA_TYPE; }
    if(length > UINT32_MAX) { return STN_DATA_LENGTH; }
    address.type = type;
    status = stn_sha256(NULL, NULL, 0, source, length, address.identifier);
    if(status == STN_DATA_OK) { *out = address; }
    return status;
}

static int nibble(unsigned char c)
{
    if(c >= '0' && c <= '9') { return c - '0'; }
    if(c >= 'a' && c <= 'f') { return c - 'a' + 10; }
    return -1;
}

stn_data_status stn_address_decode(const char *text, size_t length, stn_address *out)
{
    stn_address address = {0};
    size_t offset, i;
    if(text == NULL || out == NULL) { return STN_DATA_ARGUMENT; }
    if(length != 69 && length != 70) { return STN_DATA_LENGTH; }
    if(memcmp(text, "stn0_", 5) == 0) {
        address.type = STN_ADDRESS_IDENTITY; offset = 5;
    } else if(memcmp(text, "stnc0_", 6) == 0) {
        address.type = STN_ADDRESS_CONTRACT; offset = 6;
    } else if(memcmp(text, "stnw0_", 6) == 0) {
        address.type = STN_ADDRESS_WALLET; offset = 6;
    } else { return STN_DATA_TYPE; }
    if(length != offset + 64) { return STN_DATA_LENGTH; }
    for(i = 0; i < 32; ++i) {
        int high = nibble((unsigned char)text[offset + 2*i]);
        int low = nibble((unsigned char)text[offset + 2*i + 1]);
        if(high < 0 || low < 0) { return STN_DATA_CONTENT; }
        address.identifier[i] = (uint8_t)((high << 4) | low);
    }
    *out = address;
    return STN_DATA_OK;
}

stn_data_status stn_address_validate(const char *text, size_t length)
{
    stn_address address;
    return stn_address_decode(text, length, &address);
}

static stn_data_status format(const stn_address *address, char *text,
    size_t capacity, size_t *written, int abbreviated)
{
    static const char hex[] = "0123456789abcdef";
    const char *label;
    size_t offset, size, i, start = abbreviated ? 29u : 0u;
    char result[STN_ADDRESS_TEXT_CAPACITY];
    if(written != NULL) { *written = 0; }
    if(address == NULL || text == NULL || written == NULL) { return STN_DATA_ARGUMENT; }
    label = prefix(address->type);
    if(label == NULL) { return STN_DATA_TYPE; }
    offset = strlen(label);
    size = offset + (abbreviated ? 11u : 64u);
    if(capacity <= size) { return STN_DATA_CAPACITY; }
    memcpy(result, label, offset);
    if(abbreviated) { memcpy(result + offset, ".....", 5); offset += 5; }
    for(i = start; i < 32; ++i) {
        result[offset++] = hex[address->identifier[i] >> 4];
        result[offset++] = hex[address->identifier[i] & 15];
    }
    result[offset] = '\0';
    memcpy(text, result, size + 1);
    *written = size;
    return STN_DATA_OK;
}

stn_data_status stn_address_encode(const stn_address *address,
    char *text, size_t capacity, size_t *written)
{ return format(address, text, capacity, written, 0); }

stn_data_status stn_address_abbreviate(const stn_address *address,
    char *text, size_t capacity, size_t *written)
{ return format(address, text, capacity, written, 1); }
