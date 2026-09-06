# Platform Implementation Boundaries

Portable ISO C logic stays in src/ with shared headers in includes/.
This directory holds operating-system and architecture-specific code when
needed: files, sockets, entropy, clocks, synchronization, and device access.

## Target layout

| Operating system | Architecture directories | Current status |
| --- | --- | --- |
| Linux | x86, x64, arm32, arm64 | Reserved; no adapters or qualified builds |
| Windows | x86, x64, arm32, arm64 | x64 Release scaffold builds; adapters not implemented; other targets reserved |
| macOS (OSX) | x64, arm64 | Reserved; no adapters or qualified builds |

x86 means 32-bit Intel-compatible; x64 means 64-bit Intel-compatible.
ARM32 and ARM64 are separate targets. Exact ARM ISA/ABI, minimum OS versions,
SDKs, and compiler support must be selected before enabling a target.
Directory presence is not a promise of toolchain or operating-system support.

Place OS-wide shared implementation directly beneath its OS directory;
put code in an architecture subdirectory only when it actually differs.
Do not duplicate portable logic or consensus rules across these directories.
.gitkeep files retain empty planned target directories in Git.

The native Visual Studio project currently selects only Release/x64.
Add other configurations when their toolchain, adapter, and test requirements
are satisfied. macOS does not currently reserve legacy x86 or ARM32 targets.
