/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_record.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
static unsigned failures;
#define CHECK(expr) do { ++checks; if (!(expr)) { \
    ++failures; fprintf(stderr, "line %d: %s\n", __LINE__, #expr); \
} } while (0)

/* Independently specified byte fixture. Signature and payload are synthetic,
 * not a valid signed intelligence report. Unspecified bytes are zero. */
static const uint8_t fixture[183] = {
    [0] = 0x53, [1] = 0x54, [2] = 0x4e, [3] = 0x52,
    [5] = 1, [7] = 1,
    [8] = 0x11, [39] = 0x12,
    [40] = 0x21, [71] = 0x22,
    [72] = 0x31, [103] = 0x32,
    [104] = 1, [105] = 2, [106] = 3, [107] = 4,
    [108] = 5, [109] = 6, [110] = 7, [111] = 8,
    [115] = 3, [116] = 0xaa, [117] = 0, [118] = 0xff,
    [119] = 0x41, [182] = 0x42
};

static stn_record sample(void)
{
    static const uint8_t payload[3] = {0xaa, 0, 0xff};
    stn_record r = {0};
    r.version = 1;
    r.type = 1;
    r.network_id[0] = 0x11; r.network_id[31] = 0x12;
    r.signer_public_key[0] = 0x21; r.signer_public_key[31] = 0x22;
    r.nonce[0] = 0x31; r.nonce[31] = 0x32;
    r.issued_at = UINT64_C(0x0102030405060708);
    r.payload = payload;
    r.payload_length = 3;
    r.signature[0] = 0x41; r.signature[63] = 0x42;
    return r;
}

static void known_bytes(void)
{
    uint8_t encoded[184];
    stn_record r = sample();
    stn_record decoded = {0};
    size_t written = 0;
    memset(encoded, 0xcd, sizeof(encoded));
    CHECK(stn_record_encode(&r, encoded, 183, &written) == STN_RECORD_OK);
    CHECK(written == sizeof(fixture));
    CHECK(memcmp(encoded, fixture, sizeof(fixture)) == 0);
    CHECK(encoded[183] == 0xcd);
    CHECK(stn_record_decode(fixture, sizeof(fixture), &decoded) == STN_RECORD_OK);
    CHECK(decoded.version == 1 && decoded.type == 1);
    CHECK(decoded.issued_at == UINT64_C(0x0102030405060708));
    CHECK(memcmp(decoded.network_id, r.network_id, 32) == 0);
    CHECK(memcmp(decoded.signer_public_key, r.signer_public_key, 32) == 0);
    CHECK(memcmp(decoded.nonce, r.nonce, 32) == 0);
    CHECK(memcmp(decoded.signature, r.signature, 64) == 0);
    CHECK(decoded.payload_length == 3 && decoded.payload == fixture + 116);
    CHECK(stn_record_encode(&decoded, encoded, sizeof(encoded), &written) == STN_RECORD_OK);
    CHECK(memcmp(encoded, fixture, sizeof(fixture)) == 0);
}

static void malformed(void)
{
    uint8_t bytes[184];
    stn_record r = sample();
    stn_record before = r;
    size_t i;
    /* Every truncation, including within the variable payload/signature. */
    for (i = 0; i < sizeof(fixture); ++i) {
        CHECK(stn_record_decode(fixture, i, &r) != STN_RECORD_OK);
    }
    CHECK(r.payload == before.payload && r.issued_at == before.issued_at);
    CHECK(memcmp(r.signature, before.signature, 64) == 0);
    memcpy(bytes, fixture, sizeof(fixture)); bytes[183] = 0;
    CHECK(stn_record_decode(bytes, sizeof(bytes), &r) == STN_RECORD_LENGTH);
    bytes[0] ^= 1;
    CHECK(stn_record_decode(bytes, 183, &r) == STN_RECORD_MAGIC);
    memcpy(bytes, fixture, 183); bytes[4] = 1;
    CHECK(stn_record_decode(bytes, 183, &r) == STN_RECORD_UNSUPPORTED);
    memcpy(bytes, fixture, 183); bytes[7] = 2;
    CHECK(stn_record_decode(bytes, 183, &r) == STN_RECORD_UNSUPPORTED);
    memcpy(bytes, fixture, 183); memset(bytes + 72, 0, 32);
    CHECK(stn_record_decode(bytes, 183, &r) == STN_RECORD_NONCE);
    memcpy(bytes, fixture, 183); memset(bytes + 112, 0xff, 4);
    CHECK(stn_record_decode(bytes, 183, &r) == STN_RECORD_LIMIT);
    memcpy(bytes, fixture, 183); bytes[115] = 2;
    CHECK(stn_record_decode(bytes, 183, &r) == STN_RECORD_LENGTH);
    CHECK(stn_record_decode(NULL, 0, &r) == STN_RECORD_ARGUMENT);
    CHECK(stn_record_decode(fixture, 183, NULL) == STN_RECORD_ARGUMENT);
}

