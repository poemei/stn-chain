# Changelog

All meaningful project changes are recorded here. Entries under Unreleased
are not claims of a published or deployed release.

## Unreleased

### Validation

- SHA-256/PoW/work increment: final Windows Release/x64 build passed without
  warnings/errors. Added 323 passing checks, including SHA-256 known answers,
  independent IDs, target arithmetic, PoW and atomic cumulative work. Total:
  1,122,876 checks, zero failures; all earlier 1,122,553 checks preserved.

- Windows Release/x64 solution build succeeded with no warnings or errors.
- Record codec tests passed: 230 checks, zero failures; intelligence payload
  tests passed: 2,027 checks, zero failures; validation-context tests passed:
  462 checks, zero failures (2,719 total). Provider hooks use test doubles;
  these results do not establish cryptographic validity.
- Transaction/block suite passed 1,118,656 checks, including exhaustive
  byte-truncation loops for maximum-size containers. Combined suite:
  1,121,375 checks, zero failures; hash-provider tests use a noncryptographic stub.
- Chain-context suite passed 1,178 checks; combined Release/x64 suite passed
  1,122,553 checks with zero failures. All prior 1,121,375 checks retained.
- Application scaffold smoke check passed. Signature verification, payload
  truth/evidence verification, authorization, and consensus are not covered
  by this validation. Payload syntax and field limits are covered.
- Original envelope wire behavior and all 230 existing envelope checks
  preserved; this increment stops at payload and validation-context support.

### Added

- Real SHA-256 Windows CNG adapter behind the existing provider boundary;
  no remote service or bundled third-party dependency introduced.
- Version-3 PoW block profile with full big-endian targets, single-SHA256
  block-ID/work hash, fixed context target, and explicit verification stages.
- Bounded integer work calculation and checked cumulative-work state,
  integrated into atomic candidate/sequence validation including genesis.
- Exact PoW/work documentation and independent fixed real-hash fixtures.
- Legacy version-1 development mode retained explicitly; no mining loops,
  fork choice, persistence, networking, wallets, contracts, or economics added.

- Minimal explicit chain state, exact development-genesis context, and
  provider-bound canonical header IDs without production hashing.
- Candidate validation for structure, contextual links, body integrity,
  expected networks, and nondecreasing development timestamps.
- Bounded sequential validation with atomic output assignment, first-failure
  index/height diagnostics, and fail-closed hash-provider handling.
- Independent genesis fixture and local-chain regressions covering failure
  paths, unchanged state, loaded inputs, and a 64-block development batch.

- Versioned publication transaction wrapping one unchanged record, with
  bounded canonical encoding and reserved future transaction classes.
- Versioned 168-byte block header and ordered length-prefixed transaction
  body; development limits of 16 transactions and 1,051,880 block bytes.
- Separate transaction/header/body/block structural checks, provider-bound
  transaction IDs and body commitments, and duplicate-ID integrity checks.
- Exact wire-layout documentation, fixed independent fixtures, failure-path
  tests, and integration through intelligence, record, transaction, and block.

- Explicit staged validation context for expected network, supplied time
  policy, signature verification, authority lookup, and replay checks.
- Separate stage/final statuses with fail-closed unresolved/error behavior;
  missing providers never yield acceptance and later stages do not run.
- Deterministic observation/publication time rules with overflow-safe bounds,
  hook-contract and stage-order regression tests, and implementation limits.

- Allocation-free intelligence payload encoder/decoder for the development
  schema, with borrowed ASCII spans and exact field-length validation.
- Severity, classification alphabet, source-label, printable subject, and
  nonzero evidence-digest checks; no DNS or external-service calls.
- Intelligence tests covering independent wire bytes, all fixture truncations,
  alphabet/length boundaries, malformed fields, output failure behavior,
  and integration with the outer record envelope.

- Bounded ISO C17 record encoder/decoder with explicit errors, borrowed
  payload views, big-endian fields, length limits, and zero-nonce rejection.
- Native Release/x64 stn-chain-tests project covering independent byte
  fixtures, truncations, invalid inputs, and boundary sizes; checks run in Release.
- Documented the structural codec's separation from cryptographic, payload,
  authorization, network, and replay validation.

- .gitattributes rules for LF C/documentation files, CRLF Visual Studio and
  Windows scripts, and binary asset handling.
- Expanded .gitignore coverage for Windows build products and Visual Studio
  caches while preserving platforms/ architecture directories.

- Root Visual Studio solution, native C project, and filters configured for
  Release/x64 using v145 and Windows SDK 10.0.26100.0.
- includes/ and src/ with ISO C17 compile-time platform checks and a minimal
  console entry point identifying itself as a development scaffold.
- IDE build instructions and ignore rules for generated output/local state.
- platforms/ target directories for Linux and Windows x86/x64/ARM32/ARM64
  and macOS x64/ARM64, with implementation boundaries and support status.
- Successful Windows Release/x64 scaffold build and executable smoke check.

- Concrete C17/local-build and cryptographic-provider proposals, with
  dependency evaluation and cross-platform qualification gates.
- Proposed binary record envelope with explicit byte offsets, signing
  domains, bounded lengths, and network-bound identities.
- Proposed synthetic intelligence payload, authorization/replay behavior,
  and positive/negative qualification cases.

- Mission and scope for intelligence distribution, company decisions,
  policies, doctrine, contracts, and possible future mining economics.
- Proposed component architecture separating deterministic validation,
  node coordination, storage, networking, integrations, and mining.
- Decision register separating established direction from unresolved
  protocol and economic choices.
- Delivery milestones with verification gates.
- Repository instructions requiring maintenance of this changelog.
- Supplied version-numbering and sovereignty policy references, with
  release rollover rules and dependency continuity requirements.

### Changed

- Adopted the owner's Visual Studio IDE-first Windows workflow with native
  solution/project files planned; removed CMake/CTest as the recommended
  build/test path and retained a separate portable Unix build proposal.

- Reframed README around the broader STN Chain mission and current
  documentation-only status.
- Clarified that PoW activation and native-coin economics remain undecided.

## 2026-09-06

### Changed

- Replaced MIT for current material with the STN Chain Proprietary
  Participation License; documented permitted unchanged participation
  and the restriction on modifications (0020931).
- Moved the license into docs/LICENSE.md and linked it from README (815d183).
- Renamed project documentation to STN Chain and described the ISO C
  redesign direction (8351c16).

### Removed

- Historical Go implementation to begin a fresh redesign (0a7ae52).
