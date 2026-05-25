# Meletricks — macOS / Linux build
# Toolchain: brew install --cask gcc-arm-embedded
#        or: brew install arm-none-eabi-gcc

BOARD_DIR := sdk/boards/zoomtkldyna
BUILD     := build

CC  := arm-none-eabi-gcc
LD  := arm-none-eabi-ld
OCP := arm-none-eabi-objcopy
NM  := arm-none-eabi-nm

PYTHON ?= python3

CFLAGS := \
    -mcpu=cortex-m3 -mthumb -O2 -g          \
    -ffreestanding -fomit-frame-pointer      \
    -fno-exceptions -fno-unwind-tables       \
    -ffunction-sections -fdata-sections      \
    -Wall                                    \
    -Isdk/include -Iinclude -I$(BOARD_DIR)   \
    -Iapp -Iapp/widgets                      \
    -MMD -MP

# Assembly doesn't take C-only flags
ASFLAGS := -mcpu=cortex-m3 -mthumb -g

LDFLAGS := \
    -L sdk \
    -T $(BOARD_DIR)/zoomtkldyna.ld \
    --gc-sections \
    --emit-relocs

SRCS_C := \
    $(wildcard src/*.c)         \
    $(wildcard src/widgets/*.c) \
    $(wildcard fonts/*.c)       \
    $(wildcard app/*.c)         \
    $(wildcard app/widgets/*.c) \
    $(wildcard app/screens/*.c) \
    $(wildcard sdk/src/*.c)

SRCS_S := $(wildcard sdk/src/*.s)

OBJS := \
    $(patsubst %.c,$(BUILD)/%.o,$(SRCS_C)) \
    $(patsubst %.s,$(BUILD)/%.o,$(SRCS_S))

DEPS := $(OBJS:.o=.d)

TARGET_ELF := meletricks.elf
TARGET_BIN := meletricks.bin

.PHONY: all clean

all: $(TARGET_BIN)
	@echo "Build OK"
	@$(NM) -n $(TARGET_ELF) | grep -v " A "

$(TARGET_BIN): $(TARGET_ELF)
	$(OCP) -O binary $< $@

$(TARGET_ELF): $(OBJS)
	$(LD) $(LDFLAGS) $^ -o $@

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.s
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD) $(TARGET_ELF) $(TARGET_BIN) $(EMU_BUILD) $(EMU_ELF)

# ── Flash over BLE ────────────────────────────────────────────────────
#
# `make upload` builds the firmware and pushes it to the keyboard over
# BLE via tools/TweakLoader.  Device discovery scans for advertisements
# whose name contains $(DEVICE_FILTER) — exact one match auto-flashes,
# multiple matches prompt; override either:
#
#   make upload                          # scan, filter by name
#   make upload DEVICE_FILTER="my kbd"   # different name substring
#   make upload DEVICE=AA:BB:CC:DD:EE:FF # explicit address, skip scan
#
# Requires keystone-engine, capstone, bleak, pyelftools.  On macOS the
# keystone wheel needs libkeystone too: `brew install keystone`.

DEVICE_FILTER ?= ZOOM TKL
DEVICE        ?=
TWEAKLOADER   := tools/TweakLoader/tweakloader.py

upload_args = $(if $(DEVICE),--device $(DEVICE),--name "$(DEVICE_FILTER)")

.PHONY: upload scan

upload: $(TARGET_ELF)
	$(PYTHON) $(TWEAKLOADER) upload $(TARGET_ELF) $(upload_args)

scan:
	$(PYTHON) $(TWEAKLOADER) scan $(if $(DEVICE_FILTER),--name "$(DEVICE_FILTER)",)

# ── Host emulator guest build ─────────────────────────────────────────
#
# `make emu` builds an ELF (meletricks_emu.elf) without sdk/src/ — every
# SDK API symbol is resolved by tools/emu/emu.ld to a fixed trap address.
# tools/emu/emu.py loads that ELF in Unicorn and runs the app, dispatching
# each trap to a Python handler.

EMU_DIR   := tools/emu
EMU_BUILD := build_emu
EMU_ELF   := meletricks_emu.elf

EMU_CFLAGS := \
    -mcpu=cortex-m3 -mthumb -O2 -g           \
    -ffreestanding -fomit-frame-pointer       \
    -fno-exceptions -fno-unwind-tables        \
    -ffunction-sections -fdata-sections       \
    -fno-pic -fno-pie                         \
    -Wall                                     \
    -DSDK_EMU_BUILD                           \
    -Isdk/include -Iinclude -I$(BOARD_DIR)    \
    -Iapp -Iapp/widgets                       \
    -MMD -MP

EMU_LDFLAGS := -T $(EMU_DIR)/emu.ld --gc-sections --emit-relocs

# App + lib + fonts + emu-only guest stubs.  Excludes sdk/src/.
EMU_SRCS_C := \
    $(wildcard src/*.c)         \
    $(wildcard src/widgets/*.c) \
    $(wildcard fonts/*.c)       \
    $(wildcard app/*.c)         \
    $(wildcard app/widgets/*.c) \
    $(wildcard app/screens/*.c) \
    $(EMU_DIR)/guest_stubs.c

EMU_OBJS := $(patsubst %.c,$(EMU_BUILD)/%.o,$(EMU_SRCS_C))
EMU_DEPS := $(EMU_OBJS:.o=.d)

.PHONY: emu emu-build emu-clean

# `make emu` builds + immediately launches the emulator.  Pass extra
# python args via EMU_ARGS, e.g. `make emu EMU_ARGS="--battery 30 --caps"`.
# For build-only, run `make emu-build`.
EMU_ARGS ?=

emu: emu-build
	@echo "Emu build OK — launching..."
	$(PYTHON) $(EMU_DIR)/emu.py $(EMU_ELF) $(EMU_ARGS)

emu-build: $(EMU_ELF)

$(EMU_ELF): $(EMU_OBJS) $(EMU_DIR)/emu.ld
	$(LD) $(EMU_LDFLAGS) $(EMU_OBJS) -o $@

$(EMU_BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(EMU_CFLAGS) -c $< -o $@

emu-clean:
	rm -rf $(EMU_BUILD) $(EMU_ELF)

-include $(DEPS)
-include $(EMU_DEPS)
