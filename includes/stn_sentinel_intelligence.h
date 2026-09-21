/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_SENTINEL_INTELLIGENCE_H
#define STN_SENTINEL_INTELLIGENCE_H

#include "stn_platform.h"
#include <stddef.h>

#define STN_SENTINEL_INTELLIGENCE_MIN_SIZE 52u
#define STN_SENTINEL_INTELLIGENCE_MAX_SIZE 1390u
#define STN_SENTINEL_INTELLIGENCE_VERSION 1u

typedef enum stn_sentinel_intelligence_status {
    STN_SENTINEL_INTELLIGENCE_OK = 0,
    STN_SENTINEL_INTELLIGENCE_ARGUMENT,
    STN_SENTINEL_INTELLIGENCE_LENGTH,
    STN_SENTINEL_INTELLIGENCE_VERSION_ERROR,
    STN_SENTINEL_INTELLIGENCE_SEVERITY,
    STN_SENTINEL_INTELLIGENCE_CLASSIFICATION,
    STN_SENTINEL_INTELLIGENCE_SOURCE,
    STN_SENTINEL_INTELLIGENCE_SUBJECT,
    STN_SENTINEL_INTELLIGENCE_EVIDENCE,
    STN_SENTINEL_INTELLIGENCE_CAPACITY
} stn_sentinel_intelligence_status;

typedef struct stn_sentinel_intelligence {
    uint16_t version;
    uint64_t observed_at;
    uint8_t severity;
    const uint8_t *classification;
    uint16_t classification_length;
    const uint8_t *source;
    uint16_t source_length;
    const uint8_t *subject;
    uint16_t subject_length;
    uint8_t evidence_digest[32];
} stn_sentinel_intelligence;

/*
 * Canonical Sentinel intelligence payload codec.
 *
 * Schema checks only; no cryptography, clock, DNS, evidence retrieval,
 * authorization, replay validation, threat determination or research
 * classification is performed here.
 *
 * All text is length-delimited ASCII, not NUL-terminated.
 * All functions are allocation-free.
 *
 * Decode borrows field spans from input; retain the input unchanged for the
 * lifetime of those spans. Output is unchanged on failure. Input/output must
 * not overlap. Pointers must designate the specified readable/writable byte
 * ranges.
 */
stn_sentinel_intelligence_status stn_sentinel_intelligence_decode(
    const uint8_t *input,
    size_t length,
    stn_sentinel_intelligence *result);

/*
 * Record, field spans, output buffer and written must not overlap.
 * Failure leaves output unchanged and sets *written to zero when non-NULL.
 */
stn_sentinel_intelligence_status stn_sentinel_intelligence_encode(
    const stn_sentinel_intelligence *value,
    uint8_t *output,
    size_t capacity,
    size_t *written);

#endif