static void boundaries(void)
{
    static uint8_t payload[STN_RECORD_MAX_PAYLOAD];
    static uint8_t output[STN_RECORD_MAX_SIZE + 1];
    stn_record r = sample();
    stn_record decoded = {0};
    size_t written;
    size_t i;
    for (i = 0; i < sizeof(payload); ++i) { payload[i] = (uint8_t)(i & 255u); }
    r.payload = payload;
    r.payload_length = STN_RECORD_MAX_PAYLOAD;
    r.issued_at = UINT64_MAX;
    CHECK(stn_record_encode(&r, output, sizeof(output), &written) == STN_RECORD_OK);
    CHECK(written == STN_RECORD_MAX_SIZE);
    CHECK(output[112] == 0 && output[113] == 1 && output[114] == 0 && output[115] == 0);
    CHECK(stn_record_decode(output, written, &decoded) == STN_RECORD_OK);
    CHECK(decoded.issued_at == UINT64_MAX && decoded.payload_length == sizeof(payload));
    CHECK(memcmp(decoded.payload, payload, sizeof(payload)) == 0);
    CHECK(stn_record_decode(output, sizeof(output), &decoded) == STN_RECORD_LIMIT);
    r.payload_length = 0; r.payload = NULL; r.issued_at = 0;
    CHECK(stn_record_encode(&r, output, sizeof(output), &written) == STN_RECORD_OK);
    CHECK(written == 180);
    CHECK(stn_record_decode(output, written, &decoded) == STN_RECORD_OK);
    CHECK(decoded.payload_length == 0 && decoded.issued_at == 0);
}

static void encode_failure(void)
{
    uint8_t bytes[183];
    uint8_t before[183];
    stn_record r = sample();
    size_t written = 999;
    memset(bytes, 0x55, sizeof(bytes)); memcpy(before, bytes, sizeof(bytes));
    CHECK(stn_record_encode(&r, bytes, sizeof(bytes) - 1, &written) == STN_RECORD_CAPACITY);
    CHECK(written == 0 && memcmp(bytes, before, sizeof(bytes)) == 0);
    r.version = 2;
    CHECK(stn_record_encode(&r, bytes, sizeof(bytes), &written) == STN_RECORD_UNSUPPORTED);
    r = sample(); r.type = 0;
    CHECK(stn_record_encode(&r, bytes, sizeof(bytes), &written) == STN_RECORD_UNSUPPORTED);
    r = sample(); r.payload_length = UINT32_MAX;
    CHECK(stn_record_encode(&r, bytes, sizeof(bytes), &written) == STN_RECORD_LIMIT);
    r = sample(); r.payload = NULL;
    CHECK(stn_record_encode(&r, bytes, sizeof(bytes), &written) == STN_RECORD_ARGUMENT);
    r = sample(); memset(r.nonce, 0, 32);
    CHECK(stn_record_encode(&r, bytes, sizeof(bytes), &written) == STN_RECORD_NONCE);
    CHECK(stn_record_encode(NULL, bytes, sizeof(bytes), &written) == STN_RECORD_ARGUMENT);
    CHECK(stn_record_encode(&r, NULL, sizeof(bytes), &written) == STN_RECORD_ARGUMENT);
    CHECK(stn_record_encode(&r, bytes, sizeof(bytes), NULL) == STN_RECORD_ARGUMENT);
    CHECK(written == 0 && memcmp(bytes, before, sizeof(bytes)) == 0);
}

int test_intelligence(void);
int test_validation(void);
int test_chain_data(void);
int test_chain(void);
int test_pow(void);
int test_fork(void);
int test_storage(void);
int test_peer(void);
int test_portability(void);
int test_rpc(void);
int test_mining(void);
int test_pending(void);
int test_identity(void);
int test_authority(void);

int main(void)
{
    int intelligence_failed;
    int validation_failed;
    int chain_data_failed;
    int chain_failed;
    int pow_failed;
    int fork_failed;
    int storage_failed;
    int peer_failed;
    int portability_failed;
    int rpc_failed;
    int mining_failed;
    int pending_failed;
    int identity_failed;
    int authority_failed;
    known_bytes(); malformed(); boundaries(); encode_failure();
    printf("Record codec: %u checks, %u failures.\n", checks, failures);
    intelligence_failed = test_intelligence();
    validation_failed = test_validation();
    chain_data_failed = test_chain_data();
    chain_failed = test_chain();
    pow_failed = test_pow();
    fork_failed = test_fork();
    storage_failed = test_storage();
    peer_failed = test_peer();
    portability_failed = test_portability();
    rpc_failed = test_rpc();
    mining_failed = test_mining();
    pending_failed = test_pending();
    identity_failed = test_identity();
    authority_failed = test_authority();
    return failures == 0 && !intelligence_failed && !validation_failed && !chain_data_failed && !chain_failed && !pow_failed && !fork_failed && !storage_failed && !peer_failed && !portability_failed && !rpc_failed && !mining_failed && !pending_failed && !identity_failed && !authority_failed ? EXIT_SUCCESS : EXIT_FAILURE;
}
