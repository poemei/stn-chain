/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_validation.h"
#include <stdio.h>
#include <string.h>

static unsigned checks;
static unsigned failures;
#define CHECK(expr) do { ++checks; if (!(expr)) { \
    ++failures; fprintf(stderr, "validation line %d: %s\n", __LINE__, #expr); \
} } while (0)

/* Independent nested fixture: 116-byte header, 52-byte payload, 64-byte
 * synthetic signature. NOT cryptographically signed. */
static const uint8_t fixture[232] = {
    [0] = 0x53, [1] = 0x54, [2] = 0x4e, [3] = 0x52,
    [5] = 1, [7] = 1, [8] = 1, [40] = 2, [72] = 3,
    [111] = 10, [115] = 52,
    [117] = 1, [125] = 5, [126] = 1,
    [128] = 1, [129] = 0x61, [131] = 1, [132] = 0x62,
    [134] = 1, [135] = 0x63, [136] = 1,
    [168] = 4
};

typedef struct hook_state {
    stn_stage_status signature;
    stn_stage_status authority;
    stn_stage_status replay;
    unsigned calls;
} hook_state;

/* Test doubles exercise plumbing only. They provide no security verification. */
static stn_stage_status signature_stub(void *user, const uint8_t *domain,
    size_t domain_length, const uint8_t *bytes, size_t length,
    const uint8_t key[32], const uint8_t signature[64])
{
    static const uint8_t expected_domain[] = "STN-CHAIN:RECORD:SIGN:1";
    hook_state *s = user;
    CHECK(s->calls == 0); ++s->calls;
    CHECK(domain_length == sizeof(expected_domain));
    CHECK(memcmp(domain, expected_domain, sizeof(expected_domain)) == 0);
    CHECK(length == 168 && memcmp(bytes, fixture, 8) == 0);
    CHECK(memcmp(key, bytes + 40, 32) == 0);
    CHECK(memcmp(signature, bytes + length, 64) == 0);
    return s->signature;
}

static stn_stage_status authority_stub(void *user, const stn_record *r,
    const stn_intelligence *p)
{
    hook_state *s = user;
    CHECK(s->calls == 1); ++s->calls;
    CHECK(r->signer_public_key[0] == 2 && p->source_length == 1 && p->source[0] == 'b');
    return s->authority;
}

static stn_stage_status replay_stub(void *user, const stn_record *r)
{
    hook_state *s = user;
    CHECK(s->calls == 2); ++s->calls;
    CHECK(r->network_id[0] == 1 && r->signer_public_key[0] == 2 && r->nonce[0] == 3);
    return s->replay;
}

static stn_validation_context context(hook_state *s)
{
    stn_validation_context c = {0};
    c.expected_network[0] = 1;
    c.time_configured = 1; c.validation_time = 20;
    c.verify_signature = signature_stub; c.signature_user = s;
    c.lookup_authority = authority_stub; c.authority_user = s;
    c.check_replay = replay_stub; c.replay_user = s;
    return c;
}

static stn_validation_report run(stn_validation_context *c, hook_state *s,
                                const uint8_t *bytes, size_t n)
{
    s->calls = 0;
    return stn_validate_intelligence_record(bytes, n, c);
}

static void stages(void)
{
    hook_state s = {STN_STAGE_PASS, STN_STAGE_PASS, STN_STAGE_PASS, 0};
    stn_validation_context c = context(&s);
    stn_validation_report r;
    uint8_t bad[232];
    size_t i;
    r = run(&c, &s, fixture, sizeof(fixture));
    CHECK(r.acceptance == STN_ACCEPTANCE_UNDER_CONTEXT && s.calls == 3);
    CHECK(r.structure == STN_STAGE_PASS && r.payload == STN_STAGE_PASS &&
          r.network == STN_STAGE_PASS && r.time == STN_STAGE_PASS &&
          r.signature == STN_STAGE_PASS && r.authority == STN_STAGE_PASS && r.replay == STN_STAGE_PASS);
    for (i = 0; i < sizeof(fixture); ++i) {
        r = run(&c, &s, fixture, i);
        CHECK(r.acceptance == STN_ACCEPTANCE_REJECTED && r.structure == STN_STAGE_REJECT && s.calls == 0);
    }
    memcpy(bad, fixture, sizeof(bad)); bad[117] = 2;
    r = run(&c, &s, bad, sizeof(bad));
    CHECK(r.structure == STN_STAGE_PASS && r.payload == STN_STAGE_REJECT);
    CHECK(r.payload_error == STN_INTELLIGENCE_VERSION_ERROR && r.network == STN_STAGE_NOT_RUN && s.calls == 0);
    c.expected_network[31] = 1;
    r = run(&c, &s, fixture, sizeof(fixture));
    CHECK(r.network == STN_STAGE_REJECT && r.signature == STN_STAGE_NOT_RUN && s.calls == 0);
    c = context(&s); c.time_configured = 0;
    r = run(&c, &s, fixture, sizeof(fixture));
    CHECK(r.time == STN_STAGE_UNRESOLVED && r.acceptance == STN_ACCEPTANCE_UNRESOLVED && s.calls == 0);
    c = context(&s); c.verify_signature = NULL;
    r = run(&c, &s, fixture, sizeof(fixture));
    CHECK(r.signature == STN_STAGE_UNRESOLVED && r.authority == STN_STAGE_NOT_RUN && s.calls == 0);
    c = context(&s); c.lookup_authority = NULL;
    r = run(&c, &s, fixture, sizeof(fixture));
    CHECK(r.authority == STN_STAGE_UNRESOLVED && r.replay == STN_STAGE_NOT_RUN && s.calls == 1);
    c = context(&s); c.check_replay = NULL;
    r = run(&c, &s, fixture, sizeof(fixture));
    CHECK(r.replay == STN_STAGE_UNRESOLVED && r.acceptance == STN_ACCEPTANCE_UNRESOLVED && s.calls == 2);
    r = stn_validate_intelligence_record(fixture, sizeof(fixture), NULL);
    CHECK(r.acceptance == STN_ACCEPTANCE_ERROR && r.structure == STN_STAGE_NOT_RUN);
    r = stn_validate_intelligence_record(NULL, 0, &c);
    CHECK(r.acceptance == STN_ACCEPTANCE_ERROR && r.structure == STN_STAGE_NOT_RUN);
}

