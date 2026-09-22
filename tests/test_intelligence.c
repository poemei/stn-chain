/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_intelligence.h"
#include "stn_record.h"
#include <stdio.h>
#include <string.h>

static unsigned checks;
static unsigned failures;
#define CHECK(expr) do { ++checks; if (!(expr)) { \
    ++failures; fprintf(stderr, "intelligence line %d: %s\n", __LINE__, #expr); \
} } while (0)

/* Fixed minimum-size wire example, including a synthetic evidence digest. */
static const uint8_t fixture[52] = {
    [1] = 1, [2] = 1, [3] = 2, [4] = 3, [5] = 4,
    [6] = 5, [7] = 6, [8] = 7, [9] = 8, [10] = 10,
    [12] = 1, [13] = 0x61, [15] = 1, [16] = 0x62,
    [18] = 1, [19] = 0x7e, [20] = 1, [51] = 0xff
};

static stn_intelligence sample(void)
{
    stn_intelligence v = {0};
    v.version = 1; v.observed_at = UINT64_C(0x0102030405060708);
    v.severity = 10;
    v.classification = (const uint8_t *)"a"; v.classification_length = 1;
    v.source = (const uint8_t *)"b"; v.source_length = 1;
    v.subject = (const uint8_t *)"~"; v.subject_length = 1;
    v.evidence_digest[0] = 1; v.evidence_digest[31] = 0xff;
    return v;
}

static void known_bytes(void)
{
    stn_intelligence v = sample();
    stn_intelligence d = {0};
    uint8_t out[53];
    size_t n;
    memset(out, 0xcd, sizeof(out));
    CHECK(stn_intelligence_encode(&v, out, 52, &n) == STN_INTELLIGENCE_OK);
    CHECK(n == 52 && out[52] == 0xcd && memcmp(out, fixture, 52) == 0);
    CHECK(stn_intelligence_decode(fixture, sizeof(fixture), &d) == STN_INTELLIGENCE_OK);
    CHECK(d.version == 1 && d.severity == 10 && d.observed_at == v.observed_at);
    CHECK(d.classification == fixture + 13 && d.classification_length == 1);
    CHECK(d.source == fixture + 16 && d.source_length == 1);
    CHECK(d.subject == fixture + 19 && d.subject_length == 1);
    CHECK(memcmp(d.evidence_digest, v.evidence_digest, 32) == 0);
    CHECK(stn_intelligence_encode(&d, out, sizeof(out), &n) == STN_INTELLIGENCE_OK);
    CHECK(memcmp(out, fixture, 52) == 0);
}

static void malformed(void)
{
    uint8_t bytes[53];
    stn_intelligence d = sample();
    size_t i;
    for (i = 0; i < sizeof(fixture); ++i) {
        CHECK(stn_intelligence_decode(fixture, i, &d) == STN_INTELLIGENCE_LENGTH);
    }
    CHECK(d.observed_at == UINT64_C(0x0102030405060708) && d.source[0] == 'b');
    memcpy(bytes, fixture, 52); bytes[52] = 0;
    CHECK(stn_intelligence_decode(bytes, 53, &d) == STN_INTELLIGENCE_LENGTH);
    bytes[1] = 2;
    CHECK(stn_intelligence_decode(bytes, 52, &d) == STN_INTELLIGENCE_VERSION_ERROR);
    memcpy(bytes, fixture, 52); bytes[10] = 0;
    CHECK(stn_intelligence_decode(bytes, 52, &d) == STN_INTELLIGENCE_SEVERITY);
    bytes[10] = 11;
    CHECK(stn_intelligence_decode(bytes, 52, &d) == STN_INTELLIGENCE_SEVERITY);
    memcpy(bytes, fixture, 52); bytes[13] = 0;
    CHECK(stn_intelligence_decode(bytes, 52, &d) == STN_INTELLIGENCE_CLASSIFICATION);
    memcpy(bytes, fixture, 52); bytes[16] = 0x80;
    CHECK(stn_intelligence_decode(bytes, 52, &d) == STN_INTELLIGENCE_SOURCE);
    memcpy(bytes, fixture, 52); bytes[19] = 0x7f;
    CHECK(stn_intelligence_decode(bytes, 52, &d) == STN_INTELLIGENCE_SUBJECT);
    memcpy(bytes, fixture, 52); memset(bytes + 20, 0, 32);
    CHECK(stn_intelligence_decode(bytes, 52, &d) == STN_INTELLIGENCE_EVIDENCE);
    for (i = 11; i <= 17; i += 3) {
        memcpy(bytes, fixture, 52); bytes[i] = 0xff; bytes[i + 1] = 0xff;
        CHECK(stn_intelligence_decode(bytes, 52, &d) == STN_INTELLIGENCE_LENGTH);
    }
    CHECK(stn_intelligence_decode(NULL, 0, &d) == STN_INTELLIGENCE_ARGUMENT);
    CHECK(stn_intelligence_decode(fixture, 52, NULL) == STN_INTELLIGENCE_ARGUMENT);
}

