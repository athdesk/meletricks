"""TweakLoader — push an ARM Thumb ELF onto an fr8000-based BLE keyboard.

Two front-ends share the same BLE / ELF / patching core:

  GUI (default — no subcommand):
      python3 TweakLoader/tweakloader.py
      python3 TweakLoader/tweakloader.py path/to/meletricks.elf   # pre-fill ELF

  CLI (development / scripted from `make upload`):
      python3 TweakLoader/tweakloader.py scan [--name SUBSTR]
      python3 TweakLoader/tweakloader.py upload ELF --device AA:BB:CC:...
      python3 TweakLoader/tweakloader.py upload ELF --name SUBSTR

The Makefile wraps the upload form as
`make upload [DEVICE_FILTER="..."] [DEVICE=AA:BB:...]`.
"""

from __future__ import annotations

import argparse
import asyncio
import datetime
import os
import queue
import struct
import sys
import threading
from typing import Callable

import capstone
import capstone.arm
from bleak import BleakClient, BleakScanner
from elftools.elf.elffile import ELFFile
from keystone import KS_ARCH_ARM, KS_MODE_THUMB, Ks

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
try:
    import reloc as _reloc
except ImportError:
    _reloc = None



try:
    import tkinter as tk
    from tkinter import filedialog
    _HAS_TK = True
except ImportError:
    tk = None
    filedialog = None
    _HAS_TK = False

CHAR_W = "02f00000-0000-0000-0000-00000000ff01"
CHAR_R = "02f00000-0000-0000-0000-00000000ff00"
WRITE_CHUNK_SIZE = 560
WRITE_CHUNK_THRESHOLD = 590
READ_MIN = 16

def _build_read_cmd(addr: int, length: int) -> bytes:
    return b"\x08\x00\x06" + struct.pack("I", addr) + struct.pack("H", length)

def _build_write_cmd(addr: int, data: bytes) -> bytes:
    assert len(data) < WRITE_CHUNK_THRESHOLD
    n = len(data)
    return (
        b"\x07"
        + struct.pack("H", n + 6)
        + struct.pack("I", addr)
        + struct.pack("H", n)
        + data
    )

async def _ble_write(client: BleakClient, addr: int, data: bytes) -> None:
    if len(data) < WRITE_CHUNK_THRESHOLD:
        await client.write_gatt_char(CHAR_W, _build_write_cmd(addr, data))
    else:
        for i in range(0, len(data), WRITE_CHUNK_SIZE):
            chunk = data[i : i + WRITE_CHUNK_SIZE]
            await client.write_gatt_char(CHAR_W, _build_write_cmd(addr + i, chunk))

async def _ble_read(client: BleakClient, addr: int, length: int) -> bytes:
    over = max(length, READ_MIN)
    await client.write_gatt_char(CHAR_W, _build_read_cmd(addr, over))
    return bytes(await client.read_gatt_char(CHAR_R))[:length]

_TRAMPOLINE_ADDR = 0x11008A60
_TRAMPOLINE_MAX = 0x11009000 - _TRAMPOLINE_ADDR

_SENTINEL_ADDR = 0x11009000 - 4
_SENTINEL_VALUE = 0xB5B5B5B5

_FIXED_LOAD_BASE = 0x220E0000

_CODE_REGIONS = (
    (0x10000000, 0x11100000),
    (0x220B0000, 0x22200000),
)
_IGNORED_SECTIONS = {
    ".ARM.attributes",
    ".note.gnu.build-id",
    ".comment",
    ".fr_user_setup_marker",
}
_SHF_ALLOC = 0x2

_ks = Ks(KS_ARCH_ARM, KS_MODE_THUMB)

def _asm(src: str, addr: int = 0) -> bytes:
    encoding, _ = _ks.asm(src, addr=addr)
    return bytes(encoding)

def _valid_code_addr(addr: int) -> bool:
    addr &= ~1
    return any(lo <= addr < hi for lo, hi in _CODE_REGIONS)

class _ElfSection:
    __slots__ = ("name", "addr", "size", "data")

    def __init__(self, name: str, addr: int, size: int, data: bytes | None) -> None:
        self.name = name
        self.addr = addr
        self.size = size
        self.data = data

