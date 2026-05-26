#!/usr/bin/env python3
"""Headless UI tester: drive the emulator through every screen and dump a
PNG of each.

    python3 tools/emu/uitest.py build/<board>/meletricks_emu.elf

PNGs land in `screenshots/` (override with --out).  Requires Pillow for PNG
output; falls back to .ppm if Pillow isn't installed.
"""

from __future__ import annotations

import argparse
import logging
import random
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from emu import Emu, LCD_H, LCD_W
from ui import _rgb565_to_rgb888


log = logging.getLogger("uitest")


_KBD_LCD_NEXT, _KBD_LCD_UP, _KBD_LCD_ENTER, _KBD_LCD_BACK = 1, 2, 3, 4

# meletricks.c defaults s_input_inverted=1, so KBD_LCD_UP/NEXT are swapped
# before reaching the carousel/menu.  Define directions by their *effect*
# (carousel next / menu down) so the tour script reads naturally.
NEXT  = _KBD_LCD_UP
PREV  = _KBD_LCD_NEXT
ENTER = _KBD_LCD_ENTER
BACK  = _KBD_LCD_BACK

SETTLE_TICKS = 60
TICKS_BETWEEN_KEYS = 40


try:
    from PIL import Image as _PILImage
    _HAVE_PIL = True
except ImportError:
    _HAVE_PIL = False


def _save(frame: bytes, path: Path) -> Path:
    arr = _rgb565_to_rgb888(frame)
    if _HAVE_PIL:
        _PILImage.fromarray(arr).save(path)
        return path
    fallback = path.with_suffix(".ppm")
    with fallback.open("wb") as fh:
        fh.write(f"P6\n{LCD_W} {LCD_H}\n255\n".encode())
        fh.write(arr.tobytes())
    return fallback


class Tour:
    def __init__(self, emu: Emu, out_dir: Path) -> None:
        self.emu = emu
        self.out_dir = out_dir
        self.latest_frame: bytes | None = None
        self.step_no = 0
        out_dir.mkdir(parents=True, exist_ok=True)

    def tick(self, n: int) -> None:
        for _ in range(n):
            self.emu.run_tick()
            f = self.emu.take_frame()
            if f is not None:
                self.latest_frame = f

    def push(self, *codes: int, between: int = TICKS_BETWEEN_KEYS) -> None:
        for c in codes:
            self.emu.push_kbd_lcd(c)
            self.tick(between)

    def wait_real_ms(self, ms: int) -> None:
        """Sleep wall-clock time while ticking so widget timers (fr_millis)
        actually advance.  Needed for screens whose behavior depends on
        real-time intervals — e.g. game-of-life generation steps."""
        deadline = time.monotonic() + ms / 1000.0
        while time.monotonic() < deadline:
            self.emu.run_tick()
            f = self.emu.take_frame()
            if f is not None:
                self.latest_frame = f
            time.sleep(0.005)

    def push_hid(self, *scancodes: int, between_ms: int = 20) -> None:
        """Inject HID key events with a real-time gap to clear meletricks.c's
        HID_REPRESS_MS debounce when the same scancode repeats."""
        for sc in scancodes:
            self.emu.push_kbd_key(sc)
            self.wait_real_ms(between_ms)

    def human_type_for(self, seconds: float,
                       wpm_min: int = 70, wpm_max: int = 100,
                       seed: int = 0xDEADBEEF) -> None:
        """Inject letter keypresses at a randomly varying human pace for
        roughly `seconds` wall-clock seconds.  Each gap is drawn uniformly
        from [wpm_min, wpm_max] (5 chars/word canonical), keeping the WPM
        graph wiggling instead of pinned."""
        rng = random.Random(seed)
        deadline = time.monotonic() + seconds
        i = 0
        while time.monotonic() < deadline:
            self.emu.push_kbd_key(0x04 + (i % 26))
            wpm = rng.uniform(wpm_min, wpm_max)
            self.wait_real_ms(int(60_000 / (wpm * 5)))
            i += 1

    def capture(self, name: str) -> None:
        self.step_no += 1
        self.tick(SETTLE_TICKS)
        if self.latest_frame is None:
            log.warning("[%02d_%s] no frame ever presented", self.step_no, name)
            return
        out = self.out_dir / f"{self.step_no:02d}_{name}.png"
        actual = _save(self.latest_frame, out)
        log.info("  saved %s", actual.name)


