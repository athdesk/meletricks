#include "widget_wireframe.h"
#include "wpm.h"
#include "timer.h"

/* -- Shape catalogue ----------------------------------------------
 * Vertices are Q12 with the natural unit at ±4096 so the projection
 * focal/min_dim math keeps working unchanged for every shape. */

static const s16 CUBE_V[8][3] = {
    { -4096, -4096, -4096 }, {  4096, -4096, -4096 },
    {  4096,  4096, -4096 }, { -4096,  4096, -4096 },
    { -4096, -4096,  4096 }, {  4096, -4096,  4096 },
    {  4096,  4096,  4096 }, { -4096,  4096,  4096 },
};
static const u8 CUBE_E[12][2] = {
    {0,1},{1,2},{2,3},{3,0},
    {4,5},{5,6},{6,7},{7,4},
    {0,4},{1,5},{2,6},{3,7},
};

/* Regular tetrahedron: 4 verts at alternating corners of a cube. */
static const s16 TETRA_V[4][3] = {
    {  4096,  4096,  4096 },
    { -4096, -4096,  4096 },
    { -4096,  4096, -4096 },
    {  4096, -4096, -4096 },
};
static const u8 TETRA_E[6][2] = {
    {0,1},{0,2},{0,3},{1,2},{1,3},{2,3},
};

/* Regular octahedron: 6 verts on the principal axes. Each axis vert
 * links to the 4 perpendicular axis verts -> 12 edges. */
static const s16 OCTA_V[6][3] = {
    {  4096,     0,     0 },   /* +X */
    { -4096,     0,     0 },   /* -X */
    {     0,  4096,     0 },   /* +Y */
    {     0, -4096,     0 },   /* -Y */
    {     0,     0,  4096 },   /* +Z */
    {     0,     0, -4096 },   /* -Z */
};
static const u8 OCTA_E[12][2] = {
    {0,2},{0,3},{0,4},{0,5},
    {1,2},{1,3},{1,4},{1,5},
    {2,4},{2,5},{3,4},{3,5},
};

/* Square pyramid: apex up, square base. */
static const s16 PYRAMID_V[5][3] = {
    {     0,  4096,     0 },     /* 0: apex */
    {  4096, -4096,  4096 },     /* 1: base front-right */
    { -4096, -4096,  4096 },     /* 2: base front-left  */
    { -4096, -4096, -4096 },     /* 3: base back-left   */
    {  4096, -4096, -4096 },     /* 4: base back-right  */
};
static const u8 PYRAMID_E[8][2] = {
    {0,1},{0,2},{0,3},{0,4},      /* apex to corners */
    {1,2},{2,3},{3,4},{4,1},      /* base ring       */
};

/* Triangular prism: two parallel triangles, three connecting edges. */
static const s16 PRISM_V[6][3] = {
    {     0,  4096,  4096 },     /* 0: front-top    */
    { -4096, -4096,  4096 },     /* 1: front-botL   */
    {  4096, -4096,  4096 },     /* 2: front-botR   */
    {     0,  4096, -4096 },     /* 3: back-top     */
    { -4096, -4096, -4096 },     /* 4: back-botL    */
    {  4096, -4096, -4096 },     /* 5: back-botR    */
};
static const u8 PRISM_E[9][2] = {
    {0,1},{1,2},{2,0},
    {3,4},{4,5},{5,3},
    {0,3},{1,4},{2,5},
};

/* Hexagonal prism: 12 verts (6 top, 6 bottom), 18 edges (12 around
 * + 6 vertical). sin(60°)≈0.866 -> 3547 in Q12 with unit 4096. */