class _ElfSections:
    def __init__(self, path: str, load_base: int | None = None) -> None:
        self.path = path
        with open(path, "rb") as f:
            elf = ELFFile(f)
            raw_entry = elf.header.e_entry
            raw_secs: list[_ElfSection] = []
            sec_map: dict[int, _ElfSection] = {}

            for i, s in enumerate(elf.iter_sections()):
                if not (s["sh_flags"] & _SHF_ALLOC) or s["sh_size"] == 0:
                    continue
                if s.name in _IGNORED_SECTIONS:
                    continue
                data = None if s["sh_type"] == "SHT_NOBITS" else s.data()
                obj = _ElfSection(s.name, s["sh_addr"], s["sh_size"], data)
                raw_secs.append(obj)
                sec_map[i] = obj

            if not raw_secs:
                raise ValueError("ELF has no ALLOC sections")

            link_base = min(s.addr for s in raw_secs)
            by_idx, by_name, by_shndx, _ = _read_symtab(elf)

            self.symbols = by_name

            if load_base is None:
                self.entry = raw_entry
                self.sections = raw_secs
                self.load_base = link_base
            else:
                if _reloc is None:
                    raise RuntimeError(
                        "reloc.py is missing — cannot relocate this ELF.\n"
                        "Place reloc.py next to tweakloader.py."
                    )
                delta = load_base - link_base
                loaded_shndx = set(sec_map.keys())
                relocated_idx = {i for i, sh in by_shndx.items() if sh in loaded_shndx}
                for i, sec in sec_map.items():
                    if sec.data is None:
                        continue
                    relocs = list(_collect_relocs(elf, i))
                    if relocs:
                        sec.data = _reloc.relocate_section(
                            sec.data, sec.addr, relocs, by_idx, delta, relocated_idx
                        )
                for s in raw_secs:
                    s.addr += delta
                self.entry = raw_entry + delta
                self.sections = raw_secs
                self.load_base = load_base

    def span(self) -> tuple[int, int]:
        lo = min(s.addr for s in self.sections)
        hi = max(s.addr + s.size for s in self.sections)
        return lo, hi

def _read_symtab(elf):
    by_idx: dict[int, int] = {}
    by_name: dict[str, int] = {}
    by_shndx: dict[int, object] = {}
    fn_sizes: dict[str, int] = {}
    symtab = elf.get_section_by_name(".symtab")
    if symtab is None:
        return by_idx, by_name, by_shndx, fn_sizes
    for i, sym in enumerate(symtab.iter_symbols()):
        by_idx[i] = sym["st_value"]
        by_shndx[i] = sym["st_shndx"]
        name = sym.name
        if not name or name.startswith("$") or sym["st_shndx"] == 0:
            continue
        stype = sym["st_info"]["type"]
        if stype == "STT_FILE":
            continue
        val = sym["st_value"]
        if stype == "STT_FUNC":
            val &= ~1
            if sym["st_size"] > 0:
                fn_sizes[name] = sym["st_size"]
        by_name[name] = val
    return by_idx, by_name, by_shndx, fn_sizes

def _collect_relocs(elf, target_idx: int):
    sec = elf.get_section(target_idx)
    base = sec["sh_addr"] if elf.header.e_type in ("ET_EXEC", "ET_DYN") else 0
    for s in elf.iter_sections():
        if s["sh_type"] not in ("SHT_REL", "SHT_RELA") or s["sh_info"] != target_idx:
            continue
        for rel in s.iter_relocations():
            info = rel["r_info"]
            yield rel["r_offset"] - base, info & 0xFF, info >> 8

def _elf_has_relocs(path: str) -> bool:
    try:
        with open(path, "rb") as f:
            for s in ELFFile(f).iter_sections():
                if s["sh_type"] in ("SHT_REL", "SHT_RELA"):
                    return True
    except Exception:
        pass
    return False

def _pic_ok(insn) -> bool:
    mn = insn.mnemonic.lower()
    if mn in ("adr", "adrl"):
        return False
    if mn in ("tbb", "tbh"):
        return not any(
            op.type == capstone.arm.ARM_OP_MEM
            and op.mem.base == capstone.arm.ARM_REG_PC
            for op in insn.operands
        )
    in_jump = any(
        g in insn.groups for g in (capstone.CS_GRP_JUMP, capstone.CS_GRP_CALL)
    )
    if in_jump and any(op.type == capstone.arm.ARM_OP_IMM for op in insn.operands):
        return False
    for op in insn.operands:
        if (
            op.type == capstone.arm.ARM_OP_MEM
            and op.mem.base == capstone.arm.ARM_REG_PC
        ):
            return False
    for op in insn.operands[1:]:
        if op.type == capstone.arm.ARM_OP_REG and op.reg == capstone.arm.ARM_REG_PC:
            return False
    return True

def _steal_instructions(code: bytes, base: int, min_size: int) -> tuple[bytes, int]:
    cs = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
    cs.detail = False
    total = 0
    for insn in cs.disasm(code, base):
        total += insn.size
        if total >= min_size:
            return code[:total], total
    raise ValueError(f"Cannot decode {min_size}+ bytes at {base:#x}")

def _validate_pic(code: bytes, base: int) -> list[tuple[int, str]]:
    cs = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
    cs.detail = True
    return [
        (i.address, f"{i.mnemonic} {i.op_str}")
        for i in cs.disasm(code, base)
        if not _pic_ok(i)
    ]

