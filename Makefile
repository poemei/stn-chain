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
	src/stn_storage.c \
	src/stn_transaction.c \
	src/stn_validation.c \
	src/stn_contract.c

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
