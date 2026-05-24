/* WireframeDemo: 3D wireframe, perspective-projected, rotating around
 * X and Y. Spin speed scales with WPM. Partial redraw — erase prev
 * edges, draw new ones; no full-box clear. Shape is a pointer into
 * WIREFRAME_SHAPES so the same widget instance can morph between
 * cube / tetrahedron / octahedron at runtime via WireframeDemoSetShape. */
#ifndef HELLO_WIDGET_WIREFRAME_H
#define HELLO_WIDGET_WIREFRAME_H

#include "gfx.h"

/* Inline caps sized for the largest model we ship. prev_px/py are
 * sized to MAX_VERTS — bump alongside any shape with more vertices.
 * MAX_EDGES is documentation only (each shape carries its own
 * edges[][2]); kept here for orientation. */
#define WIREFRAME_MAX_VERTS 20    /* dodecahedron has 20 */
#define WIREFRAME_MAX_EDGES 30    /* icosahedron + dodecahedron have 30 */

typedef struct {
    const char  *name;
    const s16  (*verts)[3];     /* Q12 coords, normalised to ±4096    */
    int          vert_count;
    const u8   (*edges)[2];     /* vertex-index pairs                 */
    int          edge_count;
} WireframeShape;

/* Keep in sync with the table in widget_wireframe.c. Compile-time so
 * the menu-list CHOICE item can take it as `.option_count`. */
#define WIREFRAME_SHAPE_COUNT 9
extern const WireframeShape WIREFRAME_SHAPES[WIREFRAME_SHAPE_COUNT];

/* Label list separate from the table so the menu-list CHOICE item
 * (which takes `const char *const *`) can point straight at it. */
extern const char *const WIREFRAME_SHAPE_LABELS[WIREFRAME_SHAPE_COUNT];

typedef struct {
    GfxColor               color;
    GfxColor               bg_color;
    /* Optional overlay background, same contract as AnimDemo's. */
    const GfxRenderTarget *bg;
    /* Shape currently rendered. NULL is treated as the first entry
     * of WIREFRAME_SHAPES (the cube) — keeps construction tolerant
     * of designated initialisers that omit `.shape`. */
    const WireframeShape  *shape;
    /* Projection focal length as a percentage of min(w, h). 100 fills
     * the body face-on. 0 (default-init) is treated as 100. */
    int                    size_pct;
    /* internal */
    u32 bg_seen_version;
    s16 ax, ay;
    u32 last_tick_ms;
    s16 prev_px[WIREFRAME_MAX_VERTS];
    s16 prev_py[WIREFRAME_MAX_VERTS];
    bool   has_prev;
} WireframeDemo;

void WireframeDemoDraw(GfxRenderingTile *tile, WireframeDemo *a);
int  WireframeDemoTick(WireframeDemo *a);

/* Swap the rendered shape. Invalidates the prev-edge cache so stale
 * edges of the old shape don't repaint into the new pose. */
void WireframeDemoSetShape(WireframeDemo *a, const WireframeShape *shape);

#define NewWireframeDemo(...) \
    GfxNewWidget(sizeof(WireframeDemo), &(WireframeDemo){ __VA_ARGS__ }, \
                 GFX_DRAW_FN(WireframeDemoDraw), GFX_TICK_FN(WireframeDemoTick))

#endif