def _make_patch(patch_addr: int, target_addr: int) -> bytes:
    off = target_addr - (patch_addr + 4)
    if -0x1000000 <= off <= 0xFFFFFE:
        return _asm(f"b.w #{target_addr:#x}", addr=patch_addr)
    t = target_addr | 1
    return _asm(
        f"movw ip, #{t & 0xFFFF:#x}\nmovt ip, #{(t >> 16) & 0xFFFF:#x}\nbx ip\n",
        addr=patch_addr,
    )

def _build_trampoline(
    tramp_addr: int,
    entry_addr: int,
    stolen: bytes,
    return_addr: int,
) -> bytes:
    """push → call entry → pop → stolen instructions → branch back."""
    et = entry_addr | 1
    rt = return_addr | 1
    head = _asm(
        "push {r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12,lr}\n"
        f"movw r12, #{et & 0xFFFF:#x}\n"
        f"movt r12, #{(et >> 16) & 0xFFFF:#x}\n"
        "blx  r12\n"
        "pop  {r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12,lr}\n",
        addr=tramp_addr,
    )
    tail_addr = tramp_addr + len(head) + len(stolen)
    tail = _asm(
        f"movw r12, #{rt & 0xFFFF:#x}\nmovt r12, #{(rt >> 16) & 0xFFFF:#x}\nbx   r12\n",
        addr=tail_addr,
    )
    return head + stolen + tail

def _build_zero_shellcode(
    stolen: bytes,
    return_addr: int,
    bss_ranges: list[tuple[int, int]],
) -> bytes:
    """Assemble a one-shot BSS zeroing stub that lives in the trampoline cave.

    Layout (at _TRAMPOLINE_ADDR):
        push  {r0-r3, r12, lr}
        [for each (addr, size):
            movw/movt r0, addr
            movs  r1, #0
            movw/movt r2, aligned_size
        loop_N:
            str   r1, [r0], #4
            subs  r2, r2, #4
            bgt   loop_N
        ]
        movw/movt r0, SENTINEL_ADDR
        movw/movt r1, SENTINEL_VALUE
        str   r1, [r0]
        pop   {r0-r3, r12, lr}
        <stolen instructions>
        movw/movt/bx tail (return to firmware)

    The stolen instructions + tail mean _recover_stolen() can identify this
    shellcode the same way it identifies the regular trampoline — useful if the
    host needs to extract the stolen bytes from an already-patched device.
    """
    rt = return_addr | 1

    lines = ["push {r0, r1, r2, r3, r12, lr}"]

    for i, (addr, size) in enumerate(bss_ranges):
        size = (size + 3) & ~3
        if size == 0:
            continue
        lines += [
            f"movw r0, #{addr & 0xFFFF:#x}",
            f"movt r0, #{(addr >> 16) & 0xFFFF:#x}",
            f"movs r1, #0",
            f"movw r2, #{size & 0xFFFF:#x}",
            f"movt r2, #{(size >> 16) & 0xFFFF:#x}",
            f"zloop_{i}:",
            f"str r1, [r0], #4",
            f"subs r2, r2, #4",
            f"bgt zloop_{i}",
        ]

    lines += [
        f"movw r0, #{_SENTINEL_ADDR & 0xFFFF:#x}",
        f"movt r0, #{(_SENTINEL_ADDR >> 16) & 0xFFFF:#x}",
        f"movw r1, #{_SENTINEL_VALUE & 0xFFFF:#x}",
        f"movt r1, #{(_SENTINEL_VALUE >> 16) & 0xFFFF:#x}",
        f"str r1, [r0]",
        f"pop {{r0, r1, r2, r3, r12, lr}}",
    ]

    head = _asm("\n".join(lines), addr=_TRAMPOLINE_ADDR)

    tail_addr = _TRAMPOLINE_ADDR + len(head) + len(stolen)
    tail = _asm(
        f"movw r12, #{rt & 0xFFFF:#x}\nmovt r12, #{(rt >> 16) & 0xFFFF:#x}\nbx   r12\n",
        addr=tail_addr,
    )
    result = head + stolen + tail
    if len(result) >= _SENTINEL_ADDR - _TRAMPOLINE_ADDR:
        raise RuntimeError(
            f"Zero shellcode ({len(result)} B) would reach the sentinel word — "
            f"too many BSS ranges ({len(bss_ranges)})."
        )
    return result