static const s16 HEX_V[12][3] = {
    /* top hex (y = +4096) */
    {  4096,  4096,     0 },     /* 0 */
    {  2048,  4096,  3547 },     /* 1 */
    { -2048,  4096,  3547 },     /* 2 */
    { -4096,  4096,     0 },     /* 3 */
    { -2048,  4096, -3547 },     /* 4 */
    {  2048,  4096, -3547 },     /* 5 */
    /* bottom hex (y = -4096) */
    {  4096, -4096,     0 },     /* 6 */
    {  2048, -4096,  3547 },     /* 7 */
    { -2048, -4096,  3547 },     /* 8 */
    { -4096, -4096,     0 },     /* 9 */
    { -2048, -4096, -3547 },     /*10 */
    {  2048, -4096, -3547 },     /*11 */
};
static const u8 HEX_E[18][2] = {
    {0,1},{1,2},{2,3},{3,4},{4,5},{5,0},      /* top ring   */
    {6,7},{7,8},{8,9},{9,10},{10,11},{11,6},  /* bot ring   */
    {0,6},{1,7},{2,8},{3,9},{4,10},{5,11},    /* verticals  */
};

/* Icosahedron: 12 verts of the form (0, ±a, ±b), (±a, ±b, 0),
 * (±b, 0, ±a). Standard a = b/φ — for b=4096, a = 2532 (4096/φ).
 * 30 edges; each vertex has exactly 5 neighbours (regular 5-valent). */
static const s16 ICO_V[12][3] = {
    {     0,  2532,  4096 },     /* 0 */
    {     0,  2532, -4096 },     /* 1 */
    {     0, -2532,  4096 },     /* 2 */
    {     0, -2532, -4096 },     /* 3 */
    {  2532,  4096,     0 },     /* 4 */
    {  2532, -4096,     0 },     /* 5 */
    { -2532,  4096,     0 },     /* 6 */
    { -2532, -4096,     0 },     /* 7 */
    {  4096,     0,  2532 },     /* 8 */
    {  4096,     0, -2532 },     /* 9 */
    { -4096,     0,  2532 },     /*10 */
    { -4096,     0, -2532 },     /*11 */
};
static const u8 ICO_E[30][2] = {
    {0,2},{0,4},{0,6},{0,8},{0,10},
    {1,3},{1,4},{1,6},{1,9},{1,11},
    {2,5},{2,7},{2,8},{2,10},
    {3,5},{3,7},{3,9},{3,11},
    {4,6},{4,8},{4,9},
    {5,7},{5,8},{5,9},
    {6,10},{6,11},
    {7,10},{7,11},
    {8,9},{10,11},
};

/* Dodecahedron: 20 verts at the 8 cube corners (±a, ±a, ±a) plus 12
 * "golden-rectangle" verts of the form (0, ±α, ±β) etc. For a=2532
 * (= 4096/φ), α=1564 (= 4096/φ²), β=4096. 30 edges, each vertex
 * 3-valent. Cube corners link to one A, one B, one D vertex; A/B/D
 * verts each link to two cube corners and one same-type sibling. */
static const s16 DODECA_V[20][3] = {
    {  2532,  2532,  2532 },     /* 0  cube +++ */
    {  2532,  2532, -2532 },     /* 1  cube ++- */
    {  2532, -2532,  2532 },     /* 2  cube +-+ */
    {  2532, -2532, -2532 },     /* 3  cube +-- */
    { -2532,  2532,  2532 },     /* 4  cube -++ */
    { -2532,  2532, -2532 },     /* 5  cube -+- */
    { -2532, -2532,  2532 },     /* 6  cube --+ */
    { -2532, -2532, -2532 },     /* 7  cube --- */
    {     0,  1564,  4096 },     /* 8  A 0+α+β  */
    {     0,  1564, -4096 },     /* 9  A 0+α-β  */
    {     0, -1564,  4096 },     /*10  A 0-α+β  */
    {     0, -1564, -4096 },     /*11  A 0-α-β  */
    {  1564,  4096,     0 },     /*12  B +α+β0  */
    {  1564, -4096,     0 },     /*13  B +α-β0  */
    { -1564,  4096,     0 },     /*14  B -α+β0  */
    { -1564, -4096,     0 },     /*15  B -α-β0  */
    {  4096,     0,  1564 },     /*16  D +β0+α  */
    {  4096,     0, -1564 },     /*17  D +β0-α  */
    { -4096,     0,  1564 },     /*18  D -β0+α  */
    { -4096,     0, -1564 },     /*19  D -β0-α  */
};
static const u8 DODECA_E[30][2] = {
    /* Each cube corner -> one A, one B, one D. */
    { 0, 8},{ 0,12},{ 0,16},
    { 1, 9},{ 1,12},{ 1,17},
    { 2,10},{ 2,13},{ 2,16},
    { 3,11},{ 3,13},{ 3,17},
    { 4, 8},{ 4,14},{ 4,18},
    { 5, 9},{ 5,14},{ 5,19},
    { 6,10},{ 6,15},{ 6,18},
    { 7,11},{ 7,15},{ 7,19},
    /* Same-type pairs (A-A, B-B, D-D). */
    { 8,10},{ 9,11},
    {12,14},{13,15},
    {16,17},{18,19},
};

