"""
pygame frontend for the host emulator.

The window shows the 320x172 RGB565 panel scaled up.  Each iteration:
    1. drain pygame events -> push KBD_EV_LCD / KBD_EV_KEY into the SDK queue
    2. run one guest tick (which may call m_draw_frame)
    3. if a new frame was captured, blit it
"""

from __future__ import annotations

import logging
import sys

import numpy as np
import pygame

from emu import CONN_LABEL, LCD_H, LCD_W, OS_LABEL, Emu


log = logging.getLogger("emu.ui")


# KBD_LCD_* values from sdk/include/kbd_event.h:
LCD_DOWN, LCD_UP, LCD_ENTER, LCD_BACK = 1, 2, 3, 4


# USB HID Usage IDs (Keyboard/Keypad page 0x07) for the letters + a few
# punctuation keys.  meletricks.c's handle_input only cares that the
# scancode is non-zero (for WPM ticks) plus passing it to text_log_hid, so
# correctness here mostly matters for the typing-log screen.
_PG_TO_HID = {
    pygame.K_a: 0x04, pygame.K_b: 0x05, pygame.K_c: 0x06, pygame.K_d: 0x07,
    pygame.K_e: 0x08, pygame.K_f: 0x09, pygame.K_g: 0x0A, pygame.K_h: 0x0B,
    pygame.K_i: 0x0C, pygame.K_j: 0x0D, pygame.K_k: 0x0E, pygame.K_l: 0x0F,
    pygame.K_m: 0x10, pygame.K_n: 0x11, pygame.K_o: 0x12, pygame.K_p: 0x13,
    pygame.K_q: 0x14, pygame.K_r: 0x15, pygame.K_s: 0x16, pygame.K_t: 0x17,
    pygame.K_u: 0x18, pygame.K_v: 0x19, pygame.K_w: 0x1A, pygame.K_x: 0x1B,
    pygame.K_y: 0x1C, pygame.K_z: 0x1D,
    pygame.K_1: 0x1E, pygame.K_2: 0x1F, pygame.K_3: 0x20, pygame.K_4: 0x21,
    pygame.K_5: 0x22, pygame.K_6: 0x23, pygame.K_7: 0x24, pygame.K_8: 0x25,
    pygame.K_9: 0x26, pygame.K_0: 0x27,
    pygame.K_SPACE: 0x2C,
    pygame.K_TAB:   0x2B,
    pygame.K_MINUS: 0x2D,
    pygame.K_EQUALS: 0x2E,
    pygame.K_LEFTBRACKET:  0x2F,
    pygame.K_RIGHTBRACKET: 0x30,
    pygame.K_SEMICOLON:    0x33,
    pygame.K_QUOTE:        0x34,
    pygame.K_COMMA:        0x36,
    pygame.K_PERIOD:       0x37,
    pygame.K_SLASH:        0x38,
}


def _rgb565_to_rgb888(buf: bytes) -> np.ndarray:
    """Decode a 320x172 RGB565 (little-endian) buffer into an HxWx3 uint8."""
    pixels = np.frombuffer(buf, dtype="<u2").reshape(LCD_H, LCD_W)
    r = ((pixels >> 11) & 0x1F).astype(np.uint8)
    g = ((pixels >> 5)  & 0x3F).astype(np.uint8)
    b = ( pixels        & 0x1F).astype(np.uint8)
    # Expand 5/6/5 to 8 bits with bit-replication for smooth ramps.
    r = (r << 3) | (r >> 2)
    g = (g << 2) | (g >> 4)
    b = (b << 3) | (b >> 2)
    return np.stack([r, g, b], axis=-1)


