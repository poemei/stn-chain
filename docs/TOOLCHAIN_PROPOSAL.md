# Toolchain and Dependency Proposal

Status: Visual Studio IDE-first Windows workflow accepted by the owner;
remaining recommendations address O-001 and the implementation part of O-003.
The original proposal below is supplemented by the adopted Windows CNG
SHA-256 adapter in [POW.md](POW.md). Signature-provider selection remains open.

## Language and build

Recommend ISO C17 for the portable core. Require 8-bit bytes and exact-width
uint8_t, uint16_t, uint32_t, and uint64_t types; fail configuration when the
implementation cannot provide them. Portability means a documented supported
platform profile, not every possible conforming C implementation.

Use standard C in core code, without compiler extensions, variable-length
arrays, packed-struct serialization, or required C11 thread support. OS
threads, sockets, secure entropy, files, and clocks belong in adapters.

Use native Visual Studio .sln and .vcxproj files for Windows development.
The normal workflow is to open the solution, select Debug or Release, build,
and run/debug the test executable inside the IDE. Set project compilation
to C with ISO C17 language mode. A console test project can be selected as
the startup project; Test Explorer integration is not assumed or required.

CMake, CTest, and Ninja are not required. Command-line MSBuild remains an
automation path, not a prerequisite for the owner's daily workflow.
Propose a small POSIX shell build script for Linux/macOS that compiles the
same core and test sources with GCC or Clang. Keep source lists and build
settings aligned across paths and check that alignment when sources change.
Record exact compiler, SDK, and project-toolset versions during scaffold
qualification rather than guessing from the installed environment.

Microsoft documents C17 mode and its compiler/SDK requirements in
[C17 support](https://learn.microsoft.com/en-us/cpp/overview/install-c17-support?view=msvc-170).
These sources establish available mechanisms, not STN platform qualification.

## Qualification matrix

| Platform | Proposed compiler family | Initial evidence |
| --- | --- | --- |
| Windows x64 | MSVC; Clang as a second implementation where available | Build, unit tests, byte vectors, warnings |
| Linux x64 | GCC and Clang | Same tests plus address/undefined-behavior sanitizers where supported |
| macOS ARM64 | Apple Clang | Same byte/signature vectors and tests |
| Purpose-built STN systems | Selected per target | Integer/byte assumptions, platform adapter, full vectors |

The Windows scaffold now targets the discovered Visual Studio Community 2026
installation, v145 toolset (MSVC 14.51.36231), and SDK 10.0.26100.0.
See [BUILD.md](BUILD.md). Protocol and cross-platform qualification remain
open; a successful scaffold build is not protocol validation.

## Proposed source boundaries

    includes/            public C interfaces
    src/core/            byte codecs and deterministic validation
    src/crypto/          provider adapter, no protocol policy
    src/node/            chain coordination (later)
    platforms/           OS and architecture adapters (reserved targets)
    tests/               known-answer and negative cases
    docs/                design, decisions, changelog

Do not create placeholder executables or empty modules simply to match this
layout. Start with the codec and its tests after the format is resolved.

Core functions should accept caller-owned byte spans and output buffers,
return explicit error codes, and document lifetimes. Avoid hidden allocation,
process exits, global mutable state, and logging side effects in validation.
Distinguish malformed input, invalid signatures, unavailable provider, and
insufficient output capacity. A provider failure must never mean valid.

## Cryptography candidate

Propose SHA-256 for record IDs and Ed25519 for the first signature suite.
This does not select a PoW algorithm or a monetary ownership model.
Use PureEd25519 over the full domain-prefixed message, not Ed25519ph and
not an ad hoc signing of a digest.

RFC 8032 specifies Ed25519 with 32-byte public keys and 64-byte signatures
and includes test vectors. See [RFC 8032](https://www.rfc-editor.org/rfc/rfc8032.html).

For signatures, evaluate OpenSSL libcrypto through EVP as a provider candidate.
Its documented one-shot Ed25519 API fits the bounded-record design. Pin an
evaluated release and verify the exact API/provider configuration before
adoption. See [OpenSSL Ed25519](https://docs.openssl.org/3.5/man7/EVP_SIGNATURE-ED25519/).

A library name alone does not define consensus verification behavior.
Qualification must cover canonical encodings, scalar bounds, malformed and
small-order points, and cross-provider acceptance differences. The precise
Ed25519 acceptance profile remains an O-003 blocker before consensus use.
Do not implement curve arithmetic merely to compensate for an unreviewed
provider choice.

## Dependency evaluation and exit

| Candidate | Requirement/value | Control and exit | Remaining evaluation |
| --- | --- | --- | --- |
| Windows CNG (adopted for SHA-256) | OS cryptographic primitive, offline operation | Narrow replaceable adapter; identical byte vectors for other providers | Non-Windows adapters and independent review; see POW.md |
| Visual Studio/MSBuild | Owner's native Windows IDE workflow and optional automation | Retain solution/project settings and SDK/toolset requirements; portable core also builds through a separate Unix path | Installed instance, C workload, SDK, and IDE build/run qualification |
| OpenSSL libcrypto | Maintained hash/signature primitives | Narrow adapter, retained source/build recipe, pinned checksums; qualify replacement against identical vectors | Exact release, license notices, patch process, size, verification profile |
| OS SDK/compiler | Native executable and platform services | Document toolchain and SDK versions; retain recoverable build inputs | Installed tools and target support |

No automatic network downloads in the default build. A clean build with
provisioned dependencies must work offline.
Record origin, version, license, integrity digest, build options, and update
procedure for adopted dependencies. Third-party license terms remain intact.

Do not introduce a hosted build, hosted signing, cloud storage, or external
identity service as a prerequisite for core operation. Source hosting assists
collaboration; it is not required to validate or run the resulting software.

## Alternatives and completion gate

C99 reduces language requirements but omits conveniences such as standard
static assertions; C23 is not required by the initial design. Native Windows
projects and Unix scripts require maintaining consistent build knowledge.
This tradeoff follows the owner's accepted IDE-first workflow. A smaller
crypto provider may reduce footprint but needs an explicit security and
maintenance comparison. These tradeoffs are proposals, not settled policy.

Resolve O-001 when compiler discovery, minimum versions, and a local build
plan are recorded. Resolve the provider part of O-003 only after dependency
review and positive/negative vector qualification. No release version is
assigned; wire-format version 1 in the record proposal is not a software
release number.