def _recover_stolen(tramp_bytes: bytes, tramp_addr: int) -> bytes | None:
    """Find the stolen instructions between our stub's pop.w and the MOVW tail.

    Works for both the regular trampoline and the zeroing shellcode since both
    end with: pop.w → <stolen> → movw r12 / movt r12 / bx r12.
    """
    cs = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
    cs.detail = False
    seen_pop = False
    stolen_start = None
    for insn in cs.disasm(tramp_bytes, tramp_addr):
        mn = insn.mnemonic.lower()
        if not seen_pop:
            if mn == "pop.w" or (mn == "pop" and insn.size == 4):
                seen_pop = True
                stolen_start = insn.address + insn.size - tramp_addr
        elif mn == "movw":
            return tramp_bytes[stolen_start : insn.address - tramp_addr]
    return None

def _pack_rtc(dt: datetime.datetime) -> int:
    return (
        (((dt.year - 2000) & 0x3F) << 26)
        | ((dt.month  & 0x0F) << 22)
        | ((dt.day    & 0x1F) << 17)
        | ((dt.hour   & 0x1F) << 12)
        | ((dt.minute & 0x3F) <<  6)
        |  (dt.second & 0x3F)
    )


def _build_rtc_set_shellcode(
    rtc_set_addr: int,
    packed: int,
    stolen: bytes,
    return_addr: int,
) -> bytes:
    rt = return_addr  | 1
    rs = rtc_set_addr | 1
    head = _asm(
        "push {r0, r1, r2, r3, r12, lr}\n"
        f"movw r0,  #{packed & 0xFFFF:#x}\n"
        f"movt r0,  #{(packed >> 16) & 0xFFFF:#x}\n"
        f"movw r12, #{rs & 0xFFFF:#x}\n"
        f"movt r12, #{(rs >> 16) & 0xFFFF:#x}\n"
        "blx r12\n"
        f"movw r0,  #{_SENTINEL_ADDR & 0xFFFF:#x}\n"
        f"movt r0,  #{(_SENTINEL_ADDR >> 16) & 0xFFFF:#x}\n"
        f"movw r1,  #{_SENTINEL_VALUE & 0xFFFF:#x}\n"
        f"movt r1,  #{(_SENTINEL_VALUE >> 16) & 0xFFFF:#x}\n"
        "str r1, [r0]\n"
        "pop {r0, r1, r2, r3, r12, lr}\n",
        addr=_TRAMPOLINE_ADDR,
    )
    tail_addr = _TRAMPOLINE_ADDR + len(head) + len(stolen)
    tail = _asm(
        f"movw r12, #{rt & 0xFFFF:#x}\n"
        f"movt r12, #{(rt >> 16) & 0xFFFF:#x}\n"
        "bx r12\n",
        addr=tail_addr,
    )
    result = head + stolen + tail
    if len(result) >= _SENTINEL_ADDR - _TRAMPOLINE_ADDR:
        raise RuntimeError(
            f"RTC-set shellcode ({len(result)} B) reaches the sentinel word."
        )
    return result


async def _set_device_rtc(
    client: BleakClient,
    rtc_set_addr: int,
    stolen: bytes,
    return_addr: int,
    tick_addr: int,
    patch: bytes,
    already_patched: bool,
    log: Callable[[str], None],
) -> bool:
    now = datetime.datetime.now()
    packed = _pack_rtc(now)
    log(f"  Setting device RTC to {now.strftime('%Y-%m-%d %H:%M:%S')}")
    log(f"    fw_rtc_set @ 0x{rtc_set_addr:08x}, packed = 0x{packed:08x}")

    sc = _build_rtc_set_shellcode(rtc_set_addr, packed, stolen, return_addr)
    await _ble_write(client, _SENTINEL_ADDR, struct.pack("<I", 0))
    await _ble_write(client, _TRAMPOLINE_ADDR, sc)
    if not already_patched:
        await _ble_write(client, tick_addr, patch)

    loop = asyncio.get_event_loop()
    deadline = loop.time() + 5.0
    while True:
        raw = await _ble_read(client, _SENTINEL_ADDR, 4)
        if struct.unpack("<I", bytes(raw[:4]))[0] == _SENTINEL_VALUE:
            break
        if loop.time() > deadline:
            log("  Warning: RTC set timed out; clock may not be applied.")
            return False
        await asyncio.sleep(0.05)
    log("  RTC applied.")

    await _ble_write(client, tick_addr, stolen)
    return True


async def _upload_bytes(
    client: BleakClient,
    data: bytes,
    addr: int,
    log: Callable[[str], None] | None = None,
) -> None:
    total = len(data)
    last_bucket = -1
    for i in range(0, total, WRITE_CHUNK_SIZE):
        chunk = data[i : i + WRITE_CHUNK_SIZE]
        await _ble_write(client, addr + i, chunk)
        if log:
            pct = int((i + len(chunk)) * 100 / total)
            bucket = pct // 25
            if bucket != last_bucket:
                last_bucket = bucket
                log(f"    {pct}%...")