/* Stella octangula (stellated octahedron): two interpenetrating
 * tetrahedra. 8 verts = 4 cube corners + their inversions. 12 edges
 * = the 6 edges of each tetra; the two wireframes never share an
 * edge so they compose into a 3D star silhouette. */
static const s16 STELLA_V[8][3] = {
    {  4096,  4096,  4096 },     /* 0  tetra A */
    { -4096, -4096,  4096 },     /* 1 */
    { -4096,  4096, -4096 },     /* 2 */
    {  4096, -4096, -4096 },     /* 3 */
    { -4096, -4096, -4096 },     /* 4  tetra B (= -A) */
    {  4096,  4096, -4096 },     /* 5 */
    {  4096, -4096,  4096 },     /* 6 */
    { -4096,  4096,  4096 },     /* 7 */
};
static const u8 STELLA_E[12][2] = {
    {0,1},{0,2},{0,3},{1,2},{1,3},{2,3},      /* tetra A edges */
    {4,5},{4,6},{4,7},{5,6},{5,7},{6,7},      /* tetra B edges */
};

const WireframeShape WIREFRAME_SHAPES[WIREFRAME_SHAPE_COUNT] = {
    { "Cube",         CUBE_V,    8, CUBE_E,   12 },
    { "Tetrahedron",  TETRA_V,   4, TETRA_E,   6 },
    { "Octahedron",   OCTA_V,    6, OCTA_E,   12 },
    { "Pyramid",      PYRAMID_V, 5, PYRAMID_E, 8 },
    { "Prism",        PRISM_V,   6, PRISM_E,   9 },
    { "Hex Prism",    HEX_V,    12, HEX_E,    18 },
    { "Icosahedron",  ICO_V,    12, ICO_E,    30 },
    { "Dodecahedron", DODECA_V, 20, DODECA_E, 30 },
    { "Stella",       STELLA_V,  8, STELLA_E, 12 },
};

const char *const WIREFRAME_SHAPE_LABELS[WIREFRAME_SHAPE_COUNT] = {
    "Cube", "Tetrahedron", "Octahedron", "Pyramid",
    "Prism", "Hex Prism", "Icosahedron", "Dodecahedron", "Stella",
};

static inline const WireframeShape *resolve_shape(const WireframeDemo *a)
{
    return a->shape ? a->shape : &WIREFRAME_SHAPES[0];
}

void WireframeDemoSetShape(WireframeDemo *a, const WireframeShape *shape)
{
    if (!a) return;
    a->shape    = shape;
    /* Old pose's prev_px/py are still valid as pixel coords but the
     * old edge table is gone — drop them so the next Draw skips the
     * erase pass and just paints the new shape. With an overlay bg
     * the next Draw will full-blit and re-seed anyway. */
    a->has_prev = 0;
}

