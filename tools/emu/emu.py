#!/usr/bin/env python3
"""
Meletricks host emulator.

Loads the no-SDK guest ELF (built by `make emu`) into Unicorn, traps every
SDK API call at its fixed address (see tools/emu/emu.ld), and drives the
app's FR_SETUP + FR_TASK functions from Python.  A pygame frontend shows
the framebuffer and forwards key events into the SDK's kbd queue.

    pip install unicorn pygame pyelftools
    make emu
    python3 tools/emu/emu.py meletricks_emu.elf

Hold ESC to quit.  Arrow keys / Enter / Backspace map to the LCD nav
buttons; letter keys send HID scancodes so WPM and the typing log work.
"""

from __future__ import annotations

import argparse
import collections
import datetime
import logging
import random
import sys
import time
from pathlib import Path
from typing import Callable, Dict

from elftools.elf.elffile import ELFFile
from unicorn import (
    UC_ARCH_ARM,
    UC_HOOK_CODE,
    UC_HOOK_MEM_INVALID,
    UC_MODE_THUMB,
    UC_PROT_ALL,
    UC_PROT_EXEC,
    UC_PROT_READ,
    UC_PROT_WRITE,
    Uc,
    UcError,
)
from unicorn.arm_const import (
    UC_ARM_REG_LR,
    UC_ARM_REG_PC,
    UC_ARM_REG_R0,
    UC_ARM_REG_R1,
    UC_ARM_REG_R2,
    UC_ARM_REG_R3,
    UC_ARM_REG_SP,
)


log = logging.getLogger("emu")


# ── Guest memory layout (matches tools/emu/emu.ld) ─────────────────────

LCD_W, LCD_H = 320, 172
FB_ADDR = 0x22000000
FB_SIZE = 0x20000                       # 128 KB, covers 320*172*2 = 110160 B

PSRAM_ADDR = 0x220E0000
PSRAM_SIZE = 0x400000                   # 4 MB headroom for .text/.data/.bss/heap/stack

TRAP_BASE = 0x60000000
TRAP_SIZE = 0x1000

# A sentinel LR pushed before every guest call.  emu_start stops when PC
# reaches it, so we don't need an explicit hook.  Picked inside the trap
# region so it's always mapped.
SENTINEL = TRAP_BASE + 0xFFC


# ── SDK trap addresses (must match emu.ld PROVIDE() values) ────────────

TRAP_TABLE: Dict[str, int] = {
    "fr_micros":                0x60000001,
    "fr_millis":                0x60000005,
    "srand":                    0x60000009,
    "rand":                     0x6000000D,
    "ke_malloc":                0x60000011,
    "ke_free":                  0x60000015,
    "kbd_event_pop":            0x60000019,
    "kbd_battery_get":          0x6000001D,
    "kbd_battery_set_callback": 0x60000021,
    "kbd_status_get":           0x60000025,
    "kbd_status_set_callback":  0x60000029,
    "gui_pause":                0x6000002D,
    "lcd_te_sync_disable":      0x60000031,
    "lcd_is_idle":              0x60000035,
    "lcd_sleep":                0x60000039,
    "lcd_wake":                 0x6000003D,
    "m_draw_frame":             0x60000041,
    "sdk_get_fw_override":      0x60000045,
    "sdk_set_fw_override":      0x60000049,
    "lcd_rtc_get_live":         0x6000004D,
}


# ── ARM helpers ───────────────────────────────────────────────────────

_ARG_REGS = (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3)


def get_args(uc: Uc, n: int):
    return tuple(uc.reg_read(_ARG_REGS[i]) for i in range(n))


def set_ret(uc: Uc, value: int) -> None:
    uc.reg_write(UC_ARM_REG_R0, value & 0xFFFFFFFF)


# ── Configurable status state ─────────────────────────────────────────
#
# Mirrors the real keyboard MCU's two status snapshots (kbd_battery_t and
# kbd_status_t).  *_get handlers return these values verbatim; fire_*_cb()
# also invokes the guest-registered callbacks so widgets repaint without
# waiting for the app's next bind/poll.


# kbd_event.h enums.
CONN_NONE, CONN_BT_1, CONN_BT_2, CONN_BT_3, CONN_2_4G, CONN_WIRED = 0, 1, 2, 3, 4, 5
OS_NONE, OS_MAC, OS_WIN = 0, 1, 2