async def scan_devices() -> list[tuple[str, str, int]]:
    """Return all visible BLE devices as [(name, address, rssi), ...].

    Named devices come first, sorted by signal strength descending.
    Nameless devices follow, also sorted by signal strength.
    """
    results = await BleakScanner.discover(return_adv=True)
    out = []
    for _addr, (device, adv) in results.items():
        name = (device.name or "").strip()
        rssi = adv.rssi if adv.rssi is not None else -999
        out.append((name or "", device.address, rssi))

    out.sort(key=lambda x: (x[0] == "", -x[2]))
    return [(name or "(no name)", addr, rssi) for name, addr, rssi in out]

async def load_elf_to_device(
    device_address: str,
    elf_path: str,
    log: Callable[[str], None],
    tick_addr: int | None = None,
) -> bool:
    """Full sequence: connect → upload ELF → zero BSS → install tick hook.

    If `tick_addr` is None, the patch site is discovered from the ELF's
    `fw_tick_hook` symbol (defined per-board via FW_TICK_HOOK in board.h).
    """
    log(f"Connecting to {device_address}...")
    client = BleakClient(device_address)
    try:
        await client.connect()
        await asyncio.sleep(0.5)
        log("Connected.")

        log(f"  Parsing {os.path.basename(elf_path)}...")
        if _elf_has_relocs(elf_path) and _reloc is not None:
            blob = _ElfSections(elf_path)
            lo, _ = blob.span()
            delta = _FIXED_LOAD_BASE - lo
            sign = "-" if delta < 0 else "+"
            log(
                f"  Relocating: link 0x{lo:08x} → 0x{_FIXED_LOAD_BASE:08x} "
                f"(delta {sign}0x{abs(delta):x})"
            )
            blob = _ElfSections(elf_path, load_base=_FIXED_LOAD_BASE)
        else:
            blob = _ElfSections(elf_path)

        if tick_addr is None:
            tick_addr = blob.symbols.get("fw_tick_hook")
            if tick_addr is None:
                raise RuntimeError(
                    "ELF has no 'fw_tick_hook' symbol — define FW_TICK_HOOK "
                    "in the board header, or pass --hook to override."
                )
            log(f"  Tick hook @ 0x{tick_addr:08x} (auto-detected from ELF)")
        else:
            log(f"  Tick hook @ 0x{tick_addr:08x} (override)")

        entry = blob.entry & ~1
        if not _valid_code_addr(entry):
            raise ValueError(
                f"ELF entry 0x{entry:08x} is not in a known code region.\n"
                f"Check that the ELF is built for this target."
            )

        patch = _make_patch(tick_addr, _TRAMPOLINE_ADDR)
        site_bytes = await _ble_read(client, tick_addr, max(len(patch) + 12, 16))
        already_patched = site_bytes[: len(patch)] == patch

        if already_patched:
            tramp_bytes = await _ble_read(client, _TRAMPOLINE_ADDR, 256)
            stolen = _recover_stolen(bytes(tramp_bytes), _TRAMPOLINE_ADDR)
            if stolen is None:
                raise RuntimeError(
                    "The firmware tick is already hooked by something else.\n"
                    "Power-cycle the device and try again."
                )
            log("  (Re-using existing hook, updating trampoline)")
        else:
            stolen, _ = _steal_instructions(bytes(site_bytes), tick_addr, len(patch))
            bad = _validate_pic(stolen, tick_addr)
            if bad:
                raise RuntimeError(
                    f"Stolen instruction is not position-independent: {bad[0][1]}"
                )

        return_addr = tick_addr + len(stolen)

        bss_ranges: list[tuple[int, int]] = []
        for s in sorted(blob.sections, key=lambda x: x.addr):
            if s.data is None:
                bss_ranges.append((s.addr, s.size))
            else:
                log(f"  Uploading {s.name:<10}  {s.size} B @ 0x{s.addr:08x}")
                await _upload_bytes(client, s.data, s.addr, log)

        # RTC set must come AFTER section upload — the trampoline does a
        # BLX into the user's `fw_rtc_set` wrapper in PSRAM, so the body
        # has to be live by then. DYNA appeared to tolerate the old order
        # only because its uninitialized PSRAM contained long runs of
        # 0xAA bytes (Thumb-16 `ADD R2, SP, #imm8`, a harmless walk) so
        # the call silently timed out without faulting — the RTC wasn't
        # actually being set in the DYNA "fresh-upload" case either, the
        # timeout was just hidden in the log. On Tiga, PSRAM defaults are
        # random and one of those random halfwords faults → hard reset.
        rtc_set_addr = blob.symbols.get("fw_rtc_set")
        if rtc_set_addr:
            await _set_device_rtc(client, rtc_set_addr, stolen, return_addr,
                                  tick_addr, patch, already_patched, log)
            already_patched = False

        if bss_ranges:
            total_bss = sum(s for _, s in bss_ranges)
            log(
                f"  Zeroing BSS on device ({total_bss} B across "
                f"{len(bss_ranges)} section(s))..."
            )

            await _ble_write(client, _SENTINEL_ADDR, struct.pack("<I", 0))

            zero_sc = _build_zero_shellcode(stolen, return_addr, bss_ranges)
            await _ble_write(client, _TRAMPOLINE_ADDR, zero_sc)

            if not already_patched:
                await _ble_write(client, tick_addr, patch)
                already_patched = True

            log("  Waiting for device to finish zeroing...")
            loop = asyncio.get_event_loop()
            deadline = loop.time() + 15.0
            while True:
                raw = await _ble_read(client, _SENTINEL_ADDR, 4)
                if struct.unpack("<I", bytes(raw[:4]))[0] == _SENTINEL_VALUE:
                    break
                if loop.time() > deadline:
                    log("  Warning: zeroing timed out; BSS may not be clean.")
                    break
                await asyncio.sleep(0.05)

            log("  BSS zeroed on device.")

        log(f"  Installing call stub → entry @ 0x{entry:08x}...")
        trampoline = _build_trampoline(_TRAMPOLINE_ADDR, entry, stolen, return_addr)
        await _ble_write(client, _TRAMPOLINE_ADDR, trampoline)

        if not already_patched:
            await _ble_write(client, tick_addr, patch)

        log("")
        log("Done!  Your ELF entry function runs every firmware tick.")
        return True

    except Exception as e:
        log(f"\nError: {e}")
        return False
    finally:
        try:
            await client.disconnect()
        except Exception:
            pass



