# SHA-256, Development Proof of Work, and Chain Work

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

The context supplies one fixed_target for the entire development chain.
Each block must match it exactly. No adjustment is active. The policy structure
is the narrow boundary for a future target scheduler; adding a scheduler
requires explicit integer rules and activation. No final mainnet target policy
is established. Tests use the easiest target for a fixed real-hash fixture.

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
