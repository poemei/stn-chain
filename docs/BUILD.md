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

## Validation

The Release/x64 solution was built with the installed Visual Studio MSBuild
and the resulting executable was run successfully. No compiler warnings or
errors were emitted. Interactive IDE opening has not been verified; no
protocol tests or non-Windows builds are claimed by this scaffold check.

## Automation

MSBuild can build the same solution with Configuration=Release and
Platform=x64. The IDE remains the primary development workflow. Generated
output and local Visual Studio state are excluded by .gitignore.

Linux/macOS builds are still planned, not implemented or qualified. No
software release number has been assigned. Release is a build configuration,
not a declaration that the software is production-ready or published.
