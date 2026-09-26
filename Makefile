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
	src/stn_config.c \
	src/stn_authority.c \
	src/stn_block.c \
	src/stn_block_compensation.c \
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
	src/stn_internal_miner.c \
	src/stn_node_service.c \
	src/stn_peer.c \
	src/stn_pending.c \
	src/stn_pow.c \
	src/stn_record.c \
	src/stn_replay.c \
	src/stn_report.c \
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

CRYPTO_SOURCES := src/crypto/ed25519_donna/ed25519_provider.c
PLATFORM_SOURCES := platforms/linux/stn_sha256.c platforms/linux/stn_linux_storage.c platforms/linux/stn_linux_peer.c platforms/linux/stn_app_linux.c
SOURCES := $(CORE_SOURCES) $(CRYPTO_SOURCES) $(PLATFORM_SOURCES)
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
	install -d -o stnchain -g stnchain $(DESTDIR)/etc/stn-chain
	@if [ ! -e "$(DESTDIR)/etc/stn-chain/chain_config.json" ]; then install -o stnchain -g stnchain -m 0644 config/chain_config.json $(DESTDIR)/etc/stn-chain/chain_config.json; else chown stnchain:stnchain $(DESTDIR)/etc/stn-chain/chain_config.json; echo "Preserving existing /etc/stn-chain/chain_config.json"; fi
	install -d $(DESTDIR)/etc/systemd/system
	sed 's|@BINDIR@|$(BINDIR)|g' platforms/linux/stn-chain.service.in > $(BUILD_DIR)/stn-chain.service
	install -m 0644 $(BUILD_DIR)/stn-chain.service $(DESTDIR)/etc/systemd/system/stn-chain.service
	@if [ -z "$(DESTDIR)" ]; then systemctl daemon-reload; systemctl enable stn-chain; systemctl restart stn-chain; else echo "DESTDIR staging: systemd activation skipped."; fi
install-service: install
uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
clean:
	rm -rf $(BUILD_DIR)

# Block compensation qualification.
.PHONY: test-block-compensation
test-block-compensation: $(BUILD_DIR)/test-block-compensation
	$(BUILD_DIR)/test-block-compensation
$(BUILD_DIR)/test-block-compensation: tests/test_block_compensation.c src/stn_block_compensation.c platforms/linux/stn_sha256.c includes/stn_block_compensation.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -DSTN_BLOCK_COMPENSATION_TEST_MAIN tests/test_block_compensation.c src/stn_block_compensation.c platforms/linux/stn_sha256.c -o $@ $(LDLIBS)

# Keep existing qualification targets below this marker through the repository's generated/legacy test definitions.
include Makefile.tests
