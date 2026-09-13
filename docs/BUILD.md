# Visual Studio Build

## Phase 9 test-only runtime

The normal Release/x64 build excludes scripted providers. For the authorized
integration proof, build the same application project with the explicit property
`Phase9TestRuntime=true`. This produces `stn-chain-phase9-test.exe` and uses a
separate intermediate directory; it does not replace `stn-chain.exe`.

From a Visual Studio developer command prompt:

```text
MSBuild stn-chain.sln /m:1 /p:Configuration=Release /p:Platform=x64
MSBuild stn-chain.vcxproj /m:1 /p:Configuration=Release /p:Platform=x64 /p:Phase9TestRuntime=true
powershell -NoProfile -ExecutionPolicy Bypass -File tools/test-phase9.ps1
```

The fixture requires the script's private named shutdown event, announces its
scripted identity hooks on stderr, and binds only to loopback through the existing
runtime. It asserts signature success for a fixed test marker, resolves one test
signer, and supplies a scripted replay hook; existing active-chain replay checks
still execute. It performs no real signature verification. The script separately
proves production admission stays unavailable, then exercises real TCP, CNG PoW,
atomic NTFS persistence, clean shutdown/restart, concurrency, and height 65.
Temporary chain files and processes are cleaned up. Test outputs remain in build/.
This test fixture is not a production deployment or identity-provider implementation.

## Normal Visual Studio build

Open stn-chain.sln in the repository root with Visual Studio 2026 and the
Desktop development with C++ workload. The project compiles .c files as
ISO C17, not C++.

Release | x64 is currently the only solution configuration. Use Build >
Build Solution, then Debug > Start Without Debugging to run the console
development node. Use the configured development launch arguments or run-dev.cmd to start loopback RPC; no arguments print usage.

## Configuration

- Platform toolset: v145 (installed MSVC 14.51.36231).
- Windows SDK: 10.0.26100.0.
- Optimized Release build, warning level 4, warnings treated as errors.
- Static release C runtime; no third-party runtime dependencies added.
- Debug symbols retained for diagnosis of Release builds.
- Windows CNG SHA-256 adapter links the OS bcrypt.lib import library.
- Executable: build/x64/Release/stn-chain.exe.
- Intermediate files: build/obj/stn-chain/x64/Release/.

The solution, project, and Solution Explorer filters are in the repository
root. Headers live in includes/ and implementation files in src/. Add new
files to the native project through Visual Studio and keep its filters
aligned. No modules directory is needed for the current scaffold; introduce
one only when an actual component boundary requires it.

The header stn_platform.h checks the C language level, byte width, and
exact-width integer sizes at compile time. It does not contain OS APIs.

platforms/ reserves Linux and Windows x86/x64/ARM32/ARM64 boundaries and
macOS x64/ARM64 boundaries. Only Windows x64 is currently configured.
See [platform layout](../platforms/README.md).

## Record codec tests

After building Release | x64, right-click stn-chain-tests in Solution Explorer
and choose Set as Startup Project. Use Ctrl+F5 to run it. It prints check and
failure counts and returns a nonzero exit code if a check fails. Checks remain
enabled in Release builds; they do not use the disabled NDEBUG assert macro.
Select stn-chain as the startup project again to run the development node.

The test executable is build/x64/Release/stn-chain-tests.exe. It exercises an
independent byte fixture, round trips, all fixture truncations, capacity and
length failures, unsupported fields, empty and maximum payloads, zero nonces,
and invalid pointers. Intelligence tests add payload field and domain-label
checks, exhaustive one-byte classification/subject alphabets, all maximum-size
payload truncations, and nested envelope/payload checks. Current result:
230 envelope checks, 2,027 payload checks, and 462 validation-context checks,
with zero failures (2,719 total). Context tests cover time, network, stage
ordering, missing/rejecting/error providers, and conditional acceptance.
Provider test doubles are not cryptographic verification or consensus.

The transaction/block suite adds 1,118,656 checks, including every byte
truncation of maximum-size containers, for 1,121,375 total checks. It tests
independent wire fixtures, counts, lengths, reserved fields, provider failures,
duplicate IDs, body commitments, and nested payload integration. Hash-provider
test doubles provide no cryptographic guarantees. All original 2,719 checks
remain unchanged in behavior.

Chain-context tests add 1,178 checks, giving 1,122,553 total with zero failures.
They cover sequential links, exact genesis, provider failures, atomic state
updates, deterministic failure locations, timestamp boundaries, and 64-block
batches. This is local validation under test hashing, not distributed consensus.

