#ifndef GFX_WIDGETS_MENU_LIST_H
#define GFX_WIDGETS_MENU_LIST_H
#include "gfx.h"

/* -- Menu list ---- vertical scrolling list with selection + indicator
 * column. Mutates on user input — caller invokes Prev/Next/Activate
 * then GfxMarkDirty. */
/* Menu items are typed via the discriminator below — only the matching
 * union member is meaningful. Add new item kinds (e.g. multi-select,
 * action button) by extending the enum + union, then handling in
 * GfxMenuListDraw / GfxMenuListAdjust / your menu_activate. */
typedef enum {
    GFX_MENU_LINK   = 0,    /* activate → caller navigates to .link.target */
    GFX_MENU_TOGGLE = 1,    /* on/off, drawn as `Label: on|off`            */
    GFX_MENU_SLIDER = 2,    /* numeric, drawn as label + bar + value       */
    GFX_MENU_CHOICE = 3,    /* multi-option pick, drawn as label + option  */
    GFX_MENU_ACTION = 4,    /* one-shot callback, drawn as label only      */
} GfxMenuItemType;

typedef struct {
    const char     *label;
    GfxMenuItemType type;
    union {
        struct {
            const void *target;     /* opaque, caller-defined */
        } link;
        struct {
            int  (*get)(void);
            void (*set)(int);
        } toggle;
        struct {
            int  (*get)(void);
            void (*set)(int);
            int   min;
            int   max;
            int   step;
        } slider;
        struct {
            const char *const *options;       /* labels, indexed 0..count-1 */
            int                option_count;
            int              (*get)(void);    /* returns current index      */
            void             (*set)(int);     /* receives new index         */
        } choice;
        struct {
            void (*activate)(void);
        } action;
    };
} GfxMenuItem;

typedef enum {
    GFX_MENU_INDICATOR_LEFT  = 0,
    GFX_MENU_INDICATOR_RIGHT = 1,
    GFX_MENU_INDICATOR_NONE  = 2,
} GfxMenuIndicatorAlign;

typedef struct {
    const GfxFont            *font;
    GfxColor                  color_normal;
    GfxColor                  color_selected;
    GfxColor                  color_indicator;
    GfxColor                  color_indicator_inactive;
    GfxColor                  bg_color;
    bool                      skip_clear;
    const GfxMenuItem        *items;
    int                       item_count;
    int                       selected;
    int                       scroll;
    int                       item_spacing;
    int                       indicator_pad;
    GfxMenuIndicatorAlign     indicator_align;
    bool                      editing;            /* true = adjusting selected slider */
    /* Per-item hide bitmap: bit N == 1 means items[N] is omitted from
     * draw + selection. `items` stays const (caller's static array),
     * so visibility is controlled here on the widget. Capped at 32
     * items — well above the row count any list this size can show.
     * Selection helpers skip hidden indices, the draw fn doesn't
     * paint or reserve a row for them, and the scroll-indicator dots
     * count only visible items. */
    u32                       hidden_mask;
} GfxMenuList;

void GfxMenuListDraw      (GfxRenderingTile *tile, GfxMenuList *m);
void GfxMenuListSelectPrev(GfxMenuList *m);
void GfxMenuListSelectNext(GfxMenuList *m);

/* Hidden-item API. `idx` is the items[] index, not a visible-row
 * position. Hiding the currently selected row auto-moves selection to
 * the nearest visible row (next, then prev, then none). Marks the
 * widget dirty when called via GfxMenuListSetHidden so the caller
 * doesn't need to. GfxMenuListIsHidden is a const query helper.  */
void GfxMenuListSetHidden (GfxWidget *w, int idx, int hidden);
int  GfxMenuListIsHidden  (const GfxMenuList *m, int idx);

/* Slider adjust: when `editing` is set and the selected item has
 * get_value/set_value, applies one `value_step` in `dir` (+1 / −1)
 * and clamps to [value_min, value_max]. */
void GfxMenuListAdjust(GfxMenuList *m, int dir);

#define NewGfxMenuList(...) \
    GfxNewWidget(sizeof(GfxMenuList), \
                 &(GfxMenuList){ __VA_ARGS__ }, \
                 GFX_DRAW_FN(GfxMenuListDraw), \
                 NULL)

#endif
