# Linux backend boundary

Linux and x86/x64/ARM32/ARM64 are recognized by the centralized build configuration.
No Linux runtime provider is implemented or qualified. Application composition
fails with STN_BACKEND_LINUX_NOT_IMPLEMENTED; Windows is never a fallback.

Future providers must implement the existing hashing, transport, exclusion and
atomic storage contracts and reproduce the same canonical vectors. Pure core
code remains in src/ with portable declarations in includes/. See
[portability requirements](../../docs/PORTABILITY.md).