The SHA-256/PoW/work increment adds 323 checks, giving 1,122,876 total with
zero failures. Known-answer and linked-chain cases use real Windows hashing;
error/equality/overflow cases also retain declared test providers. See POW.md.

## Validation

The Release/x64 solution was built with the installed Visual Studio MSBuild
and the resulting executable was run successfully. No compiler warnings or
errors were emitted. The owner also confirmed the original scaffold built
and ran in Visual Studio. The codec test project was verified through MSBuild
and direct execution; its interactive IDE run is not yet confirmed. No
cryptographic validation or non-Windows qualification is claimed.

## Automation

MSBuild can build the same solution with Configuration=Release and
Platform=x64. The IDE remains the primary development workflow. Generated
output and local Visual Studio state are excluded by .gitignore.

Linux/macOS builds are still planned, not implemented or qualified. No
software release number has been assigned. Release is a build configuration,
not a declaration that the software is production-ready or published.

Fork-choice increment: adds 785 checks (1,123,661 total), zero failures in
Windows Release/x64. Both native projects include stn_fork.c; the test project
adds test_fork.c. No additional dependency or build configuration is required.

Persistence increment: adds 2,486 checks (1,126,147 total), zero failures in
Windows Release/x64. Tests create private temporary files on local NTFS and
exercise the real adapter plus injected failures. Kernel32 supplies existing
Windows file APIs; no new package is downloaded. See PERSISTENCE.md for limits.

Peer increment: ws2_32.lib is linked alongside bcrypt.lib. Added 145 checks
(1,126,292 total), zero failures in Release/x64. The suite uses private NTFS
files and ephemeral loopback sockets for a two-endpoint real-hash synchronization
test; no Internet access or public listening is required.

Portability increment: the test-project pre-build event runs 34 compile/boundary
probes through provisioned MSVC. Runtime suite adds 93 checks: 1,126,385 runtime
checks, plus 34 build probes (1,126,419 combined), zero failures. Windows backend
headers now reside in platforms/windows, included by the native project paths.
Only Release/x64 remains qualified; simulated probes do not build other targets.

The pre-build helper uses process-scoped PowerShell ExecutionPolicy Bypass to
run this repository's local verification script. It does not change the machine
or user execution policy, and it performs no network downloads.

RPC increment: adds 237 runtime checks, giving 1,126,622 runtime plus 34 build
probes (1,126,656 combined), zero failures. That earlier checkpoint tested RPC in-process; the mining increment below adds a real executable listener. Existing localhost P2P/NTFS integration still passes.
Both projects compile portable stn_rpc.c and stn_node_service.c.


## Mining runtime checkpoint

Build Release | x64, set stn-chain as startup project, and run with the supplied --dev debugger arguments (unless overridden in local VS settings). Alternatively double-click run-dev.cmd in the repository root. It serves binary RPC on 127.0.0.1:18473 and persists the explicit development fixture. See [MINING_WORK.md](MINING_WORK.md).

Run tools/test-node.ps1 with PowerShell process-local -ExecutionPolicy Bypass for the real executable/TCP/NTFS restart checks. This is separate from the C test executable. No persistent execution-policy change is needed.

Validated: 1,127,559 C runtime checks plus 42 executable integration checks = 1,127,601 runtime checks; 34 build probes; 1,127,635 total; zero failures. The Release/x64 solution built with zero warnings/errors. All 1,126,622 prior runtime checks remain passing.

## Long-running runtime/history qualification (2026-09-08)

Release/x64 built with zero warnings/errors. C test executable: 1,127,582 checks.
Existing tools/test-node.ps1: 872 checks (includes an intentional 61-second idle
and partial-frame regression). Runtime total: 1,128,454. Build/boundary probes:
34. Combined: 1,128,488, zero failures. All prior regressions remain passing.
Only Windows Release/x64 is qualified. See MINING_WORK.md and PEER_PROTOCOL.md
for tested chain lengths and runtime/protocol limits. Test artifacts remain in
private build-directory locations and are removed by the existing test harness.

## Phase 10 final integration qualification

After building Release/x64, the separate Phase9TestRuntime=true target and
STN-Stratum with its build.cmd, run tools/test-stratum-interface.ps1
-FullLifecycleOnly for the 125-check complete persistence/recovery proof.
Run the same script without switches and with -JobMappingOnly, -MinerResultOnly,
and -FailureStateOnly separately for the prior 437 checks. These runs must be
sequential: configured loopback ports 18473/18475 must be free. Fixtures use
private temporary state and clean their processes/files. Also run test-phase9.ps1,
stn-chain-tests.exe and Stratum's parser/session tests. Qualification is limited
to Windows Release/x64 with test-only identity/result fixtures.
## Phase 11 Chunk 1 calculation checks

