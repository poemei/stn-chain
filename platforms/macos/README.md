# macOS backend boundary

macOS host detection is explicit. No macOS runtime provider is implemented or
qualified. Application composition fails with STN_BACKEND_MACOS_NOT_IMPLEMENTED;
Windows is never a fallback. Reserved architecture directories remain x64/ARM64.

Future providers must implement the existing hashing, transport, exclusion and
atomic storage contracts and reproduce the same canonical vectors. Core code
is shared without consensus forks. See [portability requirements](../../docs/PORTABILITY.md).
