/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_validation.h"
#include <string.h>

static const uint8_t sign_domain[] = "STN-CHAIN:RECORD:SIGN:1";
_Static_assert(sizeof(sign_domain) == 24, "Signing domain must include one NUL.");

static stn_stage_status checked_hook(stn_stage_status value)
{
    switch (value) {
    case STN_STAGE_PASS:
    case STN_STAGE_REJECT:
    case STN_STAGE_UNRESOLVED:
    case STN_STAGE_ERROR:
        return value;
    default:
        return STN_STAGE_ERROR;
    }
}

static stn_validation_report stopped(stn_validation_report r, stn_stage_status status)
{
    switch (status) {
    case STN_STAGE_REJECT: r.acceptance = STN_ACCEPTANCE_REJECTED; break;
    case STN_STAGE_UNRESOLVED: r.acceptance = STN_ACCEPTANCE_UNRESOLVED; break;
    default: r.acceptance = STN_ACCEPTANCE_ERROR; break;
    }
    return r;
}

stn_validation_report stn_validate_intelligence_record(const uint8_t *bytes,
    size_t length, const stn_validation_context *context)
{
    stn_validation_report r = {0};
    stn_record record;
    stn_intelligence intelligence;
    r.acceptance = STN_ACCEPTANCE_ERROR;
    if (bytes == NULL || context == NULL) {
        return r;
    }
    r.envelope_error = stn_record_decode(bytes, length, &record);
    r.structure = r.envelope_error == STN_RECORD_OK ? STN_STAGE_PASS : STN_STAGE_REJECT;
    if (r.structure != STN_STAGE_PASS) { return stopped(r, r.structure); }

    r.payload_error = stn_intelligence_decode(record.payload, record.payload_length, &intelligence);
    r.payload = r.payload_error == STN_INTELLIGENCE_OK ? STN_STAGE_PASS : STN_STAGE_REJECT;
    if (r.payload != STN_STAGE_PASS) { return stopped(r, r.payload); }

    r.network = memcmp(record.network_id, context->expected_network, 32) == 0 ?
        STN_STAGE_PASS : STN_STAGE_REJECT;
    if (r.network != STN_STAGE_PASS) { return stopped(r, r.network); }

    r.time = STN_STAGE_PASS;
    if (intelligence.observed_at > record.issued_at) {
        r.time = STN_STAGE_REJECT;
    } else if (!context->time_configured) {
        r.time = STN_STAGE_UNRESOLVED;
    } else if (record.issued_at > context->validation_time &&
        record.issued_at - context->validation_time > context->future_tolerance_seconds) {
        r.time = STN_STAGE_REJECT;
    } else if (context->max_age_seconds != 0 &&
        context->validation_time > record.issued_at &&
        context->validation_time - record.issued_at > context->max_age_seconds) {
        r.time = STN_STAGE_REJECT;
    }
    if (r.time != STN_STAGE_PASS) { return stopped(r, r.time); }

    r.signature = context->verify_signature == NULL ? STN_STAGE_UNRESOLVED :
        checked_hook(context->verify_signature(context->signature_user,
            sign_domain, sizeof(sign_domain), bytes,
            STN_RECORD_HEADER_SIZE + (size_t)record.payload_length,
            record.signer_public_key, record.signature));
    if (r.signature != STN_STAGE_PASS) { return stopped(r, r.signature); }

    r.authority = context->lookup_authority == NULL ? STN_STAGE_UNRESOLVED :
        checked_hook(context->lookup_authority(context->authority_user, &record, &intelligence));
    if (r.authority != STN_STAGE_PASS) { return stopped(r, r.authority); }

    r.replay = context->check_replay == NULL ? STN_STAGE_UNRESOLVED :
        checked_hook(context->check_replay(context->replay_user, &record));
    if (r.replay != STN_STAGE_PASS) { return stopped(r, r.replay); }
    r.acceptance = STN_ACCEPTANCE_UNDER_CONTEXT;
    return r;
}