Build the existing Release/x64 solution and run stn-chain-tests.exe as before.
The existing PoW suite includes 193 targeted difficulty-calculation checks (516
PoW-suite checks total); total Chain C checks are 1,130,287. No additional test
executable or build dependency is required. The separate Phase 9/10 process suites
continue to qualify unchanged fixed-target behavior, not dynamic activation.
## Phase 11 Chunk 2 activation checks

The existing mining test suite adds 1,658 C checks (1,131,945 Chain C total).
Build the solution Release/x64 and the separate Phase9TestRuntime=true executable.
Run tools/test-stratum-interface.ps1 -DifficultyOnly after building Stratum:
834 real-process checks exercise the height-60 adjustment and restart to new
height-61 work. Run it separately from other process tests because the same
loopback ports are used. The shared fixture solver now compares all 32 target
bytes. Earlier storage/peer/long-history fixtures now obey active retarget rules.
No new executable, temporary harness or Stratum source change is needed.
## Phase 11 Chunk 3 — protocol v2 builds

Build Chain Release/x64 and the separate Phase9TestRuntime=true runtime, then
rebuild Stratum before the existing cross-process tests. Both endpoints now
require STNC v2; STNP peers require v2. Old v1 tools are explicitly incompatible.
The same C harness and process scripts qualify 40-byte work. No new dependency
or standalone test executable is required; target-one tests use scripted block
hashes, while existing adjusted-window integration retains real SHA-256.
Chunk 3 results: 1,132,168 total C checks (223 new), 34 probes, 1,544 Phase 9,
562 Phase 10, 834 adjusted-target process checks, 27 Stratum parser checks and
two session assertions. Windows Release/x64 only; zero warnings/errors/failures.

## Phase 11 Chunk 4 — final qualification (2026-09-10)

The existing stn-chain-tests.exe includes 1,109 new peer convergence checks (1,282 peer checks; 1,133,277 total C checks). Build/rebuild the Release/x64 solution in Visual Studio and run the existing executable. No new executable or dependency is needed. The harness uses ephemeral loopback sockets and private NTFS directories, then removes its files. Run the existing Phase 9 script, all five Phase 10 modes, and -DifficultyOnly sequentially as described above. Results: 34 probes, 1,544 Phase 9, 562 Phase 10, 834 adjusted-target process checks, 27 Stratum parser checks and two session assertions, all passing. Windows Release/x64 only; zero warnings/errors/failures. See ROADMAP.md for the real-hash versus scripted high-work fixture distinction.

## Phase 12 Block 2 — outbound runtime

Build Release/x64 in the existing Visual Studio solution. To configure outbound
P2P, add repeated `--peer IPv4:PORT` arguments, for example:

```text
stn-chain.exe --dev --data chain.stns --peer 127.0.0.1:19000 --peer 127.0.0.1:19001
```

Those endpoints must serve STNP v2 with the selected network/genesis; they are not
RPC ports. `--peer` is incompatible with `--once`. Without peers no worker starts.
The candidate bound remains 64; duplicate endpoints coalesce. No DNS or discovery.
The worker attempts/refreshes no more than once per five seconds and preserves
one usable outbound session. Failure advances to the next canonical candidate.
See PEER_PROTOCOL.md for scratch capacity, deadlines and RPC serialization.

Run the existing C executable and `tools/test-node.ps1 -OutboundOnly` for 165 C
and 30 process checks specific to this block. The latter uses ephemeral localhost
ports and private files, verifies session reuse/reconnect and partial-response
isolation, and disposes its test processes. Existing Phase 9/10/11 process suites
were also run once against the updated runtime. No other-platform qualification.

## Phase 12 Block 3 — peer discovery

STNP v2 discovery uses types 7 and 8; capability bit 2 advertises support. After
existing synchronization, the outbound manager performs at most one bounded
`GET_PEERS` request per connection. `PEERS` is a two-byte count followed by six
bytes per IPv4 endpoint, maximum 386-byte payload. Entries pass the existing
64-entry candidate store and never bypass Chain validation.

The focused C run reports 955 codec/policy and 19 real-Winsock discovery checks.
`tools/test-node.ps1 -DiscoveryOnly` reports 40 checks using ephemeral local peers,
including discovered-peer failover and malformed-discovery isolation. No external
seed, DNS, multicast, NAT, gossip or learned-peer database is involved.

