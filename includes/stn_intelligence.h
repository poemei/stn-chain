/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_INTELLIGENCE_H
#define STN_INTELLIGENCE_H

#include "stn_platform.h"
#include <stddef.h>

#define STN_INTELLIGENCE_MIN_SIZE 52u
#define STN_INTELLIGENCE_MAX_SIZE 1390u
#define STN_INTELLIGENCE_VERSION 1u

typedef enum stn_intelligence_status {
    STN_INTELLIGENCE_OK = 0,
    STN_INTELLIGENCE_ARGUMENT,
    STN_INTELLIGENCE_LENGTH,
    STN_INTELLIGENCE_VERSION_ERROR,
    STN_INTELLIGENCE_SEVERITY,
    STN_INTELLIGENCE_CLASSIFICATION,
    STN_INTELLIGENCE_SOURCE,
    STN_INTELLIGENCE_SUBJECT,
    STN_INTELLIGENCE_EVIDENCE,
    STN_INTELLIGENCE_CAPACITY
} stn_intelligence_status;

typedef struct stn_intelligence {
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
} stn_intelligence;

/* Schema checks only; no cryptography, clock, DNS, evidence retrieval,
 * authorization or replay validation. All text is length-delimited ASCII,
 * not NUL-terminated. All functions are allocation-free.
 * Decode borrows field spans from input; retain it unchanged for their
 * lifetime. Output is unchanged on failure. Input/output must not overlap.
 * Pointers must designate the specified readable/writable byte ranges. */
stn_intelligence_status stn_intelligence_decode(const uint8_t *input,
    size_t length, stn_intelligence *result);

/* Record, field spans, output buffer and written must not overlap.
 * Failure leaves output unchanged and sets *written to zero when non-NULL. */
stn_intelligence_status stn_intelligence_encode(const stn_intelligence *value,
    uint8_t *output, size_t capacity, size_t *written);

#endif