def run_tour(t: Tour) -> None:
    # After FR_SETUP the carousel sits on item 0 (Bounce).  Walk through
    # every screen + per-screen settings, then the settings sub-menus.

    log.info("Main carousel + per-screen settings:")
    # Carousel item 0: Bounce
    t.capture("main_bounce")
    t.push(ENTER);                   t.capture("scr_bounce")
    t.push(ENTER);                   t.capture("scr_bounce_settings")
    t.push(BACK, BACK)

    # Item 1: Wireframe
    t.push(NEXT);                    t.capture("main_wireframe")
    t.push(ENTER);                   t.capture("scr_wireframe")
    t.push(ENTER);                   t.capture("scr_wireframe_settings")
    t.push(BACK, BACK)

    # Item 2: Life — only the carousel preview here.  Entering Life
    # requires seeding the grid with a burst of keypresses, which would
    # pump a tall spike into the WPM history and ruin the graph capture
    # two steps below.  We come back to Life at the very end of the tour.
    t.push(NEXT);                    t.capture("main_life")

    # Item 3: Clock (no per-screen settings)
    t.push(NEXT);                    t.capture("main_clock")
    t.push(ENTER);                   t.capture("scr_clock")
    t.push(BACK)

    # Item 4: WPM Graph — type at a human pace before capturing so the
    # 1-Hz history has several non-zero samples and the curve is visible.
    t.push(NEXT);                    t.capture("main_graph")
    t.push(ENTER)
    t.human_type_for(seconds=8)
    t.capture("scr_graph")
    t.push(ENTER);                   t.capture("scr_graph_settings")
    t.push(BACK, BACK)

    # Item 5: Settings (and its sub-menus)
    t.push(NEXT);                    t.capture("main_settings")
    t.push(ENTER);                   t.capture("scr_settings")

    log.info("Settings sub-screens:")
    # Settings menu rows (see SETTINGS_ITEMS in app/screens/screen_settings.c):
    #   0 About, 1 Debug, 2 Fonts, 3+ toggles/choices.
    t.push(ENTER);                   t.capture("scr_settings_about")      # row 0
    t.push(BACK)                                                          # back to settings, cursor=row 0
    t.push(NEXT, ENTER);             t.capture("scr_settings_debug")      # row 1

    # Debug menu rows: 5 toggles, then row 5 = "Key events" → text screen.
    t.push(NEXT, NEXT, NEXT, NEXT, NEXT, ENTER); t.capture("scr_debug_text")
    t.push(BACK)                                                          # back to debug, cursor=row 5
    t.push(BACK)                                                          # back to settings, cursor=row 1 (Debug)
    t.push(NEXT, ENTER);             t.capture("scr_settings_fonts")      # row 2

    # Deferred Life — back out to the main carousel and walk PREV from
    # position 5 (Settings) to position 2 (Life), then seed + evolve.
    log.info("Game of Life (deferred to avoid polluting WPM history):")
    t.push(BACK, BACK)                                                    # fonts → settings menu → main
    t.push(PREV, PREV, PREV)                                              # 5→4→3→2
    t.push(ENTER)
    t.push_hid(*[0x04 + (i % 26) for i in range(30)])
    t.wait_real_ms(2000)             # ~20 life generations at step_interval_ms=100
    t.capture("scr_life")
    t.push(ENTER);                   t.capture("scr_life_settings")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("elf", type=Path, help="path to meletricks_emu.elf")
    parser.add_argument("--out", type=Path, default=Path("screenshots"),
                        help="screenshot output directory (default screenshots/)")
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args()

    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format="%(levelname)s %(name)s: %(message)s",
    )

    if not args.elf.exists():
        sys.exit(f"{args.elf}: no such file (run `make emu-build` first)")

    if not _HAVE_PIL:
        log.warning("Pillow not installed — saving .ppm instead of .png "
                    "(pip install pillow to fix)")

    emu = Emu(args.elf)
    emu.run_setup()
    log.info("FR_SETUP done; warming up")

    t = Tour(emu, args.out)
    t.tick(SETTLE_TICKS * 2)  # let the initial frame paint

    run_tour(t)
    log.info("Tour complete: %s", args.out.resolve())
    return 0


if __name__ == "__main__":
    sys.exit(main())
