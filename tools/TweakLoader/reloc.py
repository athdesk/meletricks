"""ARM ELF relocation for the FR8000 binary loader.

When an ELF is uploaded to a different base than the one it was linked
at, every absolute address embedded in the code/data has to be patched
to match the new base.  This module performs that patching against the
`.rel.<section>` entries that the GNU linker emits when invoked with
`--emit-relocs`.

# Assumption — uniform delta

We assume the *whole ELF* shifts by a single constant `delta =
load_base - link_base`.  That's true when sections are bump-allocated
contiguously, which is exactly the binary-loader case.  Under that
assumption:

* Absolute relocations (`R_ARM_ABS32` and its MOV-W/MOV-T pair siblings)
  need `delta` added to the immediate value.
* PC-relative relocations (`R_ARM_REL32`, `R_ARM_CALL`,
  `R_ARM_THM_CALL`, `R_ARM_PREL31`, …) are no-ops — both the source
  PC and the target shift by the same delta so the difference is
  preserved.

We refuse any relocation type we don't explicitly recognise, since a
silently-wrong relocation crashes the MCU with no obvious trace.

# Supported types

  R_ARM_NONE              0
  R_ARM_ABS32             2     adds delta to a 32-bit word
  R_ARM_REL32             3     no-op
  R_ARM_THM_CALL          10    no-op
  R_ARM_GOT_BREL          26    refused (no GOT in freestanding blobs)
  R_ARM_CALL              28    no-op
  R_ARM_JUMP24            29    no-op
  R_ARM_THM_JUMP24        30    no-op
  R_ARM_TARGET1           38    same as ABS32 in EABI for our usage
  R_ARM_V4BX              40    no-op (Thumb-interworking veneer)
  R_ARM_TARGET2           41    same as REL32 in EABI for our usage
  R_ARM_PREL31            42    no-op
  R_ARM_MOVW_ABS_NC       43    patches ARM MOVW immediate
  R_ARM_MOVT_ABS          44    patches ARM MOVT immediate
  R_ARM_THM_MOVW_ABS_NC   47    patches Thumb-2 MOVW immediate
  R_ARM_THM_MOVT_ABS      48    patches Thumb-2 MOVT immediate
  R_ARM_THM_JUMP19        51    no-op (Thumb-2 b<cond>.w)
  R_ARM_THM_JUMP6         52    no-op (Thumb cbz / cbnz)
  R_ARM_THM_ALU_PREL_11_0 53    no-op (Thumb-2 add Rd, pc, #imm)
  R_ARM_THM_PC12          54    no-op (Thumb-2 ldr Rd, [pc, #imm])
"""

import struct
from typing import Iterable, Mapping


# ── relocation type IDs ──────────────────────────────────────────────────────

R_ARM_NONE              = 0
R_ARM_ABS32             = 2
R_ARM_REL32             = 3
R_ARM_THM_CALL          = 10
R_ARM_GOT_BREL          = 26
R_ARM_CALL              = 28
R_ARM_JUMP24            = 29
R_ARM_THM_JUMP24        = 30
R_ARM_TARGET1           = 38
R_ARM_V4BX              = 40
R_ARM_TARGET2           = 41
R_ARM_PREL31            = 42
R_ARM_MOVW_ABS_NC       = 43
R_ARM_MOVT_ABS          = 44
R_ARM_THM_MOVW_ABS_NC   = 47
R_ARM_THM_MOVT_ABS      = 48
R_ARM_THM_JUMP19        = 51
R_ARM_THM_JUMP6         = 52
R_ARM_THM_ALU_PREL_11_0 = 53
R_ARM_THM_PC12          = 54


# Types we know shift-with-the-blob — no patching needed.
_PC_RELATIVE = frozenset({
    R_ARM_NONE,
    R_ARM_REL32,
    R_ARM_CALL,
    R_ARM_JUMP24,
    R_ARM_THM_CALL,
    R_ARM_THM_JUMP24,
    R_ARM_PREL31,
    R_ARM_TARGET2,
    R_ARM_V4BX,
    R_ARM_THM_JUMP19,
    R_ARM_THM_JUMP6,
    R_ARM_THM_ALU_PREL_11_0,
    R_ARM_THM_PC12,
})

# Types that take a simple `+= delta` to a 32-bit word.
_ABSOLUTE_WORD = frozenset({
    R_ARM_ABS32,
    R_ARM_TARGET1,
})


# ── helpers ──────────────────────────────────────────────────────────────────