## Phase 12 Block 4 — portable orchestration boundary

The shared candidate, discovery and outbound-policy interfaces compile as ISO C
without Windows headers or OS handles. `test_portability` now round-trips the
fixed-width candidate/discovery values and checks the explicit five-second pacing
constant. Windows Release/x64 remains the only runtime qualification; Linux,
ARM and macOS adapters are still reserved and were not implemented in this block.
The portability addition contributes seven checks to the existing C qualification.

## Phase 12 final integration qualification

The combined bounded scenario uses real loopback Winsock/STNP paths: a configured
candidate connects, one discovery exchange admits another endpoint, the existing
outbound manager uses that endpoint after loss, and invalid discovery/evidence
cannot alter accepted or persisted state. Blocks 1–4 contribute 1,586 focused C
checks, 70 focused executable checks and 27 pending-RPC checks. The complete Chain C total is 1,134,863;
Windows Release/x64 is the only qualified runtime. Phase 12 is complete and Phase
13 has not started.

## Phase 13 Block 1 — RPC framing preflight

Build the Release/x64 solution and run `stn-chain-tests.exe`. The RPC suite now
includes three preflight checks for exact header length, valid declared payload,
and maximum-plus-one rejection. The running Windows node applies the same helper
before reading persistent or `--once` RPC payloads. Existing Phase 9/10/11
process suites remain the affected regression set; no other-platform qualification
is claimed.

## Phase 13 Block 2 — bounded incomplete-frame sessions

Build the Release/x64 solution and run the existing test executable, then run
`powershell -ExecutionPolicy Bypass -File tools/test-node.ps1 -FramingOnly`.
The focused runtime check leaves one client with a partial frame for the
bounded 60-second operation deadline, verifies that session closes, and checks
that an independent RPC client remains usable. The Windows transport deadline
is platform plumbing; framing and dispatch remain portable.

## Phase 13 Block 3 — complete-frame session continuity

Run `powershell -ExecutionPolicy Bypass -File tools/test-node.ps1 -FramingOnly`
after the Release/x64 build. The focused run performs 33 checks for consecutive
complete requests, distinct response identifiers, deterministic unsupported and
malformed request errors, incomplete-header disconnect, Block 2 deadline
termination, and independent-client continuity. The existing
pending-RPC and full C suites remain the affected regressions.

## Phase 13 Block 4 — deterministic protocol errors

The same `-FramingOnly` run verifies deterministic unsupported-method and
malformed-complete-request responses, request-ID association where the request
decodes, a valid request after protocol errors, and continued independent
client operation. Transport disconnects remain session failures rather than
STNC status responses.

## Phase 13 Block 5 — RPC lifecycle churn

After the Release/x64 build, run
`powershell -ExecutionPolicy Bypass -File tools/test-node.ps1 -LifecycleOnly`.
The focused runtime qualification performs 153 checks across pre-request
disconnects, repeated request/teardown cycles, reconnects, request-ID
association, and an independent healthy session.

## Phase 13 Block 7 — sustained concurrent RPC load

After the Release/x64 build, run
`powershell -ExecutionPolicy Bypass -File tools/test-node.ps1 -ConcurrencyOnly`.
The focused runtime qualification opens 64 simultaneous clients, performs two
requests per client, closes them, and verifies a healthy session remains
usable. It performs 1,037 checks without adding a protocol connection cap.

## Phase 13 Block 8 — final integration qualification

The closeout evidence combines the focused framing, lifecycle, concurrency, and
pending-RPC runs with the full Chain C executable and existing Phase 9–12 and
Stratum evidence. Windows Release/x64 remains the only qualified execution
environment; Phase 14 was not started.

## Phase 14 Block 1 — identity/signature foundation

Build Release/x64 and run `stn-chain-tests.exe`. The identity suite covers the
RFC 8032 vector, canonical identity derivation, statement construction,
mutation/wrong-key rejection, malformed lengths, noncanonical encodings, and
fail-closed behavior. The provider is vendored under `src/crypto/` and uses no
network or runtime service dependency.

## Phase 14 Block 2 — deterministic authority foundation

The Release/x64 test executable includes the authority suite. It qualifies
canonical identity/action/context/evidence handling, exact scope matching,
missing authority, malformed/truncated/oversized/unsupported evidence, and the
separation between valid signatures and authority. No additional platform
provider or authority service is required.

## Phase 14 Block 3 — authority-grant provenance

The Release/x64 test executable qualifies canonical 194-byte grants, the
genesis root set, the dedicated signing domain, valid root-issued grants,
non-root issuers, signature/content mutations, malformed lengths, and Block 2
composition. Test roots are explicit fixtures; production does not receive a
default or local-configured root.

