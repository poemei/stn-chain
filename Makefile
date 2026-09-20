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
	src/stn_authority.c \
	src/stn_block.c \
	src/stn_chain.c \
	src/stn_fork.c \
	src/stn_identity.c \
	src/stn_intelligence.c \
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
	src/stn_validation.c

CRYPTO_SOURCES := \
	src/crypto/ed25519_donna/ed25519.c \
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

.PHONY: all configure clean install uninstall info

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
	@echo "  $(LOGDIR)"

clean:
	rm -rf $(BUILD_DIR)
	@echo "Build files removed."