# Intelligence Validation Context

Status: implemented development increment. No production cryptographic,
identity, replay-state, consensus, or network provider is included.

## Implemented layers

- stn_record_decode/encode: unchanged structural envelope behavior.
- stn_intelligence_decode/encode: bounded payload syntax, classification,
  source labels, subject bytes, severity, and nonzero evidence commitment.
- stn_validate_intelligence_record: staged validation with explicit context.

The context contains expected network identity, configured validation time,
future tolerance, optional publication age bound, and separate signature,
authority, and replay hooks with caller-owned state. No allocation, global
mutable validation state, ambient clock, network lookup, or automatic repair
is used. The immutable input and context must remain stable during a call.

## Stages and acceptance

The order is structure -> payload -> network -> time -> signature -> authority
-> replay. Each stage reports NOT_RUN, PASS, REJECT, UNRESOLVED, or ERROR.
Validation stops at the first non-PASS result. Later stages remain NOT_RUN.
Envelope/payload error details are meaningful only if their stage ran.

NULL input or context produces final ERROR before stages run. A missing hook
produces UNRESOLVED at that stage. Provider ERROR and unexpected hook return
values produce final ERROR. A rejection produces final REJECTED. Only every
stage returning PASS produces UNDER_CONTEXT.

UNDER_CONTEXT is conditional on trustworthy provider implementations and a
correct snapshot. It is not block acceptance, finality, truth of intelligence,
or an assertion that the library itself verified a signature. The shipped
application does not install providers or accept records. Test hooks return
scripted statuses solely to verify orchestration; they must never be installed
as production providers.

## Signature hook contract

The verifier receives the exact 24-byte signing domain including its terminal
zero byte, the unsigned envelope/payload bytes, the public key, and the
64-byte signature. It must verify PureEd25519 over domain concatenated with
unsigned bytes under the eventually qualified acceptance profile. The codec
does not substitute a digest or normalize input. Record-ID hashing and actual
signature verification remain unimplemented. A key's presence is not authority.

## Time rules for this development context

- observed_at must be no later than envelope issued_at.
- time_configured must be set; otherwise time is UNRESOLVED.
- Publication later than validation_time is allowed only within the explicit
  future_tolerance_seconds interval.
- A nonzero max_age_seconds limits age of publication, not age of observation.
  Zero disables this age bound.
- Boundary values are inclusive. Zero denotes the Unix epoch, not an absent
  timestamp. Unsigned subtraction after ordering comparisons avoids overflow.

These are development validation policies, not finalized block timestamp
rules. Validators comparing the same state must receive identical time and
policy context. A future consensus design must define how that context is
derived; a node's independent wall clock cannot supply consensus authority.

## Authority and replay

Authority hooks receive the decoded record and payload. Their eventual source
is governing contracts and authorized identities, not coin balances. Consensus
will enforce those rules; it does not originate company authority.

Replay hooks receive the network, signer, nonce, and record fields through the
record view. Hooks are read-only checks over a stable prior-state snapshot;
validation does not reserve a nonce or commit state. A later coordinator must
atomically bind validation to state commit to prevent check/commit races.
Missing state is UNRESOLVED, never assumed authorization or replay safety.

## Intelligence meaning and remaining schema choices

source is an asserted domain-shaped source; signer_public_key references the
publisher's signing key. Neither proves organizational affiliation by itself.
subject carries an opaque indicator. observed_at describes the observation;
issued_at describes publication. classification and severity are the publisher's
assessment, not consensus-certified facts. The nonzero evidence digest is a
commitment, with no raw evidence embedded or retrieval attempted.

Explicit indicator-type enumeration, confidence, bounded optional metadata,
and sensor-to-API attestation chains remain proposed extensions. The initial
schema avoids inventing their production vocabulary before Sentinel integration
requirements are known. No private raw evidence is required by this schema;
operators still must avoid placing sensitive identifiers in public subject
fields. No live intelligence is processed by this increment.

## Verification scope and stopping point

Release checks cover envelope regressions, payload byte fixtures and boundaries,
network mismatch, configured time boundaries and uint64 extremes, missing and
rejected authority/replay providers, signature-hook argument plumbing, invalid
provider statuses, stage ordering, and final acceptance outcomes.

Tests preserve the original 230 envelope checks. Positive signature outcomes
are test-double behavior, not cryptographic evidence. Windows Release/x64 is
the executed target; other platforms remain unqualified.

This increment stops here. Blocks, PoW, mining backends, wallets, contracts,
networking, RPC, gas, fees, and coin economics are not implemented.
