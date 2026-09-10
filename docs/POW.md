# SHA-256, Development Proof of Work, and Chain Work


Current work representation is 320 bits / 40 canonical big-endian bytes (Phase 11
Chunk 3). This supersedes historical 256-bit work limits below. STNC/STNP version
2 carries widened work fields; block, target, hash and mining-job formats remain.

Status: implemented and tested on Windows Release/x64. This adds real SHA-256,
PoW verification and work accounting, not mining or distributed consensus.

## Production hash provider

stn_sha256 implements the existing two-span provider contract using Windows
CNG BCrypt SHA-256 with Microsoft Primitive Provider. Its input is exactly
`domain || bytes`; only callers explicitly supplying a NUL include one.
The implementation lives in platforms/windows/stn_sha256.c. Portable core
interfaces contain no Windows types. The native projects link bcrypt.lib.

CNG is an existing OS cryptographic dependency, chosen over introducing a
bundled cryptographic library or writing a new hash primitive. It needs no
service, account, cloud, or network access. It uses per-call handles and a
fixed-size digest; provider allocation is algorithm-sized, not input-sized.
The adapter rejects spans exceeding ULONG length, handles failures without
publishing a digest, and releases handles. NULL spans require zero length.
Other platforms require equivalent providers and are not qualified yet.

The boundary remains injectable for failure tests. No test hash is silently
substituted in the production provider. Hash results do not imply signature
verification, reporter truth, or authorization. SHA-256 support does not select
an Ed25519 implementation.