static void project(const WireframeDemo *a, const WireframeShape *s,
                    int ax, int ay,
                    int cx_box, int cy_box, int focal_px,
                    s16 *out_x, s16 *out_y)
{
    int cay = GfxCosQ12(ay), say = GfxSinQ12(ay);
    int cax = GfxCosQ12(ax), sax = GfxSinQ12(ax);
    for (int i = 0; i < s->vert_count; i++) {
        int x = s->verts[i][0], y = s->verts[i][1], z = s->verts[i][2];
        int x1 = ( x * cay + z * say) >> 12;
        int z1 = (-x * say + z * cay) >> 12;
        int y2 = ( y * cax - z1 * sax) >> 12;
        int z2 = ( y * sax + z1 * cax) >> 12;
        int z_view = z2 + 16384;
        if (z_view < 4096) z_view = 4096;
        /* x1, y2 and z_view are all Q12 — the 4096s cancel, no extra
         * shift needed. A stray `>> 12` here once produced screen
         * coords 4096x oversize and lines so long Bresenham ate the
         * watchdog. */
        out_x[i] = (s16)(cx_box + (x1 * focal_px) / z_view);
        out_y[i] = (s16)(cy_box - (y2 * focal_px) / z_view);
    }
}

void WireframeDemoDraw(GfxRenderingTile *tile, WireframeDemo *a)
{
    int cx_box   = tile->box.x + tile->box.w / 2;
    int cy_box   = tile->box.y + tile->box.h / 2;
    int pct      = (a->size_pct > 0) ? a->size_pct : 100;
    int min_dim  = tile->box.w < tile->box.h ? tile->box.w : tile->box.h;
    int focal_px = (min_dim * pct) / 100;

    /* Clip to box so corner-pose overshoots can't paint into chrome,
     * and so prev-frame erase lines from a different pose can't either. */
    GfxClip saved = GfxFbPushClip(tile->fb, tile->box);

    if (a->bg
        && (GfxIsScreenChangeFrame() || a->bg_seen_version != a->bg->version))
    {
        GfxBlitFb(tile->fb, tile->box.x, tile->box.y, a->bg->fb);
        a->has_prev        = 0;
        a->bg_seen_version = a->bg->version;
    }

    const WireframeShape *s = resolve_shape(a);
    s16 px[WIREFRAME_MAX_VERTS], py[WIREFRAME_MAX_VERTS];
    project(a, s, a->ax, a->ay, cx_box, cy_box, focal_px, px, py);

    /* Prev pose was always drawn with this same shape — SetShape
     * dropped has_prev whenever the shape switched. So the current
     * edge table matches the prev_px/py coordinates. */
    if (a->has_prev) {
        for (int i = 0; i < s->edge_count; i++) {
            int p = s->edges[i][0], q = s->edges[i][1];
            if (a->bg) {
                GfxRestoreLine(tile->fb,
                               a->prev_px[p], a->prev_py[p],
                               a->prev_px[q], a->prev_py[q],
                               a->bg->fb, tile->box.x, tile->box.y);
            } else {
                GfxLine(tile->fb,
                        a->prev_px[p], a->prev_py[p],
                        a->prev_px[q], a->prev_py[q], a->bg_color);
            }
        }
    }
    for (int i = 0; i < s->edge_count; i++) {
        int p = s->edges[i][0], q = s->edges[i][1];
        GfxLine(tile->fb, px[p], py[p], px[q], py[q], a->color);
    }
    for (int i = 0; i < s->vert_count; i++) {
        a->prev_px[i] = px[i];
        a->prev_py[i] = py[i];
    }
    a->has_prev = 1;

    GfxFbPopClip(tile->fb, saved);
}

int WireframeDemoTick(WireframeDemo *a)
{
    u32 now = fr_millis();
    if (a->last_tick_ms == 0) { a->last_tick_ms = now; return 1; }
    if (now - a->last_tick_ms < 50u) return 0;
    a->last_tick_ms = now;

    /* Idle drift at (1°, 1°); typing snaps to (3°, 4°) + wpm/12 so
     * rotation visibly tracks burst speed. */
    u32 wpm = wpm_current();
    int dx, dy;
    if (wpm == 0u) { dx = 1; dy = 1; }
    else {
        dx = 3 + (int)(wpm / 12u);
        dy = 4 + (int)(wpm / 12u);
    }
    if (dx > 18) dx = 18;
    if (dy > 18) dy = 18;
    a->ax = (s16)((a->ax + dx) % 360);
    a->ay = (s16)((a->ay + dy) % 360);
    return 1;
}
