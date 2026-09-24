# STN Chain

STN Chain is a blockchain being designed to distribute intelligence and
other data, record authorized decisions, and manage contracts throughout
the STN ecosystem and its participating network.

Threat intelligence is one application of the chain. The broader mission
connects observations to decisions, policies, obligations, and recorded
outcomes across local and remote systems.

## Project status

The current ISO C17 Chain implements canonical records and blocks, identity and
authority validation, PoW and cumulative work, fork/reorganization handling,
STNS persistence, P2P synchronization, STNC RPC, pending submissions and mining
work. Windows and Linux runtime implementations are present.

As reported by Operations on 2026-09-21, Phase 16 (Storage Records) is complete
and Phase 17 (Additional Platform Qualification) is complete for the demonstrated
operational scope. Third-party applications work with the Chain API, with
authoritative Chain data publicly displayed on the stn-chain.org explorer.
Linux Stratum serves as the miner endpoint, and CPU miners connect and mine.
Empty-block mining and work delivery are reported resolved. GPU, USB-ASIC and
ASIC platforms remain untested external follow-up work; participation from those
operators is not a Phase 17 completion gate. These observations do not constitute
qualification of all hardware or final production readiness. See the
[current roadmap](docs/ROADMAP.md) for scope and historical test evidence.

Phase 18 is SHELVED / CHAIN SCOPE QUALIFIED at its bounded Chain-side
Contract boundary. Phase 19 (Economics / Issuance / Rewards) is COMPLETE:
accepted Chain state now owns the bounded economic path through qualifying-share
evidence, miner compensation, canonical issuance, integer balances and total
supply, no-fee wallet transfers, persistence primitives, P2P reconstruction and
reorganization reconstruction.

Phase 20 (Production Qualification) is ACTIVE. Qualification is proceeding in
small, bounded increments across Windows x64 and Linux x64. The current Windows
baseline includes a passing production build, `test-chain` at 1,243 checks with
0 failures, repaired Contract snapshot qualification at 337 checks with
0 failures, and qualified peer-test execution at 2,867 checks with 0 failures
for the corrected local harness. Linux x64 production build, installation and
service restart have also completed successfully during Phase 20. Same-commit
Linux qualification-test evidence and the remaining Windows runner qualification
are still pending, so this is not a final production-readiness claim.

See [Economy](docs/ECONOMY.md), [Contracts](docs/CONTRACTS.md), the
[current roadmap](docs/ROADMAP.md), and the
[typed address foundation](docs/ADDRESSES.md).
The old Go prototype is not a compatibility requirement.

## Build

For unattended Linux startup and boot/reboot service installation, see
[Linux startup](platforms/linux/README.md). Saved history resumes; missing
history initializes the existing built-in genesis.

Run `build.cmd` from an ordinary Windows Command Prompt. It discovers the
installed MSVC x64 toolchain and builds `build\stn-chain.exe`. Microsoft C++
Build Tools and the Windows SDK are required; the Visual Studio IDE is not.
Headers are in includes/ and C sources in src/.
See [build instructions](docs/BUILD.md).
Planned OS/architecture boundaries are listed in [platforms](platforms/README.md).

## Windows startup

Run `build\stn-chain.exe` without arguments to start the node on
`127.0.0.1:18473`. Like Linux, Windows resumes existing history or creates
history with the existing built-in genesis when absent. The default data file
is `stn-chain-dev.stns` in the current working directory; use `--data PATH`
to keep a fixed location across launches. The filename does not enable `--dev`.
An existing history supplies its genesis anchor and undergoes normal full
validation. Invalid or unreadable history fails without replacement.
`--genesis BLOCK` still selects an explicit anchor; `--dev` remains optional.
This changes application startup, not Windows service/boot registration.

## Run for local stratumd integration

After running `build.cmd`, run `build\stn-chain.exe --dev` for the local
development fixture. The node
listens on 127.0.0.1:18473 using binary STNC RPC v2. This explicit development
fixture supplies mining work and accepts valid solutions to multiple concurrent
loopback clients; it is not Stratum or Bitcoin JSON-RPC. See [running and mining work](docs/MINING_WORK.md) for
configuration, client protocol, persistence, and limits.

The runtime/history increment verifies real mining and restart beyond height 64,
concurrent long-lived RPC sessions, and paged P2P/recovery through 130 blocks.
Whole-snapshot STNS storage remains. Pending admission and empty-block mining
are implemented; streaming persistence is not claimed. The --dev launcher remains
a development fixture, separate from normal deployed operation.

## Direction

- Implement the portable core primarily in ISO C, with explicit platform
  interfaces for Windows, Linux, macOS, and purpose-built STN systems.
- Support independently verifiable records and replicated chain state.
- Connect Sentinel-MVC intelligence through the STN-Labz API to chain
  participants and future Sentinel consumers, including Sentinel_Daemon.
- Support authorized company decisions, policies, doctrine, agreements,
  and publication workflows, including DevBot integrations.
- Separate network consensus from authority to issue company records.
- Design for broad participation while retaining proprietary ownership.
- Account for possible future Proof-of-Work mining, miner compensation,
  and a native coin. Activation and economic parameters remain undecided.

Bitcoin and Ethereum describe the ambition for distributed consensus and
programmable contracts; protocol, virtual-machine, wallet, and network
compatibility with either is not currently a requirement.

## Design documents

- [Mission and scope](docs/MISSION.md)
- [Component architecture](docs/ARCHITECTURE.md)
- [Decision register](docs/DECISIONS.md)
- [Delivery milestones](docs/ROADMAP.md)
- [Toolchain and dependency proposal](docs/TOOLCHAIN_PROPOSAL.md)
- [Canonical record encoding proposal](docs/ENCODING_PROPOSAL.md)
- [Signed intelligence record proposal](docs/SIGNED_RECORD_PROPOSAL.md)
- [Implemented validation context and limits](docs/VALIDATION_CONTEXT.md)
- [Transaction and block development format](docs/TRANSACTION_BLOCK_FORMAT.md)
- [Local chain state and validation boundaries](docs/CHAIN_STATE.md)
- [SHA-256, PoW profiles, and chain work](docs/POW.md)
- [Fork choice and reorganization planning](docs/FORK_CHOICE.md)
- [Persistence and atomic application](docs/PERSISTENCE.md)
- [Peer protocol, synchronization and recovery](docs/PEER_PROTOCOL.md)
- [Platform isolation and invariance](docs/PORTABILITY.md)
- [RPC application/miner interface](docs/RPC.md)
- [CONTRACTS](docs/CONTRACTS.md)
- [Changelog](docs/CHANGELOG.md)

These documents distinguish established direction from proposed engineering
boundaries and unresolved protocol choices. They are not a wire protocol
specification or a claim of implemented capabilities.

## License

STN Chain is proprietary software owned by STN-Labz. You may run the
unmodified software, compile unmodified source, and redistribute unchanged
releases under the [STN Chain Proprietary Participation License](docs/LICENSE.md).
Modifications require prior written permission from STN-Labz.
This is not an open-source license.
