/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_intelligence.h"
#include <string.h>

static int alnum_lower(uint8_t c)
{
    return (c >= 0x61 && c <= 0x7a) || (c >= 0x30 && c <= 0x39);
}

static stn_intelligence_status validate(const stn_intelligence *v)
{
    size_t i;
    size_t label = 0;
    unsigned evidence = 0;
    if (v->version != STN_INTELLIGENCE_VERSION) {
        return STN_INTELLIGENCE_VERSION_ERROR;
    }
    if (v->severity < 1 || v->severity > 10) {
        return STN_INTELLIGENCE_SEVERITY;
    }
    if (v->classification_length < 1 || v->classification_length > 64) {
        return STN_INTELLIGENCE_CLASSIFICATION;
    }
    if (v->source_length < 1 || v->source_length > 253) {
        return STN_INTELLIGENCE_SOURCE;
    }
    if (v->subject_length < 1 || v->subject_length > 1024) {
        return STN_INTELLIGENCE_SUBJECT;
    }
    if (v->classification == NULL || v->source == NULL || v->subject == NULL) {
        return STN_INTELLIGENCE_ARGUMENT;
    }
    for (i = 0; i < v->classification_length; ++i) {
        uint8_t c = v->classification[i];
        if (!alnum_lower(c) && c != 0x5f && c != 0x2d) {
            return STN_INTELLIGENCE_CLASSIFICATION;
        }
    }
    for (i = 0; i < v->source_length; ++i) {
        uint8_t c = v->source[i];
        if (c == 0x2e) {
            if (label == 0 || v->source[i - 1] == 0x2d) {
                return STN_INTELLIGENCE_SOURCE;
            }
            label = 0;
        } else {
            if ((!alnum_lower(c) && c != 0x2d) ||
                (label == 0 && c == 0x2d) || label == 63) {
                return STN_INTELLIGENCE_SOURCE;
            }
            ++label;
        }
    }
    if (label == 0 || v->source[v->source_length - 1] == 0x2d) {
        return STN_INTELLIGENCE_SOURCE;
    }
    for (i = 0; i < v->subject_length; ++i) {
        if (v->subject[i] < 0x20 || v->subject[i] > 0x7e) {
            return STN_INTELLIGENCE_SUBJECT;
        }
    }
    for (i = 0; i < sizeof(v->evidence_digest); ++i) {
        evidence |= v->evidence_digest[i];
    }
    return evidence != 0 ? STN_INTELLIGENCE_OK : STN_INTELLIGENCE_EVIDENCE;
}

static uint16_t read16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static void write16(uint8_t *p, uint16_t n)
{
    p[0] = (uint8_t)(n >> 8);
    p[1] = (uint8_t)(n & 255u);
}

/* offset never exceeds length. Check subtraction before reading a field. */
static int field(const uint8_t *input, size_t length, size_t *offset,
                 const uint8_t **bytes, uint16_t *count)
{
    if (length - *offset < 2) {
        return 0;
    }
    *count = read16(input + *offset);
    *offset += 2;
    if ((size_t)*count > length - *offset) {
        return 0;
    }
    *bytes = input + *offset;
    *offset += *count;
    return 1;
}

stn_intelligence_status stn_intelligence_decode(const uint8_t *input,
    size_t length, stn_intelligence *result)
{
    stn_intelligence v = {0};
    stn_intelligence_status status;
    size_t offset = 11;
    size_t i;
    if (input == NULL || result == NULL) {
        return STN_INTELLIGENCE_ARGUMENT;
    }
    if (length < STN_INTELLIGENCE_MIN_SIZE || length > STN_INTELLIGENCE_MAX_SIZE) {
        return STN_INTELLIGENCE_LENGTH;
    }
    v.version = read16(input);
    for (i = 2; i < 10; ++i) {
        v.observed_at = (v.observed_at << 8) | input[i];
    }
    v.severity = input[10];
    if (!field(input, length, &offset, &v.classification, &v.classification_length) ||
        !field(input, length, &offset, &v.source, &v.source_length) ||
        !field(input, length, &offset, &v.subject, &v.subject_length) ||
        length - offset != sizeof(v.evidence_digest)) {
        return STN_INTELLIGENCE_LENGTH;
    }
    memcpy(v.evidence_digest, input + offset, sizeof(v.evidence_digest));
    status = validate(&v);
    if (status == STN_INTELLIGENCE_OK) {
        *result = v;
    }
    return status;
}

static void put_field(uint8_t *output, size_t *offset,
                      const uint8_t *bytes, uint16_t count)
{
    write16(output + *offset, count);
    *offset += 2;
    memcpy(output + *offset, bytes, count);
    *offset += count;
}

stn_intelligence_status stn_intelligence_encode(const stn_intelligence *value,
    uint8_t *output, size_t capacity, size_t *written)
{
    stn_intelligence_status status;
    size_t total;
    size_t offset = 11;
    size_t i;
    uint64_t time;
    if (written != NULL) {
        *written = 0;
    }
    if (value == NULL || output == NULL || written == NULL) {
        return STN_INTELLIGENCE_ARGUMENT;
    }
    status = validate(value);
    if (status != STN_INTELLIGENCE_OK) {
        return status;
    }
    total = 49u + (size_t)value->classification_length +
        value->source_length + value->subject_length;
    if (capacity < total) {
        return STN_INTELLIGENCE_CAPACITY;
    }
    write16(output, value->version);
    time = value->observed_at;
    for (i = 10; i > 2; ) {
        output[--i] = (uint8_t)(time & UINT64_C(255));
        time >>= 8;
    }
    output[10] = value->severity;
    put_field(output, &offset, value->classification, value->classification_length);
    put_field(output, &offset, value->source, value->source_length);
    put_field(output, &offset, value->subject, value->subject_length);
    memcpy(output + offset, value->evidence_digest, sizeof(value->evidence_digest));
    *written = total;
    return STN_INTELLIGENCE_OK;
}