CONN_NAMES = {
    "none": CONN_NONE,
    "bt1":  CONN_BT_1,
    "bt2":  CONN_BT_2,
    "bt3":  CONN_BT_3,
    "2.4g": CONN_2_4G,
    "usb":  CONN_WIRED,
}
CONN_LABEL = {v: k for k, v in CONN_NAMES.items()}
CONN_CYCLE = [CONN_NONE, CONN_BT_1, CONN_BT_2, CONN_BT_3, CONN_2_4G, CONN_WIRED]

OS_NAMES = {"none": OS_NONE, "mac": OS_MAC, "win": OS_WIN}
OS_LABEL = {v: k for k, v in OS_NAMES.items()}
OS_CYCLE = [OS_NONE, OS_MAC, OS_WIN]


class StatusState:
    """Backing store for kbd_battery_t and kbd_status_t."""

    def __init__(self) -> None:
        self.battery_pct = 85         # 0..100, or 0xFF to signal "no data"
        self.is_charging = 0          # 0/1, or 0xFF for "no data"
        self.caps = 0                 # 0/1
        self.conn = CONN_WIRED        # KBD_CONN_*
        self.layer = 0                # 0..N
        self.os = OS_MAC              # KBD_OS_*
        self.winlock = 0              # 0/1


# ── Emulator core ─────────────────────────────────────────────────────


