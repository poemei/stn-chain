# Component Architecture

Status: proposed engineering boundaries, not an implemented package layout.
See [DECISIONS.md](DECISIONS.md) for unresolved protocol choices.

## Components and responsibilities

| Component | Responsibility | Boundary |
| --- | --- | --- |
| Protocol core | Canonical decoding, record/block validation, authorization, deterministic state transitions, consensus checks | No network calls, filesystem access, device access, or ambient wall-clock reads during deterministic validation |
| Node | Coordinate peers, pending records, chain selection, synchronization, and accepted state | Treat local submissions and peer messages as untrusted; use the same consensus rules |
| Storage adapter | Durable blocks and state, indexes, recovery, reversible state changes | Storage representation is separate from consensus encoding |
| Network adapter | Framing, connections, peer exchange, bounded requests, block/record propagation | Transport success does not establish validity or authority |
| Submission/query interface | Receive records, expose chain state and inclusion status | Administrative operations require separate access controls |
| Integration adapters | Connect STN-Labz API, Sentinel, DevBot, and company tools | External services cannot be required to resolve consensus validity |
| Contract engine | Validate authorized contract transitions against chain state | Execution model and resource accounting remain open |
| Miner, if activated | Obtain work, search, submit results, discard stale work | Node independently validates all submitted work |
| Platform layer | Files, sockets, time, entropy, concurrency, device access | OS-specific implementation remains outside deterministic core |

These are logical responsibilities. Exact libraries, executables, public C
interfaces, and directory layout will follow the protocol decisions.

## Record processing

1. An application prepares a record and signs the defined canonical bytes.
2. A node performs bounded decoding, identity/authorization checks, and
   pending-pool admission checks.
3. A candidate block selects ordered records against a specific parent.
4. Consensus validation rechecks all included records against the applicable
   state. Pending-pool admission is not a substitute for block validation.
5. Acceptance durably associates the block with its state transition.
6. Peers receive announcements and fetch missing data; each verifies it.
7. Consumers receive inclusion and chain-position information and apply their
   chosen confirmation policy before external action.

Consensus validity and local admission policy must be distinguished. Nodes
may impose local queue policies without changing which blocks are valid.

## Determinism and portability

Consensus must specify integer widths, byte order, lengths, ordering,
encoding, domain-separated signing/hash inputs, limits, and overflow rules.
Do not serialize C structs directly or use platform-dependent native types
as a wire specification. Economic quantities, if introduced, require exact
integer units rather than floating-point arithmetic.

Validation depends on explicit input bytes, prior state, and protocol rules.
Any time rules must have specified validation context. Contract execution
cannot read local files, call arbitrary APIs, or consult local randomness.

Cryptographic implementations should sit behind narrow interfaces with
published test vectors. Algorithm and library choices remain open; a small
dependency count does not justify inventing cryptographic primitives.

Memory ownership, bounded allocation, error propagation, and thread/state
ownership must be documented before implementation interfaces are frozen.

## Chain state and recovery

The development implementation now has an explicit in-memory validation state
and bounded atomic sequence validator; see [CHAIN_STATE.md](CHAIN_STATE.md).
Bounded Windows snapshot storage/application now exists; see
[PERSISTENCE.md](PERSISTENCE.md). Scalable storage/recovery remain future work.

Genesis must be identical for participants in the same network. Loading a
file successfully is not evidence that the chain is valid. Define verified
startup state, recovery after interrupted writes, and how indexes rebuild.

Chain changes must have an ordered owner or equivalent synchronization.
Pending records should not be irretrievably removed before acceptance is
durable. Fork handling needs reversible state transitions and a defined
policy for reconsidering displaced records.

Consumers must distinguish provisional inclusion from their action threshold.
Reorganization handling for acknowledgments and external actions must be
specified; the chain cannot undo a physical action or retract a delivered
message simply by switching branches.

## Contract authority

The proposed first contract exercise is policy issuance, activation,
acknowledgment, and supersession. This is a demonstration scope, not a
commitment to exclude general-purpose contracts.

Authorization needs chain-verifiable issuer identities, roles, delegation,
rotation, and revocation rules. A company service may prepare a decision;
validators must establish its validity using available protocol data.

## Mining boundary

The version-3 development profile now verifies work and accumulates integer
chain work, using real Windows SHA-256; see [POW.md](POW.md). The miner and
hardware backends described below remain unimplemented. Legacy version 1
is explicitly non-PoW and cannot be accepted by the PoW chain profile.

If PoW is selected for activation, define a work interface that binds work
to a parent, candidate contents, target, and job identity. Define stale-work
cancellation and independently checked submission results.

Start with a CPU reference backend. GPU acceleration and ASIC support depend
on the selected algorithm and compatible devices; generic ASIC compatibility
is not promised. Hardware backends may operate concurrently where supported,
without changing consensus requirements.

## Content distribution

Separate signed metadata and commitments from payload placement. Public
inline content, encrypted content, and externally stored documents have
different availability and privacy properties. The eventual protocol must
specify which bytes validators require and how recipients obtain payloads.

## Operational sovereignty

Maintain the ability to build from STN-Labz-controlled source, operate nodes,
recover state, and retain engineering knowledge independently of a single
hosting, build, identity, or AI provider. Hosted development services may
assist but must not become an unnecessary runtime dependency.

For each significant dependency, record its requirement, licensing and
security fit, interface boundary, loss impact, and practical replacement or
recovery path. Review depth should match impact. Locally maintainable mature
cryptography is compatible with this objective; weaker bespoke cryptography
is not a sovereignty improvement. See the policy references in DECISIONS.md.

## Deployment boundary

The first implementation runs on an explicitly isolated development network.
A local block-production harness, if used before consensus is implemented,
must be labeled as such and must not be represented as a secure blockchain.
Public operation requires the consensus, authorization, recovery, and
resource-limit milestones in [ROADMAP.md](ROADMAP.md).

## Local fork evaluation

The core now revalidates two bounded complete PoW histories, compares calculated
work, discovers their shared prefix and publishes an atomic in-memory plan.
Equal work retains current. No active state is changed. See [FORK_CHOICE.md](FORK_CHOICE.md)
for fixed-target limitations, eligibility, detach/attach ranges and future replay coordination.

## Persistence coordination

The portable storage layer treats disk as untrusted canonical input and calls
existing validators/fork choice. It reloads active data, rejects stale plans,
and commits caller state only after provider replacement. Windows filesystem
operations remain in platforms/windows. No transaction, replay or mempool state
is implicitly changed; those future subsystems require coordinated application.

## Peer evidence and explicit recovery

Socket transport, framing/handshake, sync coordination, consensus and storage
remain separate. Bounded header hints avoid retransmitting validated prefixes;
full blocks and calculated work alone determine preference. Explicit recovery
can rebuild damaged snapshots without changing strict startup rejection.
See [PEER_PROTOCOL.md](PEER_PROTOCOL.md). No peer or peer majority is authoritative;
P2P is separate from future application RPC.

## Enforced platform boundary

Windows is a supported platform backend, not the STN-Chain architecture.

Consensus-relevant behavior must remain identical across supported operating
systems and processor architectures.

The current layout and compile-time selection gates are documented in
[PORTABILITY.md](PORTABILITY.md). OS-specific declarations and services live
under platforms/; portable core modules share all consensus/wire behavior.
