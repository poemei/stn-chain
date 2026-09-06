# Mission and Scope

Status: initial design baseline. No protocol implementation exists yet.

## Mission

STN Chain will provide a shared, independently verifiable history of
intelligence, authorized decisions, contracts, and their recorded outcomes.
Participating systems should be able to obtain and verify accepted chain
state without depending on one website or API as its sole source.

The original threat network is a use case within this broader mission.
The historical Go implementation is not a protocol or compatibility baseline.

## Intelligence distribution

The intended integration path is:

    Sentinel-MVC -> STN-Labz API -> chain submission adapter
        -> STN node -> participating nodes -> Sentinel consumers
                                           -> Sentinel_Daemon

The API gathers and serves intelligence; an adapter submits signed records
to a node. Nodes validate, store, and propagate records through blocks.
Consumers synchronize and apply records according to their own role.
Adapter ownership and push-versus-pull behavior remain to be specified.

A valid signature establishes who made a report, not that its claims are
true. Evidence, assessment, correction, and supersession must be represented
without pretending that consensus independently proves an observation.

## Decisions and contracts

Company decisions should identify their issuer, authority, affected parties,
scope, effective conditions, and relationship to earlier records.

Policies and doctrine require identifiable versions and authorized lifecycle
changes. Agreements require parties, obligations, conditions, and permitted
state transitions. DevBot publication workflows may use the same foundation
for documents, reports, papers, and publication approvals.

Contract execution on the chain establishes deterministic state. External
systems perform real-world actions and submit attributable acknowledgments
or outcome records. Inclusion, receipt, acknowledgment, and completion are
separate events. Offline parties must be able to catch up and determine
which obligations still apply.

## Participation and authority

Broad public participation is an objective. Exact network admission and
record-submission rules remain open.

Consensus participation does not grant company authority. Producing a block
must not authorize its miner to issue policy, approve an agreement, or sign
for another party. Those permissions derive from separately validated
identities and authorization rules.

The software license permits unchanged participation while restricting
modification. It is not a security mechanism: nodes must validate messages
as untrusted input regardless of the sender's claimed software release.

## Possible economic layer

Future scope may include Proof of Work, a native coin, miner rewards,
transfers, and fees. The architecture must account for that possibility;
it does not establish a launch date, issuance schedule, algorithm, ticker,
or entitlement to rewards. Market listing is a separate external effort.

## Data boundaries

Not every company document belongs in publicly replicated plaintext.
Payload placement, access control, retention, and availability need explicit
design. A digest verifies retrieved content but does not deliver it or
make it available. Encryption does not erase metadata or remove replicated
ciphertext when permissions change.

## Initial success criteria

- A signed intelligence record reaches a second independently running node
  and a consumer through a documented end-to-end path.
- Nodes agree on validation results for the same bytes and prior state.
- Restart and synchronization preserve verifiable state.
- An authorized policy lifecycle can be represented and independently
  checked, including acknowledgment and supersession.
- Invalid signatures, unauthorized transitions, duplicates, malformed input,
  and incompatible chain state are rejected predictably.

These criteria become implementation milestones in [ROADMAP.md](ROADMAP.md).
