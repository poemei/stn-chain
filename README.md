# stn-chain

The STN Blockchain

## Purpose

STN Chain is a distributed blockchain intended to provide trusted data
distribution and smart-contract capabilities across the STN ecosystem.

Its purposes include:

- Retrieve threat intelligence produced by Sentinel installations across
  participating websites and distribute that intelligence to other
  participating Sentinel installations through the blockchain.

- Provide a distributed mechanism for publishing and preserving threat
  intelligence concerning malicious or suspicious actors observed across
  participating systems.

- Assist DevBot with publication workflows involving blogs, posts, papers,
  reports, and contracts.

- Provide smart-contract capabilities for moving contracts and other
  authorized structured agreements throughout the company ecosystem.

- Provide a shared, independently verifiable chain of records that can be
  consumed by STN systems without requiring a single website or service to
  act as the sole source of chain state.

## Engineering Direction

### Portability

STN Chain will be developed primarily in ISO C.

The blockchain protocol and core implementation will remain
system-agnostic wherever practical.

Platform-specific functionality will be isolated behind clearly defined
interfaces so implementations can support:

- Windows
- Linux
- macOS
- purpose-built STN systems

### Mining

STN Chain will support Proof-of-Work mining using available computing
hardware.

The miner will be designed to detect and use supported resources available
on the host system, including:

- ASIC hardware
- GPU hardware
- CPU resources

Mining capability will not depend upon a single operating system or
hardware class.

Where practical, available mining resources may operate concurrently rather
than requiring the miner to select only one hardware class.

The miner's primary responsibility is to obtain STN Chain work, perform the
required hashing using available hardware, submit valid results, and
continue mining.

### Miner Architecture

The STN miner will use a portable ISO C core with platform and hardware
backends where operating-system or device-specific functionality is
required.

Conceptually:

    STN Miner
        |
        +-- Portable Mining Core
        |
        +-- Platform Layer
        |     +-- Windows
        |     +-- Linux
        |     +-- macOS
        |
        +-- Mining Backends
              +-- ASIC
              +-- GPU
              +-- CPU

Hardware detection, device communication, and acceleration mechanisms are
implementation details and will not alter the STN Chain consensus rules.

All supported miners will perform work against the same consensus-defined
Proof-of-Work requirements.

### Blockchain Core

The blockchain core will be responsible for:

- Block construction and validation
- Chain validation
- Transaction processing
- Threat-intelligence records
- Smart contracts
- Peer-to-peer communication
- Consensus
- Proof-of-Work validation
- Persistent chain state
- Chain synchronization

Mining implementation will remain separate from consensus validation.

A node does not need to understand how another participant produced valid
Proof-of-Work. It only needs to deterministically establish whether that
work satisfies the STN Chain consensus rules.

## Design Goals

STN Chain is being redesigned around the following goals:

- ISO C portability
- Operating-system independence
- CPU mining
- GPU mining
- ASIC mining
- Deterministic validation
- Distributed threat-intelligence propagation
- Smart-contract support
- Peer-to-peer chain synchronization
- Hardware-independent consensus
- Minimal unnecessary external dependencies

## Open Design Work

The following areas require engineering definition before implementation:

- Block format
- Transaction format
- Threat record format
- Smart-contract model
- Proof-of-Work algorithm
- Mining work format
- Difficulty and difficulty adjustment
- Target block interval
- Chain selection rules
- Genesis block
- Peer discovery
- Peer synchronization
- Fork handling
- Transaction and threat pools
- Cryptographic identity and signatures
- Contract authorization and validation
- Network protocol
- Persistent storage format
- Miner/backend interface
- ASIC device support
- GPU acceleration interfaces
- CPU mining implementation

## License

STN Chain is proprietary software owned by STN-Labz. You may run the
unmodified software, compile unmodified source, and redistribute unchanged
releases under the [STN Chain Proprietary Participation License](docs/LICENSE.md).
Modifications require prior written permission from STN-Labz.
This is not an open-source license.

## Documentation
- [LICENSE](docs/LICENSE.md)