def _log(msg: str) -> None:
    print(msg, flush=True)

def _filter_devices(
    devices: list[tuple[str, str, int]], name_filter: str | None
) -> list[tuple[str, str, int]]:
    if not name_filter:
        return devices
    needle = name_filter.casefold()
    return [d for d in devices if needle in d[0].casefold()]

def _print_devices(devices: list[tuple[str, str, int]]) -> None:
    for i, (name, addr, rssi) in enumerate(devices):
        _log(f"  [{i}] {name:<32}  {addr}  {rssi:>4} dBm")

def _prompt_choice(devices: list[tuple[str, str, int]]) -> str:
    """Interactive picker — returns the chosen MAC.  Stdin must be a tty."""
    if not sys.stdin.isatty():
        raise RuntimeError(
            "Multiple devices matched and stdin is not a tty.  "
            "Re-run with --device ADDR or a more specific --name."
        )
    while True:
        raw = input(f"Select device [0..{len(devices) - 1}] (or 'q' to quit): ").strip()
        if raw.lower() in ("q", "quit", "exit"):
            raise KeyboardInterrupt
        if raw.isdigit():
            idx = int(raw)
            if 0 <= idx < len(devices):
                return devices[idx][1]
        _log("  invalid choice, try again")

async def _pick_device(name_filter: str | None) -> str:
    _log(f"Scanning for BLE devices{f' matching {name_filter!r}' if name_filter else ''}...")
    all_devices = await scan_devices()
    if not all_devices:
        raise RuntimeError("No BLE devices found.  Make sure Bluetooth is enabled.")
    matches = _filter_devices(all_devices, name_filter)
    if not matches:
        _log("Filter matched nothing.  All visible devices:")
        _print_devices(all_devices)
        raise RuntimeError(f"No devices match name filter {name_filter!r}.")
    if len(matches) == 1:
        name, addr, rssi = matches[0]
        _log(f"Found single match: {name} [{addr}] {rssi} dBm")
        return addr
    _log(f"Multiple matches ({len(matches)}):")
    _print_devices(matches)
    return _prompt_choice(matches)

async def _cmd_scan(args: argparse.Namespace) -> int:
    devices = await scan_devices()
    if args.name:
        devices = _filter_devices(devices, args.name)
    if not devices:
        _log("No devices found.")
        return 1
    _print_devices(devices)
    return 0

async def _cmd_upload(args: argparse.Namespace) -> int:
    if not os.path.isfile(args.elf):
        _log(f"error: {args.elf}: no such file")
        return 2

    tick_addr: int | None = None
    if args.hook is not None:
        try:
            tick_addr = int(args.hook, 16)
        except ValueError:
            _log(f"error: invalid --hook value: {args.hook!r}")
            return 2

    if args.device:
        addr = args.device
    else:
        addr = await _pick_device(args.name)

    _log(f"--- Loading {os.path.basename(args.elf)} ---")
    ok = await load_elf_to_device(addr, args.elf, _log, tick_addr=tick_addr)
    return 0 if ok else 1

