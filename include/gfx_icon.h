#ifndef GFX_ICON_H
#define GFX_ICON_H
#include "gfx_widgets.h"

/* `h` here is the available height. It's meant as a hint.
 * Proportional icons: fixed aspect ratio, variable size.
 * Fixed icons: Can use `h` to scale.
 */
typedef void (*GfxIconDrawFn)(GfxFb *fb, int x, int y, int h,
                               GfxColor fg, GfxColor bg, void *data);

typedef enum {
    GFX_ICON_PROPORTIONAL,
    GFX_ICON_FIXED,
    GFX_ICON_TEXT,
} GfxIconType;

typedef struct {
    GfxIconType type;
    union {
        struct {
            GfxIconDrawFn  draw;
            void          *data;
        } proportional;
        struct {
            GfxIconDrawFn  draw;
            void          *data;
            int            w, h;   /* natural size; GfxIconWidth scales w to h */
        } fixed;
        struct {
            const GfxFont *font;
            /* get_text is called once by GfxIconWidth (for sizing/centering)
             * and once by GfxIconDraw (for rendering);
             * both calls must produce identical output. */
            void         (*get_text)(char *buf, int size, void *data);
            void          *data;
        } text;
    };
} GfxIcon;

/* Return the preferred width of `icon` for the given available height `h`. */
int GfxIconWidth(const GfxIcon *icon, int h);

/* Draw `icon` at (x, y) with available height `h`.
 * x is the pre-computed left edge (use GfxIconWidth to center before calling).
 */
void GfxIconDraw(GfxFb *fb, const GfxIcon *icon,
                 int x, int y, int h,
                 GfxColor fg, GfxColor bg);

/* get_text helper for string literals: copies (const char*)data into buf. */
void GfxIconTextStaticFill(char *buf, int size, void *data);

/* -- Construction macros ----------------------------------------------- */

#define NewGfxIconProportional(fn_, data_) \
    ((GfxIcon){ .type = GFX_ICON_PROPORTIONAL, \
                .proportional = { (fn_), (void *)(data_) } })

#define NewGfxIconFixed(fn_, data_, w_, h_) \
    ((GfxIcon){ .type = GFX_ICON_FIXED, \
                .fixed = { (fn_), (void *)(data_), (w_), (h_) } })

#define NewGfxIconText(font_, fn_, data_) \
    ((GfxIcon){ .type = GFX_ICON_TEXT, \
                .text = { (font_), (fn_), (void *)(data_) } })

#define NewGfxIconTextStatic(font_, str_) \
    NewGfxIconText((font_), GfxIconTextStaticFill, (void *)(str_))

#endif /* GFX_ICON_H */
