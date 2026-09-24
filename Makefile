# Copyright (c) 2026 STN-Labz. See docs/LICENSE.md.
#
# STN Chain Linux build
# Small. Deterministic. Easy to Use.

CC ?= cc

TARGET := stn-chain
BUILD_DIR := build
TARGET_PATH := $(BUILD_DIR)/$(TARGET)

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
DATADIR ?= /var/lib/stn-chain
LOGDIR ?= /var/log/stn-chain

CFLAGS ?= -O2
CFLAGS += -std=c17
CFLAGS += -Wall -Wextra -Wpedantic
CFLAGS += -Iincludes
CFLAGS += -Isrc
CFLAGS += -Isrc/crypto/ed25519_donna
CFLAGS += -Iplatforms/linux
CFLAGS += -pthread

# Linux/POSIX feature exposure is restricted to the Linux platform backend.
PLATFORM_CFLAGS := -D_GNU_SOURCE

LDLIBS += -lcrypto
LDLIBS += -pthread

UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

ifeq ($(UNAME_S),Linux)
    HOST_OS := linux
else
    $(error Unsupported host OS: $(UNAME_S))
endif

ifeq ($(UNAME_M),x86_64)
    HOST_ARCH := x64
else
    $(error Unsupported Linux architecture: $(UNAME_M))
endif

CORE_SOURCES := \
	src/main.c \
	src/stn_address.c \
	src/stn_authority.c \
	src/stn_block.c \
	src/stn_chain.c \
	src/stn_economy.c \
	src/stn_compensation.c \
	src/stn_issuance.c \
	src/stn_economic_state.c \
	src/stn_compensation_state.c \
	src/stn_issuance_binding.c \
	src/stn_wallet.c \
	src/stn_transfer.c \
	src/stn_transfer_replay.c \
	src/stn_transfer_binding.c \
	src/stn_transfer_authorization.c \
	src/stn_transfer_acceptance.c \
	src/stn_transfer_envelope.c \
	src/stn_transfer_envelope_replay.c \
	src/stn_transfer_envelope_authorization.c \
	src/stn_transfer_envelope_acceptance.c \
	src/stn_fork.c \
	src/stn_identity.c \
	src/stn_sentinel_intelligence.c \
	src/stn_lifecycle.c \
	src/stn_mining.c \
	src/stn_node_service.c \
	src/stn_peer.c \
	src/stn_pending.c \
	src/stn_pow.c \
	src/stn_record.c \
	src/stn_replay.c \
	src/stn_rpc.c \
	src/stn_share.c \
	src/stn_share_replay.c \
	src/stn_storage.c \
	src/stn_transaction.c \
	src/stn_validation.c \
	src/stn_contract.c \
	src/stn_contract_consensus.c \
	src/stn_contract_lineage.c \
	src/stn_contract_state.c \
	src/stn_contract_snapshot.c \
	src/stn_contract_transaction.c

CRYPTO_SOURCES := \
	src/crypto/ed25519_donna/ed25519_provider.c

PLATFORM_SOURCES := \
	platforms/linux/stn_sha256.c \
	platforms/linux/stn_linux_storage.c \
	platforms/linux/stn_linux_peer.c \
	platforms/linux/stn_app_linux.c

SOURCES := \
	$(CORE_SOURCES) \
	$(CRYPTO_SOURCES) \
	$(PLATFORM_SOURCES)

OBJECTS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SOURCES))

.PHONY: all configure clean install install-service uninstall info

all: configure $(TARGET_PATH)

configure:
	@echo "Configuring STN Chain for $(HOST_OS)/$(HOST_ARCH)..."
	@echo "Compiler: $(CC)"
	@echo "Build directory: $(BUILD_DIR)"