def _pump_input(emu: Emu) -> bool:
    """Drain pygame events into emu kbd queue.  Returns False on quit."""
    for ev in pygame.event.get():
        if ev.type == pygame.QUIT:
            return False
        if ev.type != pygame.KEYDOWN:
            continue

        # ESC quits the window.
        if ev.key == pygame.K_ESCAPE:
            return False

        # ── Status hotkeys (F1..F8) ──
        # F1 = caps     F2 = conn     F3 = layer    F4 = OS
        # F5 = charging F6 = winlock  F7 = -10% bat F8 = +10% bat
        if ev.key == pygame.K_F1:
            emu.set_caps(not emu.status.caps)
            log.info("status: caps=%d", emu.status.caps)
        elif ev.key == pygame.K_F2:
            new = emu.cycle_conn()
            log.info("status: conn=%s", CONN_LABEL.get(new, new))
        elif ev.key == pygame.K_F3:
            new = emu.cycle_layer()
            log.info("status: layer=%d", new)
        elif ev.key == pygame.K_F4:
            new = emu.cycle_os()
            log.info("status: os=%s", OS_LABEL.get(new, new))
        elif ev.key == pygame.K_F5:
            emu.set_charging(not emu.status.is_charging)
            log.info("status: charging=%d", emu.status.is_charging)
        elif ev.key == pygame.K_F6:
            emu.set_winlock(not emu.status.winlock)
            log.info("status: winlock=%d", emu.status.winlock)
        elif ev.key == pygame.K_F7:
            new = emu.bump_battery(-10)
            log.info("status: battery=%d%%", new)
        elif ev.key == pygame.K_F8:
            new = emu.bump_battery(+10)
            log.info("status: battery=%d%%", new)

        # ── LCD nav buttons ──
        elif ev.key == pygame.K_UP:
            emu.push_kbd_lcd(LCD_UP)
        elif ev.key == pygame.K_DOWN:
            emu.push_kbd_lcd(LCD_DOWN)
        elif ev.key == pygame.K_RETURN:
            emu.push_kbd_lcd(LCD_ENTER)
        elif ev.key == pygame.K_BACKSPACE:
            emu.push_kbd_lcd(LCD_BACK)
        else:
            hid = _PG_TO_HID.get(ev.key)
            if hid is not None:
                emu.push_kbd_key(hid)
    return True


def run_window(emu: Emu, args) -> int:
    pygame.init()
    pygame.display.set_caption("meletricks emulator")
    scale = max(1, args.scale)
    win_w, win_h = LCD_W * scale, LCD_H * scale
    screen = pygame.display.set_mode((win_w, win_h))
    clock = pygame.time.Clock()

    # Re-used buffers.
    surf_native = pygame.Surface((LCD_W, LCD_H))
    last_blit_surf = None

    running = True
    while running:
        running = _pump_input(emu)

        try:
            emu.run_tick()
        except Exception:
            log.exception("guest tick raised; stopping")
            running = False
            break

        frame = emu.take_frame()
        if frame is not None and len(frame) == LCD_W * LCD_H * 2:
            arr = _rgb565_to_rgb888(frame)
            # pygame.surfarray.blit_array expects (W, H, 3), so transpose.
            pygame.surfarray.blit_array(surf_native, arr.swapaxes(0, 1))
            last_blit_surf = pygame.transform.scale(surf_native, (win_w, win_h))

        if last_blit_surf is not None:
            screen.blit(last_blit_surf, (0, 0))
            pygame.display.flip()

        clock.tick(args.max_fps)

    pygame.quit()
    return 0


def run_headless(emu: Emu, args) -> int:
    """Run forever (or Ctrl-C); optionally dump every Nth presented frame to PPM."""
    import pathlib
    import time as _time
    out = pathlib.Path(args.dump_frames) if args.dump_frames else None
    if out is not None:
        out.mkdir(parents=True, exist_ok=True)
    n = 0
    saved = 0
    ticks = 0
    last_log = _time.monotonic()
    try:
        while True:
            emu.run_tick()
            ticks += 1
            frame = emu.take_frame()
            if frame is not None:
                n += 1
                if out is not None and (n == 1 or n % 30 == 0):
                    arr = _rgb565_to_rgb888(frame)
                    path = out / f"frame_{saved:05d}.ppm"
                    with path.open("wb") as fh:
                        fh.write(f"P6\n{LCD_W} {LCD_H}\n255\n".encode())
                        fh.write(arr.tobytes())
                    saved += 1
            now = _time.monotonic()
            if now - last_log >= 1.0:
                print(f"[hl] ticks={ticks} presented={n} saved={saved}", file=sys.stderr)
                last_log = now
    except KeyboardInterrupt:
        return 0