static void source_rules(void)
{
    const char *bad[] = {"", ".a", "a.", "a..b", "-a", "a-", "a-.b",
        "a.-b", "A.example", "https://a", "a:80", "*.a", "a_b", "a b"};
    const char *good[] = {"a", "sensor.example", "a-b.c0", "0", "a.b.c"};
    uint8_t out[STN_INTELLIGENCE_MAX_SIZE];
    uint8_t label[64];
    size_t n;
    size_t i;
    stn_intelligence v = sample();
    for (i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
        v.source = (const uint8_t *)bad[i]; v.source_length = (uint16_t)strlen(bad[i]);
        CHECK(stn_intelligence_encode(&v, out, sizeof(out), &n) == STN_INTELLIGENCE_SOURCE);
    }
    for (i = 0; i < sizeof(good) / sizeof(good[0]); ++i) {
        stn_intelligence d;
        v.source = (const uint8_t *)good[i]; v.source_length = (uint16_t)strlen(good[i]);
        CHECK(stn_intelligence_encode(&v, out, sizeof(out), &n) == STN_INTELLIGENCE_OK);
        CHECK(stn_intelligence_decode(out, n, &d) == STN_INTELLIGENCE_OK);
    }
    memset(label, 'a', sizeof(label)); v.source = label; v.source_length = 63;
    CHECK(stn_intelligence_encode(&v, out, sizeof(out), &n) == STN_INTELLIGENCE_OK);
    v.source_length = 64;
    CHECK(stn_intelligence_encode(&v, out, sizeof(out), &n) == STN_INTELLIGENCE_SOURCE);
}