info:
	@echo "STN Chain build configuration"
	@echo "OS:           $(HOST_OS)"
	@echo "Architecture: $(HOST_ARCH)"
	@echo "Compiler:     $(CC)"
	@echo "Target:       $(TARGET_PATH)"
	@echo "Install:      $(BINDIR)/$(TARGET)"
	@echo "Data:         $(DATADIR)"
	@echo "Logs:         $(LOGDIR)"

$(TARGET_PATH): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@ $(LDLIBS)
	@echo
	@echo "Built: $(TARGET_PATH)"

$(BUILD_DIR)/platforms/linux/%.o: platforms/linux/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(PLATFORM_CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

install: $(TARGET_PATH)
	install -d $(DESTDIR)$(BINDIR)
	install -m 0755 $(TARGET_PATH) $(DESTDIR)$(BINDIR)/$(TARGET)
	install -d $(DESTDIR)$(DATADIR)
	install -d $(DESTDIR)$(LOGDIR)
	@echo
	@echo "Installed:"
	@echo "  Application: $(BINDIR)/$(TARGET)"
	@echo "  Data:        $(DATADIR)"
	@echo "  Logs:        $(LOGDIR)"

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
	@echo "Removed $(BINDIR)/$(TARGET)"
	@echo "Data and logs preserved:"
	@echo "  $(DATADIR)"

# Enable/start is deliberately separate from staging files under DESTDIR.
install-service: install
	install -d $(DESTDIR)/etc/systemd/system
	sed 's|@BINDIR@|$(BINDIR)|g' platforms/linux/stn-chain.service.in > $(BUILD_DIR)/stn-chain.service
	install -m 0644 $(BUILD_DIR)/stn-chain.service $(DESTDIR)/etc/systemd/system/stn-chain.service
	@echo "Service installed. Run: systemctl daemon-reload && systemctl enable --now stn-chain"

clean:
	rm -rf $(BUILD_DIR)
	@echo "Build files removed."

# Same fixed address vectors as the Windows test suite.
.PHONY: test-address
test-address: $(BUILD_DIR)/test-address
	$(BUILD_DIR)/test-address

$(BUILD_DIR)/test-address: tests/test_address.c src/stn_address.c platforms/linux/stn_sha256.c includes/stn_address.h includes/stn_sha256.h includes/stn_transaction.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -DSTN_ADDRESS_TEST_MAIN tests/test_address.c src/stn_address.c platforms/linux/stn_sha256.c -o $@ $(LDLIBS)

# Canonical Contract v1 qualification vectors and Phase 14 authority bridge.
.PHONY: test-contract
test-contract: $(BUILD_DIR)/test-contract
	$(BUILD_DIR)/test-contract

$(BUILD_DIR)/test-contract: tests/test_contract.c src/stn_contract.c src/stn_address.c src/stn_authority.c src/stn_identity.c src/crypto/ed25519_donna/ed25519_provider.c platforms/linux/stn_sha256.c includes/stn_contract.h includes/stn_address.h includes/stn_authority.h includes/stn_identity.h includes/stn_sha256.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -DSTN_CONTRACT_TEST_MAIN tests/test_contract.c src/stn_contract.c src/stn_address.c src/stn_authority.c src/stn_identity.c src/crypto/ed25519_donna/ed25519_provider.c platforms/linux/stn_sha256.c -o $@ $(LDLIBS)


# Contract majority consensus qualification.
.PHONY: test-contract-consensus
test-contract-consensus: $(BUILD_DIR)/test-contract-consensus
	$(BUILD_DIR)/test-contract-consensus

$(BUILD_DIR)/test-contract-consensus: tests/test_contract_consensus.c src/stn_contract_consensus.c src/stn_contract.c src/stn_address.c src/stn_authority.c src/stn_identity.c src/crypto/ed25519_donna/ed25519_provider.c platforms/linux/stn_sha256.c includes/stn_contract_consensus.h includes/stn_contract.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -DSTN_CONTRACT_CONSENSUS_TEST_MAIN tests/test_contract_consensus.c src/stn_contract_consensus.c src/stn_contract.c src/stn_address.c src/stn_authority.c src/stn_identity.c src/crypto/ed25519_donna/ed25519_provider.c platforms/linux/stn_sha256.c -o $@ $(LDLIBS)


# Stable Contract DRAFT-origin lineage qualification.
.PHONY: test-contract-lineage
test-contract-lineage: $(BUILD_DIR)/test-contract-lineage
	$(BUILD_DIR)/test-contract-lineage

$(BUILD_DIR)/test-contract-lineage: tests/test_contract_lineage.c src/stn_contract_lineage.c src/stn_contract.c src/stn_address.c src/stn_authority.c src/stn_identity.c src/crypto/ed25519_donna/ed25519_provider.c platforms/linux/stn_sha256.c includes/stn_contract_lineage.h includes/stn_contract.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -DSTN_CONTRACT_LINEAGE_TEST_MAIN tests/test_contract_lineage.c src/stn_contract_lineage.c src/stn_contract.c src/stn_address.c src/stn_authority.c src/stn_identity.c src/crypto/ed25519_donna/ed25519_provider.c platforms/linux/stn_sha256.c -o $@ $(LDLIBS)


# Bounded Contract accepted-state qualification.
.PHONY: test-contract-state
test-contract-state: $(BUILD_DIR)/test-contract-state
	$(BUILD_DIR)/test-contract-state

$(BUILD_DIR)/test-contract-state: tests/test_contract_state.c src/stn_contract_state.c src/stn_contract_consensus.c src/stn_contract_lineage.c src/stn_contract.c src/stn_address.c src/stn_authority.c src/stn_identity.c src/crypto/ed25519_donna/ed25519_provider.c platforms/linux/stn_sha256.c includes/stn_contract_state.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -DSTN_CONTRACT_STATE_TEST_MAIN tests/test_contract_state.c src/stn_contract_state.c src/stn_contract_consensus.c src/stn_contract_lineage.c src/stn_contract.c src/stn_address.c src/stn_authority.c src/stn_identity.c src/crypto/ed25519_donna/ed25519_provider.c platforms/linux/stn_sha256.c -o $@ $(LDLIBS)


# Owned Contract snapshot qualification.
.PHONY: test-contract-snapshot
test-contract-snapshot: $(BUILD_DIR)/test-contract-snapshot
	$(BUILD_DIR)/test-contract-snapshot

$(BUILD_DIR)/test-contract-snapshot: tests/test_contract_snapshot.c src/stn_chain.c src/stn_compensation_state.c src/stn_block.c src/stn_transaction.c src/stn_record.c src/stn_validation.c src/stn_sentinel_intelligence.c src/stn_pow.c src/stn_lifecycle.c src/stn_replay.c src/stn_contract_transaction.c src/stn_contract_snapshot.c src/stn_contract_state.c src/stn_contract_consensus.c src/stn_contract_lineage.c src/stn_contract.c src/stn_address.c src/stn_authority.c src/stn_identity.c src/crypto/ed25519_donna/ed25519_provider.c platforms/linux/stn_sha256.c includes/stn_contract_snapshot.h src/stn_share.c src/stn_share_replay.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -DSTN_CONTRACT_SNAPSHOT_TEST_MAIN tests/test_contract_snapshot.c src/stn_share.c src/stn_share_replay.c src/stn_chain.c src/stn_compensation_state.c src/stn_block.c src/stn_transaction.c src/stn_record.c src/stn_validation.c src/stn_sentinel_intelligence.c src/stn_pow.c src/stn_lifecycle.c src/stn_replay.c src/stn_contract_transaction.c src/stn_contract_snapshot.c src/stn_contract_state.c src/stn_contract_consensus.c src/stn_contract_lineage.c src/stn_contract.c src/stn_address.c src/stn_authority.c src/stn_identity.c src/crypto/ed25519_donna/ed25519_provider.c platforms/linux/stn_sha256.c -o $@ $(LDLIBS)


# Phase 19 deterministic economy primitive qualification.
.PHONY: test-economy
test-economy: $(BUILD_DIR)/test-economy
	$(BUILD_DIR)/test-economy

$(BUILD_DIR)/test-economy: tests/test_economy.c src/stn_economy.c src/stn_pow.c includes/stn_economy.h includes/stn_pow.h src/stn_share.c src/stn_share_replay.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -DSTN_ECONOMY_TEST_MAIN tests/test_economy.c src/stn_share.c src/stn_share_replay.c src/stn_economy.c src/stn_pow.c src/stn_block.c src/stn_chain.c src/stn_compensation_state.c src/stn_transaction.c src/stn_record.c src/stn_validation.c src/stn_sentinel_intelligence.c src/stn_lifecycle.c src/stn_replay.c src/stn_contract_transaction.c src/stn_contract_snapshot.c src/stn_contract_state.c src/stn_contract_consensus.c src/stn_contract_lineage.c src/stn_contract.c src/stn_address.c src/stn_authority.c src/stn_identity.c src/crypto/ed25519_donna/ed25519_provider.c platforms/linux/stn_sha256.c -o $@ $(LDLIBS)


# Phase 19 canonical qualifying-share evidence qualification.
.PHONY: test-share
test-share: $(BUILD_DIR)/test-share
	$(BUILD_DIR)/test-share

$(BUILD_DIR)/test-share: tests/test_share.c src/stn_share.c src/stn_economy.c src/stn_pow.c src/stn_block.c src/stn_chain.c src/stn_compensation_state.c src/stn_transaction.c src/stn_record.c src/stn_validation.c src/stn_sentinel_intelligence.c src/stn_lifecycle.c src/stn_replay.c src/stn_contract_transaction.c src/stn_contract_snapshot.c src/stn_contract_state.c src/stn_contract_consensus.c src/stn_contract_lineage.c src/stn_contract.c src/stn_address.c src/stn_authority.c src/stn_identity.c src/crypto/ed25519_donna/ed25519_provider.c platforms/linux/stn_sha256.c includes/stn_share.h src/stn_share_replay.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -DSTN_SHARE_TEST_MAIN tests/test_share.c src/stn_share_replay.c src/stn_share.c src/stn_economy.c src/stn_pow.c src/stn_block.c src/stn_chain.c src/stn_compensation_state.c src/stn_transaction.c src/stn_record.c src/stn_validation.c src/stn_sentinel_intelligence.c src/stn_lifecycle.c src/stn_replay.c src/stn_contract_transaction.c src/stn_contract_snapshot.c src/stn_contract_state.c src/stn_contract_consensus.c src/stn_contract_lineage.c src/stn_contract.c src/stn_address.c src/stn_authority.c src/stn_identity.c src/crypto/ed25519_donna/ed25519_provider.c platforms/linux/stn_sha256.c -o $@ $(LDLIBS)


# Phase 19 deterministic accepted-share replay qualification.
.PHONY: test-share-replay
test-share-replay: $(BUILD_DIR)/test-share-replay
	$(BUILD_DIR)/test-share-replay

$(BUILD_DIR)/test-share-replay: tests/test_share_replay.c src/stn_share_replay.c includes/stn_share_replay.h includes/stn_share.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -DSTN_SHARE_REPLAY_TEST_MAIN tests/test_share_replay.c src/stn_share_replay.c -o $@ $(LDLIBS)


# Phase 19 canonical mining issuance record qualification.
.PHONY: test-issuance
test-issuance: $(BUILD_DIR)/test-issuance
	$(BUILD_DIR)/test-issuance

$(BUILD_DIR)/test-issuance: tests/test_issuance.c src/stn_issuance.c src/stn_compensation.c includes/stn_issuance.h includes/stn_compensation.h includes/stn_address.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -DSTN_ISSUANCE_TEST_MAIN tests/test_issuance.c src/stn_issuance.c src/stn_compensation.c -o $@ $(LDLIBS)


# Phase 19 deterministic accepted economic state qualification.
.PHONY: test-economic-state
test-economic-state: $(BUILD_DIR)/test-economic-state
	$(BUILD_DIR)/test-economic-state

$(BUILD_DIR)/test-economic-state: tests/test_economic_state.c src/stn_economic_state.c src/stn_transfer.c includes/stn_economic_state.h includes/stn_issuance.h includes/stn_transfer.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -DSTN_ECONOMIC_STATE_TEST_MAIN tests/test_economic_state.c src/stn_economic_state.c src/stn_transfer.c -o $@ $(LDLIBS)


# Phase 19 deterministic accepted compensation mapping state qualification.
.PHONY: test-compensation-state
test-compensation-state: $(BUILD_DIR)/test-compensation-state
	$(BUILD_DIR)/test-compensation-state

$(BUILD_DIR)/test-compensation-state: tests/test_compensation_state.c src/stn_compensation_state.c includes/stn_compensation_state.h includes/stn_compensation.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -DSTN_COMPENSATION_STATE_TEST_MAIN tests/test_compensation_state.c src/stn_compensation_state.c -o $@ $(LDLIBS)


.PHONY: test-issuance-binding
test-issuance-binding: $(BUILD_DIR)/test-issuance-binding
	$(BUILD_DIR)/test-issuance-binding

ISSUANCE_BINDING_SOURCES := $(filter-out src/main.c,$(CORE_SOURCES)) $(CRYPTO_SOURCES) platforms/linux/stn_sha256.c

$(BUILD_DIR)/test-issuance-binding: tests/test_issuance_binding.c $(ISSUANCE_BINDING_SOURCES)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -DSTN_ISSUANCE_BINDING_TEST_MAIN tests/test_issuance_binding.c $(ISSUANCE_BINDING_SOURCES) -o $@ $(LDLIBS)


# Phase 19 deterministic wallet primitive qualification.
.PHONY: test-wallet
test-wallet: $(BUILD_DIR)/test-wallet
	$(BUILD_DIR)/test-wallet

$(BUILD_DIR)/test-wallet: tests/test_wallet.c src/stn_wallet.c src/stn_address.c platforms/linux/stn_sha256.c includes/stn_wallet.h includes/stn_address.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -DSTN_WALLET_TEST_MAIN tests/test_wallet.c src/stn_wallet.c src/stn_address.c platforms/linux/stn_sha256.c -o $@ $(LDLIBS)


# Phase 19 deterministic transfer primitive qualification.
.PHONY: test-transfer
test-transfer: $(BUILD_DIR)/test-transfer
	$(BUILD_DIR)/test-transfer

$(BUILD_DIR)/test-transfer: tests/test_transfer.c src/stn_transfer.c includes/stn_transfer.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -DSTN_TRANSFER_TEST_MAIN tests/test_transfer.c src/stn_transfer.c -o $@ $(LDLIBS)


# Phase 19 deterministic accepted-transfer replay qualification.
.PHONY: test-transfer-replay
test-transfer-replay: $(BUILD_DIR)/test-transfer-replay
	$(BUILD_DIR)/test-transfer-replay

$(BUILD_DIR)/test-transfer-replay: tests/test_transfer_replay.c src/stn_transfer_replay.c src/stn_transfer.c includes/stn_transfer_replay.h includes/stn_transfer.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -DSTN_TRANSFER_REPLAY_TEST_MAIN tests/test_transfer_replay.c src/stn_transfer_replay.c src/stn_transfer.c -o $@ $(LDLIBS)


# Phase 19 accepted transfer replay/economic-state binding qualification.
.PHONY: test-transfer-binding
test-transfer-binding: $(BUILD_DIR)/test-transfer-binding
	$(BUILD_DIR)/test-transfer-binding

$(BUILD_DIR)/test-transfer-binding: tests/test_transfer_binding.c src/stn_transfer_binding.c src/stn_transfer_replay.c src/stn_transfer.c src/stn_economic_state.c includes/stn_transfer_binding.h includes/stn_transfer_replay.h includes/stn_transfer.h includes/stn_economic_state.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -DSTN_TRANSFER_BINDING_TEST_MAIN tests/test_transfer_binding.c src/stn_transfer_binding.c src/stn_transfer_replay.c src/stn_transfer.c src/stn_economic_state.c -o $@ $(LDLIBS)


# Phase 19 deterministic source-wallet transfer authorization qualification.
.PHONY: test-transfer-authorization
test-transfer-authorization: $(BUILD_DIR)/test-transfer-authorization
	$(BUILD_DIR)/test-transfer-authorization

$(BUILD_DIR)/test-transfer-authorization: tests/test_transfer_authorization.c src/stn_transfer_authorization.c src/stn_transfer.c src/stn_wallet.c src/stn_address.c src/stn_identity.c src/crypto/ed25519_donna/ed25519_provider.c platforms/linux/stn_sha256.c includes/stn_transfer_authorization.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -DSTN_TRANSFER_AUTHORIZATION_TEST_MAIN tests/test_transfer_authorization.c src/stn_transfer_authorization.c src/stn_transfer.c src/stn_wallet.c src/stn_address.c src/stn_identity.c src/crypto/ed25519_donna/ed25519_provider.c platforms/linux/stn_sha256.c -o $@ $(LDLIBS)


# Phase 19 authorized accepted-transfer qualification.
.PHONY: test-transfer-acceptance
test-transfer-acceptance: $(BUILD_DIR)/test-transfer-acceptance
	$(BUILD_DIR)/test-transfer-acceptance

TRANSFER_ACCEPTANCE_SOURCES := src/stn_transfer_acceptance.c src/stn_transfer_authorization.c src/stn_transfer_binding.c src/stn_transfer_replay.c src/stn_transfer.c src/stn_economic_state.c src/stn_wallet.c src/stn_address.c src/stn_identity.c src/crypto/ed25519_donna/ed25519_provider.c platforms/linux/stn_sha256.c

$(BUILD_DIR)/test-transfer-acceptance: tests/test_transfer_acceptance.c $(TRANSFER_ACCEPTANCE_SOURCES) includes/stn_transfer_acceptance.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -DSTN_TRANSFER_ACCEPTANCE_TEST_MAIN tests/test_transfer_acceptance.c $(TRANSFER_ACCEPTANCE_SOURCES) -o $@ $(LDLIBS)


# Phase 19 canonical transfer envelope qualification.
.PHONY: test-transfer-envelope
test-transfer-envelope: $(BUILD_DIR)/test-transfer-envelope
	$(BUILD_DIR)/test-transfer-envelope

$(BUILD_DIR)/test-transfer-envelope: tests/test_transfer_envelope.c src/stn_transfer_envelope.c src/stn_transfer.c includes/stn_transfer_envelope.h includes/stn_transfer.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -DSTN_TRANSFER_ENVELOPE_TEST_MAIN tests/test_transfer_envelope.c src/stn_transfer_envelope.c src/stn_transfer.c -o $@ $(LDLIBS)


# Phase 19 nonce-aware transfer replay qualification.
.PHONY: test-transfer-envelope-replay
test-transfer-envelope-replay: $(BUILD_DIR)/test-transfer-envelope-replay
	$(BUILD_DIR)/test-transfer-envelope-replay

$(BUILD_DIR)/test-transfer-envelope-replay: tests/test_transfer_envelope_replay.c src/stn_transfer_envelope_replay.c src/stn_replay.c src/stn_record.c src/stn_identity.c src/crypto/ed25519_donna/ed25519_provider.c includes/stn_transfer_envelope_replay.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -DSTN_TRANSFER_ENVELOPE_REPLAY_TEST_MAIN tests/test_transfer_envelope_replay.c src/stn_transfer_envelope_replay.c src/stn_replay.c src/stn_record.c src/stn_identity.c src/crypto/ed25519_donna/ed25519_provider.c -o $@ $(LDLIBS)


# Phase 19 nonce-bound transfer authorization qualification.
.PHONY: test-transfer-envelope-authorization
test-transfer-envelope-authorization: $(BUILD_DIR)/test-transfer-envelope-authorization
	$(BUILD_DIR)/test-transfer-envelope-authorization

$(BUILD_DIR)/test-transfer-envelope-authorization: tests/test_transfer_envelope_authorization.c src/stn_transfer_envelope_authorization.c src/stn_transfer.c src/stn_wallet.c src/stn_address.c src/stn_identity.c src/crypto/ed25519_donna/ed25519_provider.c platforms/linux/stn_sha256.c includes/stn_transfer_envelope_authorization.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -DSTN_TRANSFER_ENVELOPE_AUTHORIZATION_TEST_MAIN tests/test_transfer_envelope_authorization.c src/stn_transfer_envelope_authorization.c src/stn_transfer.c src/stn_wallet.c src/stn_address.c src/stn_identity.c src/crypto/ed25519_donna/ed25519_provider.c platforms/linux/stn_sha256.c -o $@ $(LDLIBS)


# Phase 19 nonce-aware authorized transfer acceptance qualification.
.PHONY: test-transfer-envelope-acceptance
test-transfer-envelope-acceptance: $(BUILD_DIR)/test-transfer-envelope-acceptance
	$(BUILD_DIR)/test-transfer-envelope-acceptance

TRANSFER_ENVELOPE_ACCEPTANCE_SOURCES := src/stn_transfer_envelope_acceptance.c src/stn_transfer_envelope_authorization.c src/stn_transfer_envelope_replay.c src/stn_transfer.c src/stn_economic_state.c src/stn_wallet.c src/stn_address.c src/stn_replay.c src/stn_record.c src/stn_identity.c src/crypto/ed25519_donna/ed25519_provider.c platforms/linux/stn_sha256.c

$(BUILD_DIR)/test-transfer-envelope-acceptance: tests/test_transfer_envelope_acceptance.c $(TRANSFER_ENVELOPE_ACCEPTANCE_SOURCES) includes/stn_transfer_envelope_acceptance.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -DSTN_TRANSFER_ENVELOPE_ACCEPTANCE_TEST_MAIN tests/test_transfer_envelope_acceptance.c $(TRANSFER_ENVELOPE_ACCEPTANCE_SOURCES) -o $@ $(LDLIBS)


# Phase 19 canonical transfer transaction qualification.
.PHONY: test-transfer-transaction
test-transfer-transaction: $(BUILD_DIR)/test-transfer-transaction
	$(BUILD_DIR)/test-transfer-transaction

TRANSFER_TRANSACTION_SOURCES := src/stn_transaction.c src/stn_transfer_envelope.c src/stn_transfer.c src/stn_record.c src/stn_contract_transaction.c src/stn_contract.c src/stn_contract_lineage.c src/stn_contract_state.c src/stn_contract_consensus.c src/stn_share.c src/stn_compensation.c src/stn_issuance.c src/stn_sentinel_intelligence.c src/stn_identity.c src/crypto/ed25519_donna/ed25519_provider.c platforms/linux/stn_sha256.c

$(BUILD_DIR)/test-transfer-transaction: tests/test_transfer_transaction.c $(TRANSFER_TRANSACTION_SOURCES) includes/stn_transaction.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -DSTN_TRANSFER_TRANSACTION_TEST_MAIN tests/test_transfer_transaction.c $(TRANSFER_TRANSACTION_SOURCES) -o $@ $(LDLIBS)
