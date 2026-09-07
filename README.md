# STN Chain

STN Chain is a blockchain being designed to distribute intelligence and
other data, record authorized decisions, and manage contracts throughout
the STN ecosystem and its participating network.

Threat intelligence is one application of the chain. The broader mission
connects observations to decisions, policies, obligations, and recorded
outcomes across local and remote systems.

## Project status

The historical Go prototype has been removed. This repository currently
contains design documentation, an ISO C17 Visual Studio scaffold, and a tested
structural record encoder/decoder, intelligence payload codec, and explicit
validation context, deterministic transaction/block containers, and local
chain-state/link validation, Windows SHA-256, and development PoW verification
with cumulative chain-work accounting, validated fork/reorganization planning,
bounded Windows persistence with atomic chain application, and a bounded P2P
exchange with validated synchronization and explicit recovery;
there is no implemented node, miner, contract runtime, or coin.
The redesign has no compatibility requirement
with the old Go implementation.

## Build

Open [stn-chain.sln](stn-chain.sln) in Visual Studio 2026 and build
Release | x64. Headers are in includes/ and C sources in src/.
See [Visual Studio build instructions](docs/BUILD.md).
Planned OS/architecture boundaries are listed in [platforms](platforms/README.md).

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