class Emu:
    """Loads the guest ELF, drives FR_SETUP / FR_TASK, dispatches SDK traps."""

    # sdk_task_t laid out in tools/emu/emu.ld is identical to sdk/include/entry.h:
    #   entry(4) stack_base(4) stack_size(4) saved_sp(4) resume_lr(4) saved_regs(32) suspended(1)
    # padded to 4-byte alignment -> 56 bytes per entry.
    SDK_TASK_T_SIZE = 56

    def __init__(self, elf_path: Path) -> None:
        self.uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB)

        # Last RGB565 frame captured from m_draw_frame.  The frontend reads
        # this each loop and blits to pygame.
        self.last_frame: bytes = b""
        self.frame_ready = False

        self.kbd_queue: collections.deque = collections.deque(maxlen=64)
        self.rng = random.Random(0xDEADBEEF)
        self.fw_override = 0

        # ke_malloc bump allocator state, populated after symbols load.
        self.heap_next = 0
        self.heap_end = 0
        self.stack_top = 0

        # Mutable status (battery, caps, conn, layer, …).  *_get reads it
        # directly; setters call fire_*_cb() to push the change into the
        # guest so widgets repaint immediately.
        self.status = StatusState()

        # Guest-registered callback function pointers.  0 until the app
        # binds its widgets via the *_set_callback traps.
        self.battery_cb = 0
        self.status_cb = 0

        self.t_zero = time.monotonic()

        self._map_memory()
        self._load_elf(elf_path)
        self._install_hooks()

        # Tiny scratch carved off the front of the heap.  Used as the
        # output buffer for *_get and the inbound-struct buffer for the
        # fire_*_cb path; both kbd_battery_t (8 B) and kbd_status_t (12 B)
        # fit comfortably with room for future growth.
        self.host_scratch = self.heap_next
        self.heap_next += 64

        log.info("entry hello_setup=0x%08x", self.setup_entry)
        log.info("heap %s..%s (%d KB), stack_top %s",
                 hex(self.heap_next), hex(self.heap_end),
                 (self.heap_end - self.heap_next) // 1024,
                 hex(self.stack_top))
        log.info("%d task(s) registered", len(self.tasks))
        for i, t in enumerate(self.tasks):
            log.info("  task %d: entry=0x%08x stack=0x%08x sz=0x%x",
                     i, t["entry"], t["stack_base"], t["stack_size"])

    # ── Memory map + ELF load ─────────────────────────────────────

    def _map_memory(self) -> None:
        self.uc.mem_map(FB_ADDR, FB_SIZE, UC_PROT_READ | UC_PROT_WRITE)
        self.uc.mem_map(PSRAM_ADDR, PSRAM_SIZE, UC_PROT_ALL)
        self.uc.mem_map(TRAP_BASE, TRAP_SIZE, UC_PROT_READ | UC_PROT_EXEC)

        # Fill the trap region with `bx lr` (Thumb 0x4770) so that even if
        # a hook misfires, execution returns cleanly.  We rely on emu_start
        # `until=SENTINEL` to stop the dispatcher; we don't need an
        # instruction at the sentinel itself.
        self.uc.mem_write(TRAP_BASE, b"\x70\x47" * (TRAP_SIZE // 2))

    def _load_elf(self, elf_path: Path) -> None:
        with elf_path.open("rb") as fh:
            elf = ELFFile(fh)

            for seg in elf.iter_segments():
                if seg["p_type"] != "PT_LOAD" or seg["p_filesz"] == 0:
                    continue
                addr = seg["p_paddr"]
                data = seg.data()
                self.uc.mem_write(addr, data)

            # Symbol table.
            symtab = elf.get_section_by_name(".symtab")
            if symtab is None:
                sys.exit("ELF has no .symtab — strip it not")
            syms = {s.name: s["st_value"] for s in symtab.iter_symbols()}

        # FR_SETUP is the first function in .fr_user_setup; we just jump to
        # _fr_user_setup_start.  An ENTRY directive is irrelevant since we
        # drive PC ourselves.
        self.setup_entry = syms["_fr_user_setup_start"] & ~1
        setup_end = syms["_fr_user_setup_end"]
        self.has_setup = setup_end > syms["_fr_user_setup_start"]

        self.heap_next = syms["_heap_start"]
        self.heap_end = syms["_heap_end"]
        self.stack_top = syms["_stack_top"]

        # Walk the task table.
        tt_start = syms["_fr_user_tasks_table_start"]
        tt_end = syms["_fr_user_tasks_table_end"]
        self.tasks = []
        addr = tt_start
        while addr < tt_end:
            entry = int.from_bytes(self.uc.mem_read(addr, 4), "little")
            stack_base = int.from_bytes(self.uc.mem_read(addr + 4, 4), "little")
            stack_size = int.from_bytes(self.uc.mem_read(addr + 8, 4), "little")
            self.tasks.append({
                "entry": entry,        # Thumb bit set
                "stack_base": stack_base,
                "stack_size": stack_size,
            })
            addr += self.SDK_TASK_T_SIZE

    # ── Hook install ──────────────────────────────────────────────

    def _install_hooks(self) -> None:
        for name, trap_addr in TRAP_TABLE.items():
            real = trap_addr & ~1
            handler = SDK_HANDLERS[name]
            # Capture name + handler in default args.
            def hook(uc, _addr, _size, _ud, _h=handler, _n=name):
                try:
                    _h(self, uc)
                except Exception:
                    log.exception("SDK handler %s raised", _n)
                    uc.emu_stop()
                # Return via LR.  Unicorn's ARM emu uses bit 0 of any
                # value written to PC as a mode hint (set = Thumb, clear =
                # ARM); preserve it or we silently switch to ARM-decode and
                # the next valid Thumb instruction looks like garbage.
                uc.reg_write(UC_ARM_REG_PC, uc.reg_read(UC_ARM_REG_LR))
            self.uc.hook_add(UC_HOOK_CODE, hook, begin=real, end=real)

        # Invalid memory access — log and stop instead of silently failing.
        def on_invalid(uc, access, address, size, value, _ud):
            log.error("INVALID MEM access=%s addr=0x%08x size=%d value=%d pc=0x%08x",
                      access, address, size, value, uc.reg_read(UC_ARM_REG_PC))
            return False
        self.uc.hook_add(UC_HOOK_MEM_INVALID, on_invalid)

    # ── Time + RNG + heap ─────────────────────────────────────────

    def micros(self) -> int:
        return int((time.monotonic() - self.t_zero) * 1_000_000) & 0xFFFFFFFF

    def millis(self) -> int:
        return int((time.monotonic() - self.t_zero) * 1_000) & 0xFFFFFFFF

    def ke_malloc(self, size: int) -> int:
        # Align up to 8 — matches what most embedded allocators do, and the
        # SDK comment mentions firmware resets on OOM (pool starvation) so
        # we want a clean OOM instead of silent corruption.
        size = (size + 7) & ~7
        if self.heap_next + size > self.heap_end:
            log.error("ke_malloc OOM: requested %d, have %d remaining",
                      size, self.heap_end - self.heap_next)
            return 0
        addr = self.heap_next
        self.heap_next += size
        return addr

    # ── Public: input + tick driving ──────────────────────────────

    def push_kbd_lcd(self, action: int) -> None:
        self.kbd_queue.append({"type": 1, "code": action, "time_ms": self.millis()})

    def push_kbd_key(self, scancode: int) -> None:
        self.kbd_queue.append({"type": 0, "code": scancode, "time_ms": self.millis()})

    def run_setup(self) -> None:
        if not self.has_setup:
            return
        self._call_guest(self.setup_entry, self.stack_top, timeout_us=10_000_000)

    def run_tick(self) -> None:
        for task in self.tasks:
            entry_pc = task["entry"] & ~1
            task_sp = task["stack_base"] + task["stack_size"]
            self._call_guest(entry_pc, task_sp, timeout_us=2_000_000)

    def _call_guest(self, pc: int, sp: int, *, timeout_us: int) -> None:
        self.uc.reg_write(UC_ARM_REG_SP, sp)
        self.uc.reg_write(UC_ARM_REG_LR, SENTINEL | 1)
        try:
            self.uc.emu_start(pc | 1, SENTINEL, timeout=timeout_us)
        except UcError as e:
            cur_pc = self.uc.reg_read(UC_ARM_REG_PC)
            cur_lr = self.uc.reg_read(UC_ARM_REG_LR)
            log.error("guest fault @ pc=0x%08x lr=0x%08x: %s", cur_pc, cur_lr, e)
            raise

    def take_frame(self) -> bytes | None:
        """Return the most recent framebuffer if one has been presented since
        the last call, else None."""
        if not self.frame_ready:
            return None
        self.frame_ready = False
        return self.last_frame

    # ── Status struct serialization + callback firing ─────────────

    def _pack_battery(self) -> bytes:
        # kbd_battery_t: u8 pct, u8 charging, 2B pad, u32 last_update_ms.
        return (bytes([self.status.battery_pct & 0xFF,
                       self.status.is_charging & 0xFF, 0, 0])
                + self.millis().to_bytes(4, "little"))

    def _pack_status(self) -> bytes:
        # kbd_status_t: u8 caps, u8 conn, u8 layer, u8 os, u8 winlock,
        #               3B pad, u32 last_update_ms.
        return (bytes([self.status.caps & 0xFF,
                       self.status.conn & 0xFF,
                       self.status.layer & 0xFF,
                       self.status.os & 0xFF,
                       self.status.winlock & 0xFF,
                       0, 0, 0])
                + self.millis().to_bytes(4, "little"))

    def fire_battery_cb(self) -> None:
        if not self.battery_cb:
            return
        self.uc.mem_write(self.host_scratch, self._pack_battery())
        self.uc.reg_write(UC_ARM_REG_R0, self.host_scratch)
        self._call_guest(self.battery_cb & ~1, self.stack_top,
                         timeout_us=1_000_000)

    def fire_status_cb(self) -> None:
        if not self.status_cb:
            return
        self.uc.mem_write(self.host_scratch, self._pack_status())
        self.uc.reg_write(UC_ARM_REG_R0, self.host_scratch)
        self._call_guest(self.status_cb & ~1, self.stack_top,
                         timeout_us=1_000_000)

    # ── Status mutators (used by CLI + hotkeys) ───────────────────

    def set_battery(self, pct: int, *, charging: int | None = None) -> None:
        pct = max(0, min(100, pct))
        changed = pct != self.status.battery_pct
        self.status.battery_pct = pct
        if charging is not None and charging != self.status.is_charging:
            self.status.is_charging = 1 if charging else 0
            changed = True
        if changed:
            self.fire_battery_cb()

    def set_charging(self, on: bool) -> None:
        self.status.is_charging = 1 if on else 0
        self.fire_battery_cb()

    def set_caps(self, on: bool) -> None:
        self.status.caps = 1 if on else 0
        self.fire_status_cb()

    def set_conn(self, conn: int) -> None:
        self.status.conn = conn
        self.fire_status_cb()

    def set_layer(self, layer: int) -> None:
        self.status.layer = max(0, min(9, layer))
        self.fire_status_cb()

    def set_os(self, os_mode: int) -> None:
        self.status.os = os_mode
        self.fire_status_cb()

    def set_winlock(self, on: bool) -> None:
        self.status.winlock = 1 if on else 0
        self.fire_status_cb()

    def cycle_conn(self) -> int:
        i = CONN_CYCLE.index(self.status.conn) if self.status.conn in CONN_CYCLE else 0
        new = CONN_CYCLE[(i + 1) % len(CONN_CYCLE)]
        self.set_conn(new)
        return new

    def cycle_os(self) -> int:
        i = OS_CYCLE.index(self.status.os) if self.status.os in OS_CYCLE else 0
        new = OS_CYCLE[(i + 1) % len(OS_CYCLE)]
        self.set_os(new)
        return new

    def cycle_layer(self) -> int:
        self.set_layer((self.status.layer + 1) % 6)
        return self.status.layer

    def bump_battery(self, delta: int) -> int:
        self.set_battery(self.status.battery_pct + delta)
        return self.status.battery_pct


# ── SDK trap handlers ─────────────────────────────────────────────────


def _h_fr_micros(emu: Emu, uc: Uc) -> None:
    set_ret(uc, emu.micros())


def _h_fr_millis(emu: Emu, uc: Uc) -> None:
    set_ret(uc, emu.millis())


def _h_srand(emu: Emu, uc: Uc) -> None:
    (seed,) = get_args(uc, 1)
    emu.rng = random.Random(seed)


def _h_rand(emu: Emu, uc: Uc) -> None:
    # ROM rand() returns positive int (signed range, but treated as u32 below);
    # we mirror RAND_MAX = 0x7FFFFFFF.
    set_ret(uc, emu.rng.randint(0, 0x7FFFFFFF))


def _h_ke_malloc(emu: Emu, uc: Uc) -> None:
    size, _kind = get_args(uc, 2)
    addr = emu.ke_malloc(size)
    set_ret(uc, addr)


def _h_ke_free(emu: Emu, uc: Uc) -> None:
    # Bump allocator; widgets/screens never free in the MVP path.  If a
    # call shows up in logs we'll need to switch to a real allocator.
    pass


def _h_kbd_event_pop(emu: Emu, uc: Uc) -> None:
    (out_ptr,) = get_args(uc, 1)
    if not emu.kbd_queue:
        set_ret(uc, 0)
        return
    ev = emu.kbd_queue.popleft()
    # kbd_event_t layout (sdk/include/kbd_event.h):
    #   u8 type @0, u8 code @1, pad @2..3, u32 time_ms @4..7   (total 8)
    buf = bytes([ev["type"] & 0xFF, ev["code"] & 0xFF, 0, 0])
    buf += (ev["time_ms"] & 0xFFFFFFFF).to_bytes(4, "little")
    uc.mem_write(out_ptr, buf)
    set_ret(uc, 1)


def _h_kbd_battery_get(emu: Emu, uc: Uc) -> None:
    (out_ptr,) = get_args(uc, 1)
    uc.mem_write(out_ptr, emu._pack_battery())


def _h_kbd_battery_set_callback(emu: Emu, uc: Uc) -> None:
    (cb,) = get_args(uc, 1)
    emu.battery_cb = cb


def _h_kbd_status_get(emu: Emu, uc: Uc) -> None:
    (out_ptr,) = get_args(uc, 1)
    uc.mem_write(out_ptr, emu._pack_status())


def _h_kbd_status_set_callback(emu: Emu, uc: Uc) -> None:
    (cb,) = get_args(uc, 1)
    emu.status_cb = cb


def _h_gui_pause(emu: Emu, uc: Uc) -> None: pass
def _h_lcd_te_sync_disable(emu: Emu, uc: Uc) -> None: pass
def _h_lcd_is_idle(emu: Emu, uc: Uc) -> None: set_ret(uc, 1)
def _h_lcd_sleep(emu: Emu, uc: Uc) -> None: pass
def _h_lcd_wake(emu: Emu, uc: Uc) -> None: pass


def _h_lcd_rtc_get_live(emu: Emu, uc: Uc) -> None:
    (out_ptr,) = get_args(uc, 1)
    now = datetime.datetime.now()
    buf = (now.year.to_bytes(2, "little")
           + bytes([now.month, now.day, now.hour, now.minute, now.second, 0]))
    uc.mem_write(out_ptr, buf)


def _h_sdk_get_fw_override(emu: Emu, uc: Uc) -> None:
    set_ret(uc, emu.fw_override)


def _h_sdk_set_fw_override(emu: Emu, uc: Uc) -> None:
    (enabled,) = get_args(uc, 1)
    emu.fw_override = 1 if enabled else 0


def _h_m_draw_frame(emu: Emu, uc: Uc) -> None:
    (fb_ptr,) = get_args(uc, 1)
    n = LCD_W * LCD_H * 2
    emu.last_frame = bytes(uc.mem_read(fb_ptr, n))
    emu.frame_ready = True


SDK_HANDLERS: Dict[str, Callable[[Emu, Uc], None]] = {
    "fr_micros":                _h_fr_micros,
    "fr_millis":                _h_fr_millis,
    "srand":                    _h_srand,
    "rand":                     _h_rand,
    "ke_malloc":                _h_ke_malloc,
    "ke_free":                  _h_ke_free,
    "kbd_event_pop":            _h_kbd_event_pop,
    "kbd_battery_get":          _h_kbd_battery_get,
    "kbd_battery_set_callback": _h_kbd_battery_set_callback,
    "kbd_status_get":           _h_kbd_status_get,
    "kbd_status_set_callback":  _h_kbd_status_set_callback,
    "gui_pause":                _h_gui_pause,
    "lcd_te_sync_disable":      _h_lcd_te_sync_disable,
    "lcd_is_idle":              _h_lcd_is_idle,
    "lcd_sleep":                _h_lcd_sleep,
    "lcd_wake":                 _h_lcd_wake,
    "m_draw_frame":             _h_m_draw_frame,
    "sdk_get_fw_override":      _h_sdk_get_fw_override,
    "sdk_set_fw_override":      _h_sdk_set_fw_override,
    "lcd_rtc_get_live":         _h_lcd_rtc_get_live,
}


# ── Frontend entrypoint ───────────────────────────────────────────────


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("elf", type=Path, help="path to meletricks_emu.elf")
    parser.add_argument("--scale", type=int, default=3, help="window scale (default 3)")
    parser.add_argument("--max-fps", type=int, default=60, help="frontend frame cap")
    parser.add_argument("--no-window", action="store_true",
                        help="headless: run ticks and dump every 30th frame to PPM")
    parser.add_argument("-v", "--verbose", action="store_true")

    # ── Status defaults (also tweakable live via F1..F8 in the window) ─
    sg = parser.add_argument_group("initial status data")
    sg.add_argument("--battery", type=int, default=85, metavar="PCT",
                    help="battery percent 0..100 (default 85)")
    sg.add_argument("--charging", action="store_true",
                    help="report battery as charging")
    sg.add_argument("--conn", choices=sorted(CONN_NAMES), default="usb",
                    help="connection type (default usb)")
    sg.add_argument("--caps", action="store_true", help="caps-lock on")
    sg.add_argument("--layer", type=int, default=0, metavar="N",
                    help="active keyboard layer 0..9 (default 0)")
    sg.add_argument("--os", choices=sorted(OS_NAMES), default="mac",
                    dest="os_mode", help="reported host OS (default mac)")
    sg.add_argument("--winlock", action="store_true",
                    help="report Win-lock active")

    args = parser.parse_args()

    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
    )

    if not args.elf.exists():
        sys.exit(f"{args.elf}: no such file (run `make emu` first)")

    emu = Emu(args.elf)

    # Seed StatusState from CLI before FR_SETUP runs — that way the app's
    # BindCallback() snapshot (which calls *_get inline) sees the user's
    # values immediately, no fire_*_cb roundtrip needed.
    emu.status.battery_pct = max(0, min(100, args.battery))
    emu.status.is_charging = 1 if args.charging else 0
    emu.status.conn = CONN_NAMES[args.conn]
    emu.status.caps = 1 if args.caps else 0
    emu.status.layer = max(0, min(9, args.layer))
    emu.status.os = OS_NAMES[args.os_mode]
    emu.status.winlock = 1 if args.winlock else 0

    emu.run_setup()
    log.info("FR_SETUP done")

    # ui.py sits next to emu.py.  When invoked as a script, that dir is
    # already on sys.path so absolute imports work.
    if args.no_window:
        from ui import run_headless
        return run_headless(emu, args)

    from ui import run_window
    return run_window(emu, args)


if __name__ == "__main__":
    sys.exit(main())