def _read_u32(buf: bytes, off: int) -> int:
    return struct.unpack_from("<I", buf, off)[0]


def _write_u32(buf: bytearray, off: int, val: int) -> None:
    struct.pack_into("<I", buf, off, val & 0xFFFFFFFF)


# ARM MOVW / MOVT (encoding A2):
#
#   31..28  27..20      19..16  15..12  11..0
#   cond    0011 0w00   imm4    Rd      imm12
#
# Combined imm16 = (imm4 << 12) | imm12.  MOVW writes the low 16 bits
# of a 32-bit constant; MOVT writes the high 16 (and the assembler
# pairs them so the runtime address is reconstructed in two halves).

def _arm_movw_movt_get(instr: int) -> int:
    return ((instr >> 4) & 0xF000) | (instr & 0xFFF)


def _arm_movw_movt_set(instr: int, imm16: int) -> int:
    imm16 &= 0xFFFF
    return (instr & ~0x000F_0FFF) | ((imm16 & 0xF000) << 4) | (imm16 & 0xFFF)


# Thumb-2 MOVW / MOVT (encoding T3):
#
#   first  halfword:  1111 0 i 1 0 0 1 0 0  imm4         (bits [10] = i, [3:0] = imm4)
#   second halfword:  0 imm3 Rd imm8                      (bits [14:12] = imm3, [7:0] = imm8)
#
# Combined imm16 = (imm4 << 12) | (i << 11) | (imm3 << 8) | imm8.
# Both halfwords are stored little-endian, so the bytes on disk are
# `hw1_lo hw1_hi hw2_lo hw2_hi`.

def _thumb_movw_movt_get(buf: bytes, off: int) -> int:
    hw1 = struct.unpack_from("<H", buf, off)[0]
    hw2 = struct.unpack_from("<H", buf, off + 2)[0]
    imm4 = hw1 & 0xF
    i    = (hw1 >> 10) & 0x1
    imm3 = (hw2 >> 12) & 0x7
    imm8 = hw2 & 0xFF
    return (imm4 << 12) | (i << 11) | (imm3 << 8) | imm8


def _thumb_movw_movt_set(buf: bytearray, off: int, imm16: int) -> None:
    imm16 &= 0xFFFF
    imm4 = (imm16 >> 12) & 0xF
    i    = (imm16 >> 11) & 0x1
    imm3 = (imm16 >>  8) & 0x7
    imm8 =  imm16        & 0xFF
    hw1 = struct.unpack_from("<H", buf, off)[0]
    hw2 = struct.unpack_from("<H", buf, off + 2)[0]
    hw1 = (hw1 & ~0x040F) | (i << 10) | imm4
    hw2 = (hw2 & ~0x70FF) | (imm3 << 12) | imm8
    struct.pack_into("<H", buf, off, hw1)
    struct.pack_into("<H", buf, off + 2, hw2)


# ── core: relocate a single section ──────────────────────────────────────────