def _cli_main(argv: list[str]) -> int:
    p = argparse.ArgumentParser(
        prog="tweakloader",
        description="Upload an ARM Thumb ELF to an fr8000-based BLE keyboard.",
    )
    sub = p.add_subparsers(dest="cmd", required=True)

    scan_p = sub.add_parser("scan", help="List visible BLE devices.")
    scan_p.add_argument("--name", help="Case-insensitive substring filter.")
    scan_p.set_defaults(func=_cmd_scan)

    up_p = sub.add_parser("upload", help="Upload an ELF and install the tick hook.")
    up_p.add_argument("elf", help="path to the ELF file to upload")
    up_p.add_argument(
        "--device",
        help="explicit BLE address (skips scan); overrides --name",
    )
    up_p.add_argument(
        "--name",
        help="case-insensitive substring filter for the device name.  "
             "If exactly one device matches, it is used automatically; "
             "otherwise the user is prompted to pick one.",
    )
    up_p.add_argument(
        "--hook",
        default=None,
        help="firmware tick function address (hex).  By default this is "
             "read from the ELF's `fw_tick_hook` symbol, which is set per "
             "board via FW_TICK_HOOK in sdk/boards/<board>/board.h.",
    )
    up_p.set_defaults(func=_cmd_upload)

    args = p.parse_args(argv)
    try:
        return asyncio.run(args.func(args))
    except KeyboardInterrupt:
        _log("\nAborted.")
        return 130
    except Exception as e:
        _log(f"error: {e}")
        return 1



