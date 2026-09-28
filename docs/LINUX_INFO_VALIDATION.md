# Linux INFO availability patch

The replacement production file is `platforms/linux/stn_app_linux.c`.
The uploaded `stn_mining.c` and `stn_mining.h` match the repository byte for byte
and require no changes. The original Linux app also matched the upload before
this patch. Project whitepaper v0.3 (September 21, 2026) and
`development_protocol.md` were used as references; synced sources were not edited.
Changed code carries the required AI annotations.

INFO keeps the STNC v2 24-byte header and exact 184-byte payload: network,
genesis, accepted height, accepted tip, 40-byte cumulative work, target, flags
and accepted block count. It uses a short independent mutex to copy these
bytes. No Chain pointers, storage access, history reconstruction, logging or
network operations occur under that mutex.

Startup publishes only after storage initialization/load succeeds through the
existing validation path. Serialized RPC dispatch and outbound sync publish
the actual accepted state before releasing the dispatch lock. Publication does
not depend on increasing height or on the operation's return code, so a
same-height or shorter preferred reorganization is represented correctly.
During an operation INFO serves the last completed publication; it does not
describe peer claims, candidate progress or synchronization completion.

Other RPC methods still use the existing dispatch lock. A client waiting for
a different method on a sequential connection must finish that request before
INFO on that connection can be handled. Independent INFO connections remain
available. No protocol fields, consensus rules or validation bypasses were added.
The disabled internal-miner path, configuration and mining files are untouched.
This patch's publication points cover the requested external RPC/P2P operation;
enabling internal mining is outside its scope.

## Checks completed here

- The focused helper test compiled using MSVC C17, `/W4 /WX`.
- 40,039 checks passed: exact fields/correlation/length, unavailable state,
  a held synchronization lock, concurrent coherent publication, same-height
  and lower-height snapshots, count overflow, malformed framing, and the
  existing dispatch path for non-INFO requests.
- The Windows runner extracts the exact production INFO helpers and two
  standalone codec dependencies, using SRW locks in place of pthread mutexes.
  Synthetic states exercise serialization/concurrency only, not consensus.
- The Python network test passed syntax validation, but has not run here.
- The standard Windows `build.cmd` compiled its configured sources but failed
  linking 12 existing missing symbols in compensation, reward, pending,
  contract-query and transaction-status code. Those build omissions were not
  changed as part of this Linux INFO fix.
- There is no configured Linux runtime on this host, so a complete Linux build
  and real pthread/socket testing are not claimed. No Linux binary is supplied.

Repeat the Windows helper check from the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/test-linux-info.ps1
```

## VPS build and tests

Copy the supplied full files over their matching repository paths. Only
`platforms/linux/stn_app_linux.c` is required for the production fix; the other
files provide test/build support and documentation. Keep your existing miner
configuration. This package does not include or change configuration files.

From the Linux repository root, with the existing C/OpenSSL build dependencies:

```sh
make
make test-linux-info
python3 tools/test-linux-info-network.py build/stn-chain
```

The C target compiles the complete Linux runtime into its test translation
unit and uses real pthread mutexes. The network test starts separate fixture
nodes with temporary Chain data, temporary configuration disabling mining and
ephemeral ports. It withholds a real outbound STNP reply while issuing INFO
requests, including concurrent clients, fragmented frames and repeated requests
on one connection. INFO must complete within its one-second socket deadline.
It also tests malformed INFO and the existing `--once` path. It does not install
or restart the production service or touch production Chain data; the existing
runtime logger may append fixture events if its system log path is writable.

Changes are uncommitted and have not been deployed.