References: [CNG hash creation](https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcryptcreatehash),
[NIST SHA-256 examples](https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Standards-and-Guidelines/documents/examples/SHA256.pdf),
[NIST test data](https://www.nist.gov/itl/ai/ai-standards-and-guidelines-group/nsrl-test-data).
These sources define the API/algorithm; test execution here is not a formal
cryptographic certification or independent security audit.

## Compatibility profiles

Version 1 remains the exact legacy development-only, non-PoW block profile:
flags, target, and work nonce must be zero. Its existing tests and behavior
are preserved. Version 2 stays reserved/rejected, including existing negative
fixtures. New version 3 activates the fixed-width target and nonce in the
existing 168-byte header. This is a wire-format choice, not a software version.

A chain context with pow_policy == NULL explicitly selects legacy version 1.
Its PoW/work stages remain NOT_RUN and target/cumulative-work fields stay zero.
It is not a PoW-valid chain and cannot accept version 3. A non-NULL pow_policy
strictly selects version 3 for both genesis and successors, and rejects legacy
blocks. There is no mixed-chain downgrade or automatic profile detection.
Applications must deliberately configure the PoW context to require work.

## Canonical work input and block identity

    block_id = pow_hash = SHA256("STN-CHAIN:BLOCK:ID:1" || 00 || header_bytes)

Single SHA-256, not double SHA-256. Header bytes are exactly the canonical
168-byte encoding, including target, work nonce, timestamp, version, network,
parent, height, transaction commitment, count and body length. Existing ID
inputs are unchanged; all header changes affect the preimage. Body integrity
is independently checked before local chain acceptance. Transaction IDs and
body commitments retain their existing canonical domains and full-byte inputs.

The block ID and PoW hash are exactly the same value. There is no host-endian
reinterpretation or reversal before target comparison. This construction does
not claim compatibility with Bitcoin mining hardware, SHA256d ASICs, Stratum,
or any current GPU/ASIC backend. Future backends must implement this exact work
format or require an explicit future protocol change.

## Target and difficulty representation

Version 3 reuses bytes 120-151 (formerly reserved_target) as a 32-byte unsigned
big-endian target T. Bytes 152-159 (reserved_work_nonce in the current C ABI)
become a uint64 big-endian work nonce. Flags remain zero. Header width and all
other offsets stay unchanged. C field names are retained for source continuity.

Permitted development targets: 1 <= T <= 2^255 - 1. The easiest is `7f` followed
by 31 `ff` bytes; the hardest is 31 zero bytes followed by `01`. A value must
use exactly 32 bytes. Leading zeros are required where applicable; they are
not an alternate encoding. Zero, 31/33-byte values, and values with the top
bit set are rejected. No compact notation, exponent, signed encoding, or
floating-point difficulty exists, so compact overflow/noncanonical forms have
no accepted representation.

The context supplies the genesis/bootstrap target. Ordinary v3 validation and
mining enforce the activated Phase 11 rule documented below; this supersedes
the earlier fixed-target-only checkpoint. The target representation is unchanged.

## Verification

Interpret the 32-byte hash as an unsigned big-endian integer H. Accept work
when H <= T (inclusive); reject H > T. Malformed targets are rejected before
comparison. Missing or failed hashing remains unresolved/error and cannot
publish valid work. stn_pow_verify returns a digest only on success.

In PoW chain mode, candidate validation retains existing structural, link,
network and body checks, checks the context target, computes the block ID,
compares it against target, then calculates/adds work. Only successful active
stages commit output. PoW does not override failed signatures or authority:
those semantic providers still remain a separate, incomplete layer.

## Work arithmetic and state

    block_work(T) = floor(2^256 / (T + 1))

Work is a bounded 256-bit unsigned integer stored as 32 big-endian bytes.
A fixed 33-byte remainder and denominator implement binary long division of
2^256, with no dynamic allocation. Examples:

- T = 2^255-1 contributes 2.
- T = 2 contributes a 32-byte value of repeated 55 hex.
- T = 1 contributes 2^255.

Harder targets never contribute less work; floor rounding can give equal work
for adjacent targets. Addition uses carry checks and rejects overflow without
wrapping or changing output. Two hardest-target blocks exceed the selected
256-bit accumulator and are explicitly rejected. This bound is an engineering
limit, not a block reward or a coin-supply ceiling.

Chain state adds current_target and cumulative_work. EMPTY requires both zero.
Each accepted version-3 block, including genesis, contributes work. Failed or
unresolved blocks contribute none. Fixed-target prefix consistency requires
cumulative_work == block_work(T) * (height + 1), with checked bounded arithmetic
that also handles UINT64_MAX height without overflowing a uint64 block count.
This formula checks stored metadata; candidate acceptance still requires real
PoW per block. Work is not substituted with height or block count.

All full-sequence operations remain atomic. Untrusted loaded history must be
revalidated from EMPTY; consistency checks cannot authenticate forged metadata.
Fork choice now compares revalidated cumulative work and produces an in-memory plan; see [FORK_CHOICE.md](FORK_CHOICE.md). The persistence layer can now apply a revalidated plan; see [PERSISTENCE.md](PERSISTENCE.md).

## Genesis

Version-3 genesis is explicit exact bytes supplied in context and receives no
PoW exemption. It must satisfy the same target and body checks as successors.
Tests use independently specified, fixed zero-nonce/zero-timestamp genesis and
child headers, verified with real SHA-256. No search loop or current-time genesis
creation exists. These fixtures are not final production genesis.

Independent expected identifiers were calculated using .NET SHA256 over the
specified wire bytes, separately from the CNG adapter under test:

- Transaction: 523e35d5006fe26eb9308e53d08455036a2ba3c5366f9bc2eb307161846022f1
- Body: ecf91bfa6a4e06d86a24d636eed25f01cf302949e85351d437e04c4e49219a76
- PoW genesis: 368d0a851ef34ec12ed69475830f4728cb94d47c6e9b953254bb3591c15e2825
- Child: 35eb9fa251a3eeca9412b3c23159fbdbf26d874fa9af8969341d2ef67d8c8726

The independent fixture construction is a wire-level cross-check, not an
independent cryptographic backend certification.

## Qualification and limits

Executed: official known-answer digest checks for empty input, abc, the
multi-block abcdbc... message, and one million a bytes; split-span equality;
independent IDs; serialization preservation; changed witness/nonce IDs; target
bounds; exact comparison; all 255 power-of-two target/work relationships;
non-power-of-two division; cumulative work; overflow; provider failure paths;
fixed-target transition rejection; corrupt work-state rejection; complete and
incremental PoW-chain agreement; atomic failure and legacy regressions.

Real SHA-256 handles production-path tests. Clearly named fake providers handle
exact hash equality, impossible work boundaries, and errors; they are not
production defaults. Windows x64 is tested; other platform adapters and an
independent security review remain unverified.

Deferred: mining/search loops, hardware detection, CPU/GPU/ASIC miners, Stratum,
job distribution, rewards/payouts, issuance, treasury, fees/gas, difficulty
adjustment, networking, discovery,
RPC, scalable persistence, wallets, explorers, and contract execution. This increment
stops at deterministic hashing, work verification and local chain accounting.

## RPC mining work

The configured mining service constructs canonical templates, exposes target and deterministic work ID, and atomically accepts solved blocks through normal full consensus validation and persistence. Only the 8-byte big-endian nonce at block offset 152 may change. See [MINING_WORK.md](MINING_WORK.md). No mining loop, Stratum or economics are implemented.

## Phase 11 Chunk 1 — historical calculation-only checkpoint

The owner's Authorized Development Parameters for Phase 11 resolve calculation
parameters previously open in O-011. `stn_target_next` is a pure ISO C17 primitive;
normal block validation and mining templates do not call it. Existing single
SHA-256, block/work identities, full-target comparison, nonce and fixed-target
chain-work/fork-choice behavior are unchanged. Calculation qualification does
not activate dynamic difficulty.

| Parameter | Authorized value |
| --- | --- |
| Intended average block interval | 60 seconds |
| Adjustment window / expected span | 60 blocks / 3600 seconds |
| Candidate boundary | H >= 60 and H modulo 60 = 0 |
| Historical endpoints | accepted heights H-60 and H-1 |
| Measured span | end timestamp minus start timestamp |
| Non-increasing endpoints | use minimum span, without unsigned subtraction |
| Span clamp | 900 through 14400 seconds |
| Calculation | floor(previous target * clamped span / 3600) |
| Target clamp | 1 through 2^255-1 |
| Bootstrap | configured/genesis target at heights 0 through 59 |
| Non-boundary after bootstrap | predecessor target unchanged |

The specified endpoints contain 59 timestamp gaps, while the authorized expected
span is 3600 seconds. This is intentional implementation of the supplied rule;
it is not silently changed to 3540 seconds. No partial-window estimation occurs.

The API receives candidate height, bootstrap target and decoded, already accepted
headers. Caller must establish history acceptance and ancestry; this calculation
neither authenticates headers nor reads storage. Supply no history at height zero,
one predecessor at ordinary heights, and exactly 60 ascending contiguous headers
at adjustment boundaries. Heights, v3 versions and target domains are checked;
bootstrap history through the first boundary must match the bootstrap target.
Missing history returns UNRESOLVED, excess/wrong-height/version history CONTENT,
invalid target TARGET and null mandatory arguments ARGUMENT. No fallback target
is published on error; output remains unchanged. Output may alias input because
publication occurs after all reads. No local configuration or clock is consulted.

Arithmetic reads canonical unsigned big-endian bytes directly. Base-256 scalar
multiplication uses 34 bytes (272 bits) for the at-most-269-bit product. Its
uint32_t step is bounded by 3,686,399. Big-endian long division by 3600 has steps
bounded by 921,599; the final remainder is discarded explicitly. Clamp the complete
quotient before narrowing to 32 bytes, and map zero to target one. There is no
native large-integer requirement, allocation, float, dependency or signed overflow.

The existing test_pow.c harness adds 193 checks including exact/faster/slower
spans, both clamp edges, UINT64_MAX spans/heights, non-increasing endpoints,
bootstrap and later boundaries, failure atomicity, aliasing and repeated outputs.
Inspectable vectors include 10000 at 3601 seconds -> 10002; 7 at 1800 -> 3;
1 at 900 -> 1; and hexadecimal 7fff...ffff at 900 -> 1fff...ffff.
A mixed-byte 256-bit vector verifies every byte participates without endian loss.
Windows Release/x64 only: total Chain C checks 1,130,287, plus 34 build probes.
Dynamic consensus activation, template changes and Phase 11 Chunk 2 remain deferred.
## Phase 11 Chunk 2 — required-target consensus activated

The authorized Chunk 1 calculation is now enforced by ordinary v3 validation and
mining. Both call stn_chain_required_target, which uses only the evaluated
branch's validated history and delegates all adjustment arithmetic to
stn_target_next. The configured fixed_target field is now the bootstrap/genesis
target, not a post-bootstrap override. Heights 0..59 retain it; height 60 uses
accepted heights 0 and 59; subsequent multiples of 60 use H-60 and H-1.

State retains at most 60 decoded calculation records for the current period.
Only successful block validation appends them; the next period starts after
accepting its boundary block. Missing/wrong-height/invalid records fail closed.
These records are derived state, never a persisted difficulty authority. Storage
and fork evaluation reconstruct them from canonical blocks from EMPTY. Storage
plan comparison also checks this derived history. No storage format changed.

Target equality is checked separately before PoW: an easier, harder or previous
period target is rejected even when real SHA-256 satisfies that encoded target.
Mining checks next-block cumulative-work capacity using the required target and
copies the same bytes into its canonical candidate. STNC 0x2002/0x2003 layouts
and work-ID hashing remain unchanged. INFO continues reporting accepted tip state.

Cumulative work remains the checked sum of floor(2^256/(T+1)) per accepted block.
The fixed-target height formula remains a bootstrap consistency check only;
afterwards, full historical revalidation establishes cumulative work. There is
no altered fork preference or wrapping arithmetic. Hardest target 1 remains the
calculation clamp (covered in Chunk 1), but a 60-block history at targets small
enough to adjust to 1 cannot fit the existing 256-bit cumulative-work domain.
Such histories/continuations fail the existing work-overflow checks; this chunk
does not invent an exception or claim accepted minimum-target window qualification.

Qualification: 1,658 new C checks use real SHA-256 histories for exact 3600,
non-clamped 1800/7200, minimum-span and maximum-span cases, and the easiest-target
clamp. They verify STNC template bytes, repeatable IDs, independently solved
wrong-target rejection, corruption/missing-history failure, accepted adjusted
persistence/reload, branch-specific reorganization and the second boundary at
height 120 with continuation to 122. Existing minimum-target arithmetic and
work-overflow tests remain passing.

The existing process harness -DifficultyOnly adds 834 checks: accepted bootstrap
history to 59, exact adjusted STNC/STNM target at 60, actual Stratum submission,
accepted persistence, recovery without the coordinator and new work at 61.
Scripted identity/result fixtures remain test-only; Stratum source is unchanged.
All 1,131,945 Chain C checks, 34 probes, 1,544 Phase 9 and 562 prior Phase 10
checks pass. Windows Release/x64 builds have zero warnings/errors/failures.

Compatibility: old fixed-target v3 histories beyond a boundary are accepted only
if they satisfy the now-required targets. There is no automatic migration,
reset, alternate consensus profile or recovery bypass. Canonical history remains
authoritative. Other platforms, production identities, hardware, economics and
Phase 11 Chunk 3 are not qualified or implemented here.
## Phase 11 Chunk 3 — 320-bit work and development-history boundary

Authorized consensus rule (2026-09-10): cumulative work is an exact unsigned
320-bit integer encoded as exactly 40 big-endian bytes. Target/hash/work-ID widths
remain 32 bytes. The target domain remains 1 through 2^255-1, and per-block work
remains floor(2^256/(T+1)). At most 2^64 blocks, including height-zero genesis,
each contribute at most 2^255 work, so every supported history fits within 2^319.
No valid history needs saturation, wrapping, truncation or an artificial ceiling.
The generic addition API still rejects out-of-domain 320-bit overflow atomically;
that guard cannot be reached by valid cumulative work over supported heights.

stn_work now holds 40 canonical bytes. Addition visits every byte; target work
is zero-extended into that representation. Fork comparison compares all 40 bytes.
Genesis and target-one successors can accumulate past 2^256 with exact ordering.
Current-window difficulty and the authorized adjustment formula are unchanged.

STNC and STNP use wire version 2, explicitly rejecting version 1 rather than
silently interpreting its narrower work fields. STNC INFO is 184 bytes: work
at 104..143, target at 144..175, status at 176 and count at 180. SUBMIT_WORK success
is 80 bytes: block ID 32, height 8, work 40. STNP STATE is 84 payload bytes:
height 8, tip 32, work 40, count 4. All work fields are unsigned big-endian with
leading zeros required. Wrong lengths are rejected at the relevant message
boundary. Other payload layouts, STNM, target bytes and work-ID semantics remain.

Persistence stores canonical blocks rather than cumulative-work metadata, so its
format is unchanged. Reload and reorganization reconstruct exact 40-byte sums
from accepted history; no serialized difficulty/work cache becomes authoritative.
The Stratum client version and affected response bounds were updated, including
the historical adapter buffer. Mining job/result formats were not redesigned.

Pre-Phase-11 fixed-target chains were explicit development/test evidence, with
no established compatibility guarantee. If their targets violate the activated
rule, revalidation rejects TARGET at the actual boundary. They are not described
as byte-corrupted merely because consensus evolved. No rewriting, bypass,
automatic migration or alternate work ordering is introduced. Development history
that already satisfies current rules remains eligible for normal revalidation.

Qualification uses arithmetic boundaries and scripted block-ID fixtures for
otherwise computationally infeasible target-one blocks. The tests cover full
height range at target one, adjacent minimum targets, high-bit comparison,
addition boundary/failure atomicity, 61 minimum-target blocks, canonical storage
reconstruction, high-work STNC/P2P serialization and invalid lengths/old versions.
Real SHA-256 adjusted branches retain reorg preference after storage reload.
No hardware or production-identity qualification is implied.
## Phase 11 Chunk 4 — final qualification (2026-09-10)

Phase 11 is COMPLETE on Windows Release/x64. The actual Winsock/NTFS peer harness now qualifies divergent 1,800/7,200-second histories at height 60, exact branch-derived targets, rejection despite encoded-target-valid PoW, reorganization and reconstructed mining targets. A separate scripted block-hash fixture exercises work beyond 256 bits; it is not a claim of mining those hard targets. Formula, parameters and 320-bit work rules are unchanged. See ROADMAP.md for counts and qualification limits.
