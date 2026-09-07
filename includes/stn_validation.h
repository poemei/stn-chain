/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_VALIDATION_H
#define STN_VALIDATION_H

#include "stn_record.h"
#include "stn_intelligence.h"

typedef enum stn_stage_status {
    STN_STAGE_NOT_RUN = 0,
    STN_STAGE_PASS,
    STN_STAGE_REJECT,
    STN_STAGE_UNRESOLVED,
    STN_STAGE_ERROR
} stn_stage_status;

typedef enum stn_acceptance {
    STN_ACCEPTANCE_REJECTED = 0,
    STN_ACCEPTANCE_UNRESOLVED,
    STN_ACCEPTANCE_ERROR,
    STN_ACCEPTANCE_UNDER_CONTEXT
} stn_acceptance;

/* Hooks return PASS, REJECT, UNRESOLVED or ERROR only. Any other value is
 * treated as ERROR. They must be deterministic, read-only queries over the
 * supplied snapshot: no clock/DNS/network reads or replay-state mutation.
 * A signature hook must actually verify PureEd25519 over domain || unsigned
 * bytes. PASS is a security assertion by the caller's provider, not evidence
 * that this library implements cryptography. Domain includes its zero byte. */
typedef stn_stage_status (*stn_signature_hook)(void *user,
    const uint8_t *domain, size_t domain_length,
    const uint8_t *unsigned_bytes, size_t unsigned_length,
    const uint8_t public_key[32], const uint8_t signature[64]);
typedef stn_stage_status (*stn_authority_hook)(void *user,
    const stn_record *record, const stn_intelligence *intelligence);
typedef stn_stage_status (*stn_replay_hook)(void *user,
    const stn_record *record);

typedef struct stn_validation_context {
    uint8_t expected_network[32];
    /* Unconfigured time is UNRESOLVED. Values are unsigned Unix seconds.
     * observed_at <= issued_at is always required by this dev profile.
     * Future publication is bounded by future_tolerance_seconds.
     * max_age_seconds == 0 means no age limit; never consult ambient time. */
    int time_configured;
    uint64_t validation_time;
    uint64_t future_tolerance_seconds;
    uint64_t max_age_seconds;
    stn_signature_hook verify_signature;
    void *signature_user;
    stn_authority_hook lookup_authority;
    void *authority_user;
    stn_replay_hook check_replay;
    void *replay_user;
} stn_validation_context;

typedef struct stn_validation_report {
    stn_stage_status structure;
    stn_stage_status payload;
    stn_stage_status network;
    stn_stage_status time;
    stn_stage_status signature;
    stn_stage_status authority;
    stn_stage_status replay;
    stn_acceptance acceptance;
    stn_record_status envelope_error;
    stn_intelligence_status payload_error;
} stn_validation_report;

/* Stages stop at the first failure/unresolved/error. Later stages are NOT_RUN.
 * NULL input/context returns ERROR before parsing. Context, hooks and input
 * must remain unchanged during the call. Returned report owns no pointers.
 * No records or state are committed. UNDER_CONTEXT means all caller-supplied
 * checks passed, not block inclusion, consensus acceptance, or proven truth.
 * The library ships no cryptographic, authority, or replay-state provider. */
stn_validation_report stn_validate_intelligence_record(const uint8_t *bytes,
    size_t length, const stn_validation_context *context);

#endif
