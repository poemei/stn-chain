# Visual Studio Build

Open stn-chain.sln in the repository root with Visual Studio 2026 and the
Desktop development with C++ workload. The project compiles .c files as
ISO C17, not C++.

Release | x64 is currently the only solution configuration. Use Build >
Build Solution, then Debug > Start Without Debugging to run the console
scaffold. It prints its development status and exits; it is not a node yet.

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
Select stn-chain as the startup project again to run the application scaffold.

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
