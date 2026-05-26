# Meletricks — macOS / Linux build
# Toolchain: brew install --cask gcc-arm-embedded
#        or: brew install arm-none-eabi-gcc

# Each board lives under sdk/boards/<name>/ and supplies a board.h with FW_*
# addresses.  Build products are namespaced per board so switching never
# reuses stale objects.
#
#   make                  # build every board found under sdk/boards/
#   make BOARD=zoom75tiga # build just that one
#   make BOARD=X upload   # flash that one
#   make clean            # wipe build/ for all boards
#
BOARDS := $(notdir $(wildcard sdk/boards/*))

ifndef BOARD
# ─── Top-level: no BOARD specified, iterate over every board ───
.DEFAULT_GOAL := all
.PHONY: all release clean $(BOARDS) upload emu emu-build emu-clean scan

all: $(BOARDS)

$(BOARDS):
	@$(MAKE) --no-print-directory BOARD=$@

# `make release` builds every board and copies the .bin/.elf into release/
# with the board name in the filename, ready for distribution.
release:
	@for b in $(BOARDS); do $(MAKE) --no-print-directory BOARD=$$b release || exit $$?; done

clean:
	rm -rf build release

# Remove the legacy build products that used to land in the project root.
.PHONY: clean-legacy
clean-legacy:
	rm -rf meletricks.elf meletricks.bin meletricks_emu.elf build_emu emu_frames

upload emu emu-build emu-clean scan:
	@echo "$@: specify BOARD=<name>. Available: $(BOARDS)" >&2; exit 2

else
# ─── Per-board build (BOARD is set) ───
BOARD_DIR := sdk/boards/$(BOARD)
BUILD     := build/$(BOARD)

ifeq ($(wildcard $(BOARD_DIR)/board.h),)
$(error Unknown BOARD '$(BOARD)': $(BOARD_DIR)/board.h not found. Available: $(BOARDS))
endif

CC  := arm-none-eabi-gcc
LD  := arm-none-eabi-ld
OCP := arm-none-eabi-objcopy
NM  := arm-none-eabi-nm

PYTHON ?= python3

CFLAGS := \
    -mcpu=cortex-m3 -mthumb -O2 -g3         \
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
    -T sdk/fr8000.ld \
    --gc-sections \
    --emit-relocs \
    --undefined=fw_rtc_set

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

TARGET_ELF := $(BUILD)/meletricks.elf
TARGET_BIN := $(BUILD)/meletricks.bin

RELEASE_DIR := release

.PHONY: all release clean

all: $(TARGET_BIN)
	@echo "[$(BOARD)] Build OK"
	@$(NM) -n $(TARGET_ELF) | grep -v " A "

release: $(TARGET_BIN)
	@mkdir -p $(RELEASE_DIR)
	cp $(TARGET_BIN) $(RELEASE_DIR)/meletricks-$(BOARD).bin
	cp $(TARGET_ELF) $(RELEASE_DIR)/meletricks-$(BOARD).elf
	@echo "[$(BOARD)] release -> $(RELEASE_DIR)/meletricks-$(BOARD).{bin,elf}"

$(TARGET_BIN): $(TARGET_ELF)
	$(OCP) -O binary $< $@

$(TARGET_ELF): $(OBJS)
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) $^ -o $@

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.s
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD)

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
EMU_BUILD := $(BUILD)/emu
EMU_ELF   := $(BUILD)/meletricks_emu.elf

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
	@mkdir -p $(dir $@)
	$(LD) $(EMU_LDFLAGS) $(EMU_OBJS) -o $@

$(EMU_BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(EMU_CFLAGS) -c $< -o $@

emu-clean:
	rm -rf $(EMU_BUILD) $(EMU_ELF)

-include $(DEPS)
-include $(EMU_DEPS)

endif  # ifndef BOARD
