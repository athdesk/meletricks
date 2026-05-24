# fonts/

Each `font_*.c` here is a self-contained `const gfx_font_t font_<name>`
plus its `index_*`, `glyphs_*`, and `bitmap_*` private tables. They are
generated from TTF/OTF by `../tools/font_gen.py`.

To add a new font:

```
python ..\tools\font_gen.py C:\Windows\Fonts\arial.ttf 14 ^
    --name arial_14 --out font_arial_14.c
```

This also drops `font_<name>.preview.txt` alongside the `.c` — a per-glyph
ASCII grid used as the ground truth when verifying on-device output.

To use a font in a program:

```c
extern const gfx_font_t font_arial_14;
gfx_draw_text(&fb, &font_arial_14, x, y, "hello", GFX_WHITE);
```

That's it — there is no font registry, no init call. The font is just C
data linked into the blob.
