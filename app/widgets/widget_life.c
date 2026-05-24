#include "widget_life.h"
#include "fr8000.h"   /* rand */
#include "timer.h"

void LifeDemoDraw(GfxRenderingTile *tile, LifeDemo *l)
{
    int cp   = l->cell_px;
    int draw = cp - 1; if (draw < 1) draw = 1;

    /* First frame after a screen change: the framebuffer was just
     * cleared, so prev_drawn no longer reflects what's on screen.
     * Force a full repaint and resync. After that we only flip the
     * cells whose state changed since the last paint. */
    if (GfxIsScreenChangeFrame()) {
        GfxFillRect(tile->fb, tile->box.x, tile->box.y,
                    tile->box.w, tile->box.h, l->bg_color);
        for (int r = 0; r < l->rows; r++) {
            for (int c = 0; c < l->cols; c++) {
                int i = r * l->cols + c;
                if (l->grid[i]) {
                    GfxFillRect(tile->fb,
                                tile->box.x + c * cp, tile->box.y + r * cp,
                                draw, draw, l->color);
                }
                l->prev_drawn[i] = l->grid[i];
            }
        }
        return;
    }

    for (int r = 0; r < l->rows; r++) {
        for (int c = 0; c < l->cols; c++) {
            int i   = r * l->cols + c;
            u8  cur = l->grid[i];
            if (cur != l->prev_drawn[i]) {
                GfxFillRect(tile->fb,
                            tile->box.x + c * cp, tile->box.y + r * cp,
                            draw, draw, cur ? l->color : l->bg_color);
                l->prev_drawn[i] = cur;
            }
        }
    }
}

int LifeDemoTick(LifeDemo *l)
{
    u32 now = fr_millis();
    if (l->last_step_ms == 0) { l->last_step_ms = now; return 0; }
    if (now - l->last_step_ms < l->step_interval_ms) return 0;
    l->last_step_ms = now;

    int cols = l->cols, rows = l->rows;
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            int n = 0;
            for (int dr = -1; dr <= 1; dr++) {
                int rr = (r + dr + rows) % rows;
                for (int dc = -1; dc <= 1; dc++) {
                    if (dr == 0 && dc == 0) continue;
                    int cc = (c + dc + cols) % cols;
                    if (l->grid[rr * cols + cc]) n++;
                }
            }
            u8 alive = l->grid[r * cols + c];
            l->next[r * cols + c] = (alive ? (n == 2 || n == 3) : (n == 3));
        }
    }
    for (int i = 0; i < cols * rows; i++) l->grid[i] = l->next[i];
    return 1;
}

typedef struct {
    u8 cell_count;
    s8 cells[8][2];           /* (dr, dc) offsets from the anchor */
} LifePattern;

/* Mix of still lifes, oscillators, and chaotic seeds — a single live
 * cell would starve immediately, so each keystroke drops a whole
 * shape with some probability instead. */
static const LifePattern LIFE_PATTERNS[] = {
    { 5, { {0,1},{1,2},{2,0},{2,1},{2,2} } },              /* Glider */
    { 3, { {0,0},{0,1},{0,2} } },                          /* Blinker */
    { 4, { {0,0},{0,1},{1,0},{1,1} } },                    /* Block */
    { 6, { {0,1},{0,2},{0,3},{1,0},{1,1},{1,2} } },        /* Toad */
    { 6, { {0,0},{0,1},{1,0},{2,3},{3,2},{3,3} } },        /* Beacon */
    { 5, { {0,1},{0,2},{1,0},{1,1},{2,1} } },              /* R-pentomino */
    { 7, { {0,1},{1,3},{2,0},{2,1},{2,4},{2,5},{2,6} } },  /* Acorn */
};
#define LIFE_PATTERN_COUNT  ((int)(sizeof(LIFE_PATTERNS)/sizeof(LIFE_PATTERNS[0])))

void LifeDemoAddRandomPattern(LifeDemo *l)
{
    const LifePattern *p = &LIFE_PATTERNS[(rand() & 0x7FFF) % LIFE_PATTERN_COUNT];
    int r0 = (rand() & 0x7FFF) % l->rows;
    int c0 = (rand() & 0x7FFF) % l->cols;
    for (int i = 0; i < p->cell_count; i++) {
        int r = (r0 + p->cells[i][0] + l->rows) % l->rows;
        int c = (c0 + p->cells[i][1] + l->cols) % l->cols;
        l->grid[r * l->cols + c] = 1;
    }
}

void LifeDemoSetAccent(LifeDemo *l, GfxColor c)
{
    l->color = c;
    int n = l->cols * l->rows;
    for (int i = 0; i < n; i++) l->prev_drawn[i] = !l->grid[i];
}

void LifeDemoSetBg(LifeDemo *l, GfxColor c)
{
    l->bg_color = c;
    int n = l->cols * l->rows;
    for (int i = 0; i < n; i++) l->prev_drawn[i] = !l->grid[i];
}