## Phase 14 Block 4 — authority-grant revocation

The Release/x64 test executable qualifies canonical grant identifiers, the
129-byte revocation codec and signing domain, issuer matching, malformed input,
root authorization, duplicate application, revoked-grant rejection, and
reconstruction by replaying accepted evidence. Persistence and reorganization
remain history-derived; no local revocation database is introduced.

## Phase 14 Block 5 — identity rotation

The Release/x64 test executable qualifies the 129-byte rotation codec and
dedicated signing domain, active-identity authorization, sequential lineage,
duplicate/conflict handling, cycle rejection, root-rotation rejection, and
current-identity reconstruction. Historical signature and grant behavior remain
separate from current identity state.

## Phase 14 Block 6 — signed-action replay protection

The Release/x64 test executable qualifies canonical signer/nonce replay IDs,
fresh versus replay results, accepted-state-only consumption, reconstruction,
malformed inputs, and caller-owned state capacity handling. Existing grant,
revocation, and rotation lifecycle tests remain part of the same regression
gate; production transaction/history integration remains deferred.

## Phase 13 Block 6 — deterministic shutdown

The Release/x64 build includes listener-close-before-worker-join shutdown
ordering. Run the lifecycle qualification after the build; it confirms active
session cleanup and independent-client continuity while the full C test
executable remains the regression gate.

## Phase 14 Block 7 — lifecycle transaction integration

Lifecycle transactions use the existing 12-byte transaction header and direct
qualified payloads. No second envelope or serialization format was added.
Pending admission remains evidence handling; accepted-history reconstruction is
the authority and identity state transition boundary. The lifecycle replay
nonce is deterministic SHA-256 over the lifecycle replay domain, transaction
type, and canonical signed-statement fields.

## Phase 14 Block 8 — production lifecycle state integration

The Chain context accepts a separate Genesis Initial Identity Set of at most 16
sorted unique canonical identities. Lifecycle transactions are applied to a
candidate-local state clone and published atomically with accepted Chain state;
rejected candidates cannot consume replay state or alter authority/identity
state. The Release/x64 lifecycle qualification covers production candidate
integration, initial-set validation, and accepted-state propagation. No Block 9
or Phase 15 work is included.

## Phase 15 Block 1 — canonical production records (2026-09-12)

Use the existing Visual Studio Release | x64 solution and test startup project.
No new executable, CMake dependency, platform target or production signing tool is
introduced. The local tests use deterministic test-only Ed25519 keys; none are
written to a key file or installed as production credentials. The repository's
existing policy excludes `tests/` from Git; local qualification additions retain
that policy and are not silently force-added.

The full C executable passes **1,135,591 checks**. Phase 15 adds **610** checks:
17 record-ID/token known-answer and boundary checks, 264 activation-history
checks, and 329 signed-publication integration checks. These targeted counts are
already included in the full count; nested pending/difficulty/peer subgroup
summaries must not be counted twice.

The signed fixture covers exact authority action/context matching, invalid
signature, revocation, retired identity, replay, rejected-candidate isolation,
pending selection, 20 successive publications beyond the old 16-entry limit,
storage restart, greater-work branch replacement and replacement restart. It
also passes through real STNC encode/dispatch/admission, work-template generation,
nonce-only solved-work submission and persistence, plus STNP block framing into
the common validator. Network mismatch and provider failure fail closed.

Process qualification retains the existing commands above: Phase 9 (1,544), all
five Phase 10 modes (562), Phase 11 `-DifficultyOnly` (834), pending RPC (27),
Phase 12 outbound/discovery (30 + 40), RPC framing/lifecycle/concurrency
(33 + 153 + 1,037), and Stratum parser/session checks (27 + 2). Build boundary
probes remain 34. These are Windows Release/x64 results; simulated compile probes
do not qualify another platform. The unfiltered node test now uses the existing
bounded target-aware fixture solver from height 60 instead of obsolete
fixed-target nonce vectors; no production mining interface changes are involved.

Final unfiltered executable/TCP/NTFS qualification: **926 checks, zero failures**.
Process/Stratum total: **5,215**; C plus process/Stratum: **1,140,806**;
including 34 build probes: **1,140,840**. Release/x64 solution and the separate
Phase 9 fixture build finish with **0 warnings, 0 errors, 0 failures**. The
production lifecycle checks use real Ed25519/SHA-256; old development-hook and
high-work hash-fixture tests retain their documented narrower interpretation.
