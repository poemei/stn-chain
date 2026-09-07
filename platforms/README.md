# Platform Implementation Boundaries

Portable ISO C logic stays in src/ with shared headers in includes/.
This directory holds operating-system and architecture-specific code when
needed: files, sockets, entropy, clocks, synchronization, and device access.

## Target layout

| Operating system | Architecture directories | Current status |
| --- | --- | --- |
| Linux | x86, x64, arm32, arm64 | Reserved; no adapters or qualified builds |
| Windows | x86, x64, arm32, arm64 | x64 Release builds and CNG SHA-256 adapter tested; other targets reserved |
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

Windows now also has a local NTFS snapshot provider in stn_storage_windows.c,
qualified by Release/x64 file tests. Portable coordination remains in src/.
Linux/macOS persistence providers are not implemented. See docs/PERSISTENCE.md.

Windows stn_peer_windows.c provides nonblocking Winsock exact transfers and
finite session deadlines. Loopback synchronization is tested; public Internet
and non-Windows transports are unqualified. Protocol/sync logic remains in src/.