def relocate_section(
    data: bytes,
    section_link_addr: int,
    relocations: "Iterable[tuple[int, int, int]]",
    symbols_orig: "Mapping[int, int]",
    delta: int,
    relocated_sym_indices: "set[int]",
) -> bytes:
    """Apply a section's relocations and return its new bytes.

    Args:
        data: the section's original (linked) bytes.
        section_link_addr: the original linker-time address of the
            section (used only for sanity / error messages).
        relocations: an iterable of (offset, type_id, sym_idx) — the
            entries from `.rel.<section>`.  Offsets are within `data`.
            (REL has no explicit addend; the addend lives in `data`.)
        symbols_orig: index → linker-time symbol value.  Index 0 is
            the undefined-symbol slot and never referenced by a valid
            relocation.
        delta: load_base - link_base.
        relocated_sym_indices: the symbols whose runtime address
            shifted by `delta` — typically those whose `st_shndx`
            references a section we loaded.  ABS32 / MOVW / MOVT
            relocs whose target is NOT in this set keep their
            link-time value (e.g. `SHN_ABS` firmware syscalls like
            `m_draw_frame` must stay at their fixed ROM address).

    Returns the patched bytes.  Raises ValueError if any relocation
    type isn't handled — silent miscompiles are worse than a refusal.
    """
    if delta == 0:
        # Same base — nothing to patch, even for absolute types.
        return data
    out = bytearray(data)
    for off, rtype, sym_idx in relocations:
        if rtype in _PC_RELATIVE:
            continue
        # Only references INTO the relocated blob shift by delta.
        # Anything resolving to a fixed firmware address (SHN_ABS) or
        # an external / debug section stays where it is.
        sym_delta = delta if sym_idx in relocated_sym_indices else 0
        if rtype in _ABSOLUTE_WORD:
            if off + 4 > len(out):
                raise ValueError(
                    f"R_ARM_ABS32 at section 0x{section_link_addr:x}+0x{off:x} "
                    f"runs past section end"
                )
            cur = _read_u32(out, off)
            _write_u32(out, off, cur + sym_delta)
            continue
        if rtype == R_ARM_MOVW_ABS_NC:
            if off + 4 > len(out):
                raise ValueError(
                    f"R_ARM_MOVW_ABS_NC at +0x{off:x} runs past section end"
                )
            instr = _read_u32(out, off)
            sym_val = symbols_orig.get(sym_idx, 0)
            full = (sym_val + sym_delta) & 0xFFFFFFFF
            # MOVW takes the low 16 bits of the resolved value.
            instr = _arm_movw_movt_set(instr, full & 0xFFFF)
            _write_u32(out, off, instr)
            continue
        if rtype == R_ARM_MOVT_ABS:
            if off + 4 > len(out):
                raise ValueError(
                    f"R_ARM_MOVT_ABS at +0x{off:x} runs past section end"
                )
            instr = _read_u32(out, off)
            sym_val = symbols_orig.get(sym_idx, 0)
            full = (sym_val + sym_delta) & 0xFFFFFFFF
            # MOVT takes the high 16 bits.
            instr = _arm_movw_movt_set(instr, (full >> 16) & 0xFFFF)
            _write_u32(out, off, instr)
            continue
        if rtype == R_ARM_THM_MOVW_ABS_NC:
            if off + 4 > len(out):
                raise ValueError(
                    f"R_ARM_THM_MOVW_ABS_NC at +0x{off:x} runs past section end"
                )
            sym_val = symbols_orig.get(sym_idx, 0)
            full = (sym_val + sym_delta) & 0xFFFFFFFF
            _thumb_movw_movt_set(out, off, full & 0xFFFF)
            continue
        if rtype == R_ARM_THM_MOVT_ABS:
            if off + 4 > len(out):
                raise ValueError(
                    f"R_ARM_THM_MOVT_ABS at +0x{off:x} runs past section end"
                )
            sym_val = symbols_orig.get(sym_idx, 0)
            full = (sym_val + sym_delta) & 0xFFFFFFFF
            _thumb_movw_movt_set(out, off, (full >> 16) & 0xFFFF)
            continue
        if rtype == R_ARM_GOT_BREL:
            raise ValueError(
                "R_ARM_GOT_BREL is not supported — build the blob with "
                "-fno-pic / no GOT references"
            )
        raise ValueError(
            f"unsupported ARM relocation type {rtype} at "
            f"section 0x{section_link_addr:x}+0x{off:x}"
        )
    return bytes(out)


# ── ELF wrapper: pull relocations out of a parsed file ───────────────────────


def collect_relocations_for(elf, target_section_name: str):
    """Yield `(offset, type, sym_idx)` for every entry in
    `.rel.<target_section_name>` (or `.rela.<...>`).  Empty if there
    are no relocations for that section."""
    rel_name = ".rel" + target_section_name
    rela_name = ".rela" + target_section_name
    for name in (rel_name, rela_name):
        sec = elf.get_section_by_name(name)
        if sec is None:
            continue
        is_rela = name.startswith(".rela")
        for rel in sec.iter_relocations():
            off = rel["r_offset"]
            info = rel["r_info"]
            sym_idx = info >> 8
            rtype = info & 0xFF
            # RELA carries an explicit addend; we ignore it because in
            # `--emit-relocs` output the addend is also baked into the
            # original section bytes (which is what we patch).  GCC's
            # ARM output is REL anyway, so this is just defensive.
            _ = is_rela
            yield off, rtype, sym_idx


def build_symbol_index(elf) -> "dict[int, int]":
    """Build symbol_index → linker-time value map for `.symtab`.
    Includes the Thumb LSB exactly as the linker recorded it, since
    relocation math expects the same encoding."""
    symtab = elf.get_section_by_name(".symtab")
    if symtab is None:
        return {}
    out: "dict[int, int]" = {}
    for i, sym in enumerate(symtab.iter_symbols()):
        out[i] = sym["st_value"]
    return out