static void boundaries_and_envelope(void)
{
    uint8_t classification[65];
    uint8_t source[254];
    uint8_t subject[1025];
    uint8_t payload[STN_INTELLIGENCE_MAX_SIZE + 1];
    uint8_t wire[STN_RECORD_OVERHEAD + STN_INTELLIGENCE_MAX_SIZE];
    stn_intelligence v = sample();
    stn_intelligence d = {0};
    stn_record r = {0};
    stn_record decoded = {0};
    size_t n;
    size_t wire_length;
    size_t i;
    memset(classification, 'a', sizeof(classification));
    memset(source, 'b', sizeof(source));
    source[63] = '.'; source[127] = '.'; source[191] = '.';
    memset(subject, ' ', sizeof(subject));
    v.classification = classification; v.classification_length = 64;
    v.source = source; v.source_length = 253;
    v.subject = subject; v.subject_length = 1024;
    v.observed_at = UINT64_MAX; v.severity = 1;
    CHECK(stn_intelligence_encode(&v, payload, sizeof(payload), &n) == STN_INTELLIGENCE_OK);
    CHECK(n == STN_INTELLIGENCE_MAX_SIZE);
    CHECK(stn_intelligence_decode(payload, n, &d) == STN_INTELLIGENCE_OK);
    CHECK(d.observed_at == UINT64_MAX && d.subject_length == 1024);
    CHECK(memcmp(d.source, source, 253) == 0 && memcmp(d.subject, subject, 1024) == 0);
    /* Every truncation of a multi-field maximum-size payload is rejected. */
    for (i = 0; i < n; ++i) {
        CHECK(stn_intelligence_decode(payload, i, &d) != STN_INTELLIGENCE_OK);
    }
    CHECK(stn_intelligence_decode(payload, sizeof(payload), &d) == STN_INTELLIGENCE_LENGTH);
    r.version = 1; r.type = 1; r.nonce[31] = 1;
    r.payload = payload; r.payload_length = (uint32_t)n;
    CHECK(stn_record_encode(&r, wire, sizeof(wire), &wire_length) == STN_RECORD_OK);
    CHECK(stn_record_decode(wire, wire_length, &decoded) == STN_RECORD_OK);
    CHECK(stn_intelligence_decode(decoded.payload, decoded.payload_length, &d) == STN_INTELLIGENCE_OK);
    wire[STN_RECORD_HEADER_SIZE + 10] = 11;
    CHECK(stn_record_decode(wire, wire_length, &decoded) == STN_RECORD_OK);
    CHECK(stn_intelligence_decode(decoded.payload, decoded.payload_length, &d) == STN_INTELLIGENCE_SEVERITY);
    v.classification_length = 65;
    CHECK(stn_intelligence_encode(&v, payload, sizeof(payload), &n) == STN_INTELLIGENCE_CLASSIFICATION);
    v.classification_length = 64; v.source_length = 254;
    CHECK(stn_intelligence_encode(&v, payload, sizeof(payload), &n) == STN_INTELLIGENCE_SOURCE);
    v.source_length = 253; v.subject_length = 1025;
    CHECK(stn_intelligence_encode(&v, payload, sizeof(payload), &n) == STN_INTELLIGENCE_SUBJECT);
}

static void errors_and_ascii(void)
{
    stn_intelligence v = sample();
    uint8_t out[52];
    uint8_t before[52];
    uint8_t c;
    unsigned code;
    size_t n = 999;
    memset(out, 0xa5, sizeof(out)); memcpy(before, out, sizeof(out));
    CHECK(stn_intelligence_encode(&v, out, 51, &n) == STN_INTELLIGENCE_CAPACITY);
    CHECK(n == 0 && memcmp(out, before, sizeof(out)) == 0);
    CHECK(stn_intelligence_encode(NULL, out, 52, &n) == STN_INTELLIGENCE_ARGUMENT);
    CHECK(stn_intelligence_encode(&v, NULL, 52, &n) == STN_INTELLIGENCE_ARGUMENT);
    CHECK(stn_intelligence_encode(&v, out, 52, NULL) == STN_INTELLIGENCE_ARGUMENT);
    v.subject = NULL;
    CHECK(stn_intelligence_encode(&v, out, 52, &n) == STN_INTELLIGENCE_ARGUMENT);
    CHECK(n == 0 && memcmp(out, before, sizeof(out)) == 0);
    v = sample(); v.subject_length = 0;
    CHECK(stn_intelligence_encode(&v, out, 52, &n) == STN_INTELLIGENCE_SUBJECT);
    v = sample(); v.classification_length = 0;
    CHECK(stn_intelligence_encode(&v, out, 52, &n) == STN_INTELLIGENCE_CLASSIFICATION);
    /* Exhaustive one-byte alphabet checks avoid locale-dependent behavior. */
    for (code = 0; code < 256; ++code) {
        int valid;
        c = (uint8_t)code; v = sample(); v.subject = &c;
        valid = code >= 32 && code <= 126;
        CHECK((stn_intelligence_encode(&v, out, 52, &n) == STN_INTELLIGENCE_OK) == valid);
        v = sample(); v.classification = &c;
        valid = (code >= 97 && code <= 122) || (code >= 48 && code <= 57) || code == 45 || code == 95;
        CHECK((stn_intelligence_encode(&v, out, 52, &n) == STN_INTELLIGENCE_OK) == valid);
    }
}

int test_intelligence(void);
int test_intelligence(void)
{
    known_bytes(); malformed(); source_rules(); boundaries_and_envelope(); errors_and_ascii();
    printf("Intelligence payload: %u checks, %u failures.\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