static void hook_failures(void)
{
    const stn_stage_status values[] = {STN_STAGE_REJECT, STN_STAGE_UNRESOLVED,
        STN_STAGE_ERROR, STN_STAGE_NOT_RUN, (stn_stage_status)99};
    const stn_acceptance expected[] = {STN_ACCEPTANCE_REJECTED, STN_ACCEPTANCE_UNRESOLVED,
        STN_ACCEPTANCE_ERROR, STN_ACCEPTANCE_ERROR, STN_ACCEPTANCE_ERROR};
    size_t i;
    for (i = 0; i < sizeof(values)/sizeof(values[0]); ++i) {
        hook_state s = {STN_STAGE_PASS, STN_STAGE_PASS, STN_STAGE_PASS, 0};
        stn_validation_context c = context(&s);
        stn_validation_report r;
        s.signature = values[i]; r = run(&c, &s, fixture, sizeof(fixture));
        CHECK(r.acceptance == expected[i] && s.calls == 1 && r.authority == STN_STAGE_NOT_RUN);
        s.signature = STN_STAGE_PASS; s.authority = values[i];
        r = run(&c, &s, fixture, sizeof(fixture));
        CHECK(r.acceptance == expected[i] && s.calls == 2 && r.replay == STN_STAGE_NOT_RUN);
        s.authority = STN_STAGE_PASS; s.replay = values[i];
        r = run(&c, &s, fixture, sizeof(fixture));
        CHECK(r.acceptance == expected[i] && s.calls == 3);
    }
}

static void time_rules(void)
{
    hook_state s = {STN_STAGE_PASS, STN_STAGE_PASS, STN_STAGE_PASS, 0};
    stn_validation_context c = context(&s);
    stn_validation_report r;
    uint8_t bytes[232];
    memcpy(bytes, fixture, sizeof(bytes)); bytes[125] = 11;
    r = run(&c, &s, bytes, sizeof(bytes));
    CHECK(r.time == STN_STAGE_REJECT && s.calls == 0);
    c.validation_time = 9;
    r = run(&c, &s, fixture, sizeof(fixture));
    CHECK(r.time == STN_STAGE_REJECT && s.calls == 0);
    c.future_tolerance_seconds = 1;
    r = run(&c, &s, fixture, sizeof(fixture));
    CHECK(r.acceptance == STN_ACCEPTANCE_UNDER_CONTEXT);
    c.validation_time = 20; c.max_age_seconds = 9;
    r = run(&c, &s, fixture, sizeof(fixture));
    CHECK(r.time == STN_STAGE_REJECT && s.calls == 0);
    c.max_age_seconds = 10;
    r = run(&c, &s, fixture, sizeof(fixture));
    CHECK(r.acceptance == STN_ACCEPTANCE_UNDER_CONTEXT);
    memcpy(bytes, fixture, sizeof(bytes)); memset(bytes + 104, 0xff, 8);
    memset(bytes + 118, 0xff, 8);
    c.validation_time = UINT64_MAX; c.max_age_seconds = 1;
    r = run(&c, &s, bytes, sizeof(bytes));
    CHECK(r.acceptance == STN_ACCEPTANCE_UNDER_CONTEXT);
    c.validation_time = 0; c.future_tolerance_seconds = UINT64_MAX;
    r = run(&c, &s, bytes, sizeof(bytes));
    CHECK(r.acceptance == STN_ACCEPTANCE_UNDER_CONTEXT);
    c.future_tolerance_seconds = UINT64_MAX - 1;
    r = run(&c, &s, bytes, sizeof(bytes));
    CHECK(r.time == STN_STAGE_REJECT && s.calls == 0);
    memset(bytes + 104, 0, 8); memset(bytes + 118, 0, 8);
    c.validation_time = 0; c.max_age_seconds = 0;
    r = run(&c, &s, bytes, sizeof(bytes));
    CHECK(r.acceptance == STN_ACCEPTANCE_UNDER_CONTEXT);
    c.validation_time = UINT64_MAX; c.max_age_seconds = UINT64_MAX - 1;
    r = run(&c, &s, bytes, sizeof(bytes));
    CHECK(r.time == STN_STAGE_REJECT && s.calls == 0);
}

int test_validation(void);
int test_validation(void)
{
    stages(); hook_failures(); time_rules();
    printf("Validation context: %u checks, %u failures (test hooks, no cryptography).\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
