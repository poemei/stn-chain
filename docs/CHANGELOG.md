# Changelog

All meaningful project changes are recorded here. Entries under Unreleased
are not claims of a published or deployed release.

## Unreleased

### Added

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