class _App:
    def __init__(self, root) -> None:
        self.root = root
        self.root.title("TweakLoader")
        self.root.minsize(500, 520)
        self.root.resizable(True, True)

        self._elf_var = tk.StringVar()
        self._hook_addr_var = tk.StringVar(value="")
        self._devices: list[tuple[str, str, int]] = []
        self._log_queue: queue.Queue[str] = queue.Queue()
        self._busy = False

        self._build_ui()
        self._poll_log()

    def _build_ui(self) -> None:
        PAD = {"padx": 10, "pady": 5}

        tk.Label(
            self.root,
            text="TweakLoader",
            font=("Segoe UI", 13, "bold"),
        ).pack(**PAD, anchor="w")

        elf_frame = tk.LabelFrame(self.root, text="ELF File", padx=8, pady=4)
        elf_frame.pack(fill="x", **PAD)
        row = tk.Frame(elf_frame)
        row.pack(fill="x")
        tk.Entry(
            row, textvariable=self._elf_var, state="readonly", font=("Consolas", 9)
        ).pack(side="left", fill="x", expand=True)
        tk.Button(row, text="Browse…", command=self._browse_elf).pack(
            side="left", padx=(6, 0)
        )

        dev_frame = tk.LabelFrame(self.root, text="Bluetooth Device", padx=8, pady=4)
        dev_frame.pack(fill="x", **PAD)
        list_row = tk.Frame(dev_frame)
        list_row.pack(fill="x")

        self._listbox = tk.Listbox(
            list_row,
            height=6,
            selectmode=tk.SINGLE,
            font=("Consolas", 9),
            exportselection=False,
        )
        sb = tk.Scrollbar(list_row, command=self._listbox.yview)
        self._listbox.config(yscrollcommand=sb.set)
        self._listbox.pack(side="left", fill="both", expand=True)
        sb.pack(side="left", fill="y")

        self._scan_btn = tk.Button(
            list_row, text="Scan", width=9, command=self._start_scan
        )
        self._scan_btn.pack(side="left", padx=(8, 0), anchor="n")
        self._listbox.bind("<<ListboxSelect>>", lambda _: self._refresh_load_btn())

        hook_frame = tk.LabelFrame(self.root, text="Tick Hook Address", padx=8, pady=4)
        hook_frame.pack(fill="x", **PAD)
        tk.Entry(
            hook_frame, textvariable=self._hook_addr_var, font=("Consolas", 9), width=20
        ).pack(side="left")
        tk.Label(
            hook_frame,
            text="  leave empty to auto-detect from ELF (fw_tick_hook symbol)",
            font=("Segoe UI", 8),
            fg="#666666",
        ).pack(side="left")

        self._load_btn = tk.Button(
            self.root,
            text="Load & Install",
            font=("Segoe UI", 11, "bold"),
            command=self._start_load,
            state="disabled",
            bg="#0078D7",
            fg="white",
            activebackground="#005fa3",
            activeforeground="white",
            relief="flat",
            padx=6,
            pady=6,
        )
        self._load_btn.pack(fill="x", padx=10, pady=6)

        log_frame = tk.LabelFrame(self.root, text="Log", padx=6, pady=4)
        log_frame.pack(fill="both", expand=True, padx=10, pady=(0, 8))
        self._log_text = tk.Text(
            log_frame,
            state="disabled",
            font=("Consolas", 9),
            wrap="word",
            bg="#1e1e1e",
            fg="#d4d4d4",
        )
        log_sb = tk.Scrollbar(log_frame, command=self._log_text.yview)
        self._log_text.config(yscrollcommand=log_sb.set)
        self._log_text.pack(side="left", fill="both", expand=True)
        log_sb.pack(side="left", fill="y")

    def _enqueue(self, msg: str) -> None:
        self._log_queue.put(msg)

    def _poll_log(self) -> None:
        try:
            while True:
                msg = self._log_queue.get_nowait()
                self._log_text.config(state="normal")
                self._log_text.insert("end", msg + "\n")
                self._log_text.see("end")
                self._log_text.config(state="disabled")
        except queue.Empty:
            pass
        self.root.after(50, self._poll_log)

    def _browse_elf(self) -> None:
        path = filedialog.askopenfilename(
            title="Select ELF file",
            filetypes=[("ELF files", "*.elf"), ("All files", "*.*")],
        )
        if path:
            self._elf_var.set(path)
            self._refresh_load_btn()

    def _start_scan(self) -> None:
        if self._busy:
            return
        self._set_busy(True)
        self._listbox.delete(0, "end")
        self._devices = []
        self._enqueue("Scanning for BLE devices...")

        def _run() -> None:
            try:
                loop = asyncio.new_event_loop()
                devices = loop.run_until_complete(scan_devices())
                loop.close()
                self.root.after(0, lambda: self._on_scan_done(devices))
            except Exception as e:
                self.root.after(0, lambda: self._finish(f"Scan failed: {e}"))

        threading.Thread(target=_run, daemon=True).start()

    def _on_scan_done(self, devices: list[tuple[str, str, int]]) -> None:
        self._devices = devices
        if devices:
            for name, addr, rssi in devices:
                self._listbox.insert("end", f"{name}  [{addr}]  {rssi} dBm")
            self._enqueue(
                f"Found {len(devices)} device(s). Select a device and click Load & Install."
            )
        else:
            self._enqueue("No BLE devices found. Make sure Bluetooth is enabled.")
        self._set_busy(False)

    def _start_load(self) -> None:
        sel = self._listbox.curselection()
        elf = self._elf_var.get()
        if not sel or not elf or not os.path.isfile(elf):
            return
        raw_hook = self._hook_addr_var.get().strip()
        tick_addr: int | None
        if raw_hook:
            try:
                tick_addr = int(raw_hook, 16)
            except ValueError:
                self._enqueue(f"Invalid hook address: {raw_hook!r}")
                return
        else:
            tick_addr = None  # auto-detect from ELF
        _, addr, _ = self._devices[sel[0]]
        self._set_busy(True)
        self._enqueue(f"\n--- Loading {os.path.basename(elf)} ---")

        def _run() -> None:
            try:
                loop = asyncio.new_event_loop()
                loop.run_until_complete(
                    load_elf_to_device(addr, elf, self._enqueue, tick_addr=tick_addr)
                )
                loop.close()
            except Exception as e:
                self._enqueue(f"Fatal: {e}")
            finally:
                self.root.after(0, lambda: self._set_busy(False))

        threading.Thread(target=_run, daemon=True).start()

    def _refresh_load_btn(self) -> None:
        ok = (
            bool(self._elf_var.get())
            and bool(self._listbox.curselection())
            and not self._busy
        )
        self._load_btn.config(state="normal" if ok else "disabled")

    def _set_busy(self, busy: bool) -> None:
        self._busy = busy
        self._scan_btn.config(state="disabled" if busy else "normal")
        self._load_btn.config(
            text="Working…" if busy else "Load & Install",
            state="disabled"
            if busy
            else (
                "normal"
                if self._listbox.curselection() and self._elf_var.get()
                else "disabled"
            ),
        )

    def _finish(self, msg: str) -> None:
        self._enqueue(msg)
        self._set_busy(False)

def _launch_gui(elf_arg: str | None) -> int:
    if not _HAS_TK:
        _log("error: tkinter is not available on this Python install.")
        _log("       use the CLI instead:")
        _log("         tweakloader.py scan [--name SUBSTR]")
        _log("         tweakloader.py upload ELF [--device A | --name SUBSTR]")
        return 1
    root = tk.Tk()
    app = _App(root)
    if elf_arg and os.path.isfile(elf_arg):
        app._elf_var.set(elf_arg)
    root.mainloop()
    return 0



_CLI_COMMANDS = {"scan", "upload"}

def main() -> int:
    argv = sys.argv[1:]

    if argv and (argv[0] in _CLI_COMMANDS or argv[0] in ("-h", "--help")):
        return _cli_main(argv)

    return _launch_gui(argv[0] if argv else None)

if __name__ == "__main__":
    sys.exit(main())
