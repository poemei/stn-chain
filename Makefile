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
CFLAGS += -std=c17 -Wall -Wextra -Wpedantic -Iincludes -Isrc -Isrc/crypto/ed25519_donna -Iplatforms/linux -pthread
PLATFORM_CFLAGS := -D_GNU_SOURCE
LDLIBS += -lcrypto -pthread
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
CORE_SOURCES := src/main.c src/stn_address.c src/stn_config.c src/stn_authority.c src/stn_block.c src/stn_block_compensation.c src/stn_block_compensation_replay.c src/stn_block_compensation_acceptance.c src/stn_block_compensation_candidate.c src/stn_block_reward.c src/stn_chain.c src/stn_economy.c src/stn_compensation.c src/stn_issuance.c src/stn_economic_state.c src/stn_compensation_state.c src/stn_issuance_binding.c src/stn_wallet.c src/stn_transfer.c src/stn_transfer_replay.c src/stn_transfer_binding.c src/stn_transfer_authorization.c src/stn_transfer_acceptance.c src/stn_transfer_envelope.c src/stn_transfer_envelope_replay.c src/stn_transfer_envelope_authorization.c src/stn_transfer_envelope_acceptance.c src/stn_fork.c src/stn_identity.c src/stn_sentinel_intelligence.c src/stn_lifecycle.c src/stn_mining.c src/stn_internal_miner.c src/stn_node_service.c src/stn_peer.c src/stn_pending.c src/stn_pending_cleanup.c src/stn_share_pending.c src/stn_pow.c src/stn_record.c src/stn_replay.c src/stn_report.c src/stn_rpc.c src/stn_share.c src/stn_share_replay.c src/stn_storage.c src/stn_transaction.c src/stn_transaction_status.c src/stn_validation.c src/stn_contract.c src/stn_contract_consensus.c src/stn_contract_lineage.c src/stn_contract_state.c src/stn_contract_snapshot.c src/stn_contract_transaction.c src/stn_contract_query.c src/stn_contract_response.c
CRYPTO_SOURCES := src/crypto/ed25519_donna/ed25519_provider.c
PLATFORM_SOURCES := platforms/linux/stn_sha256.c platforms/linux/stn_linux_storage.c platforms/linux/stn_linux_peer.c platforms/linux/stn_app_linux.c
SOURCES := $(CORE_SOURCES) $(CRYPTO_SOURCES) $(PLATFORM_SOURCES)
OBJECTS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SOURCES))
.PHONY: all configure clean install install-service uninstall info test-contract-query test-contract-response test-transaction-status test-linux-info test-mining-capacity
all: configure $(TARGET_PATH)

# [AI:GPT-6 | 2026-09-28 14:52:23 UTC]
# The test includes stn_app_linux.c to exercise its private INFO helpers.
test-linux-info: $(BUILD_DIR)/test-linux-info
	$(BUILD_DIR)/test-linux-info
$(BUILD_DIR)/test-linux-info: tests/test_linux_info.c $(filter-out src/main.c,$(CORE_SOURCES)) $(CRYPTO_SOURCES) $(PLATFORM_SOURCES)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(PLATFORM_CFLAGS) tests/test_linux_info.c $(filter-out src/main.c,$(CORE_SOURCES)) $(CRYPTO_SOURCES) $(filter-out platforms/linux/stn_app_linux.c,$(PLATFORM_SOURCES)) -o $@ $(LDLIBS)
# [End AI:GPT-6]
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
	install -m 0755 $(TARGET_PATH) $(DESTDIR)$(BINDIR)
	install -d $(DESTDIR)$(DATADIR)
	install -d $(DESTDIR)$(LOGDIR)
	install -d -o stnchain -g stnchain $(DESTDIR)/etc/stn-chain
	@if [ ! -e "$(DESTDIR)/etc/stn-chain/chain_config.json" ]; then install -o stnchain -g stnchain -m 0644 config/chain_config.json $(DESTDIR)/etc/stn-chain/chain_config.json; else chown stnchain:stnchain $(DESTDIR)/etc/stn-chain/chain_config.json; echo "Preserving existing /etc/stn-chain/chain_config.json"; fi
	install -d $(DESTDIR)/etc/systemd/system
	sed 's|@BINDIR@|$(BINDIR)|g' platforms/linux/stn-chain.service.in > $(BUILD_DIR)/stn-chain.service
	install -m 0644 $(BUILD_DIR)/stn-chain.service $(DESTDIR)/etc/systemd/system/stn-chain.service
	@if [ -z "$(DESTDIR)" ]; then systemctl daemon-reload; systemctl enable stn-chain; systemctl restart stn-chain; else echo "DESTDIR staging: systemd activation skipped."; fi
	@echo
	@echo "Installed:"
	@echo "  Application: $(BINDIR)/$(TARGET)"
	@echo "  Service:     /etc/systemd/system/stn-chain.service"
	@echo "  Config:      /etc/stn-chain/chain_config.json"
	@echo "  Data:         $(DATADIR)"
	@echo "  Logs:         $(LOGDIR)"
uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
	@echo "Removed $(BINDIR)/$(TARGET)"
	@echo "Data and logs preserved:"
	@echo "  $(DATADIR)"
	@echo "  $(LOGDIR)"
install-service: install
clean:
	rm -rf $(BUILD_DIR)
	@echo "Build files removed."

test-contract-query: $(BUILD_DIR)/test-contract-query
	$(BUILD_DIR)/test-contract-query
$(BUILD_DIR)/test-contract-query: tests/test_contract_query.c src/stn_contract_query.c src/stn_contract_state.c src/stn_contract_consensus.c src/stn_contract_lineage.c src/stn_contract.c src/stn_address.c src/stn_authority.c src/stn_identity.c src/crypto/ed25519_donna/ed25519_provider.c platforms/linux/stn_sha256.c includes/stn_contract_query.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -DSTN_CONTRACT_QUERY_TEST_MAIN tests/test_contract_query.c src/stn_contract_query.c src/stn_contract_state.c src/stn_contract_consensus.c src/stn_contract_lineage.c src/stn_contract.c src/stn_address.c src/stn_authority.c src/stn_identity.c src/crypto/ed25519_donna/ed25519_provider.c platforms/linux/stn_sha256.c -o $@ $(LDLIBS)

test-contract-response: $(BUILD_DIR)/test-contract-response
	$(BUILD_DIR)/test-contract-response
$(BUILD_DIR)/test-contract-response: tests/test_contract_response.c src/stn_contract_response.c src/stn_contract.c src/stn_address.c src/stn_identity.c src/crypto/ed25519_donna/ed25519_provider.c platforms/linux/stn_sha256.c includes/stn_contract_response.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_contract_response.c src/stn_contract_response.c src/stn_contract.c src/stn_address.c src/stn_identity.c src/crypto/ed25519_donna/ed25519_provider.c platforms/linux/stn_sha256.c -o $@ $(LDLIBS)

test-transaction-status: $(BUILD_DIR)/test-transaction-status
	$(BUILD_DIR)/test-transaction-status
$(BUILD_DIR)/test-transaction-status: tests/test_transaction_status.c src/stn_transaction_status.c $(filter-out src/main.c,$(CORE_SOURCES)) $(CRYPTO_SOURCES) $(PLATFORM_SOURCES)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(PLATFORM_CFLAGS) -DSTN_TRANSACTION_STATUS_TEST_MAIN tests/test_transaction_status.c $(filter-out src/main.c,$(CORE_SOURCES)) $(CRYPTO_SOURCES) $(PLATFORM_SOURCES) -o $@ $(LDLIBS)

test-mining-capacity: $(BUILD_DIR)/test-mining-capacity
	$(BUILD_DIR)/test-mining-capacity
$(BUILD_DIR)/test-mining-capacity: tests/test_mining_capacity.c includes/stn_block.h includes/stn_economy.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_mining_capacity.c -o $@
