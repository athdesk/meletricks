#include "icons.h"

void icon_life(GfxFb *fb, int x, int y, int h, GfxColor fg, GfxColor bg, void *data)
{
    (void)bg; (void)data;
    int s  = h;
    int m  = s / 6;
    int cs = (s - 2 * m) / 3;
    /* Glider seed. */
    int p[5][2] = { {1,0}, {2,1}, {0,2}, {1,2}, {2,2} };
    for (int i = 0; i < 5; i++) {
        GfxFillRect(fb, x + m + p[i][0] * cs, y + m + p[i][1] * cs,
                    cs - 1, cs - 1, fg);
    }
}

void icon_cube(GfxFb *fb, int x, int y, int h, GfxColor fg, GfxColor bg, void *data)
{
    (void)bg; (void)data;
    int s  = h;
    int m  = s / 8;
    int dx = (s - 2 * m) / 4;
    int dy = (s - 2 * m) / 4;
    int fx0 = x + m,          fy0 = y + m + dy;
    int fx1 = x + s - m - dx, fy1 = y + s - m;
    int bx0 = x + m + dx,     by0 = y + m;
    int bx1 = x + s - m,      by1 = y + s - m - dy;
    GfxLine(fb, fx0, fy0, fx1, fy0, fg);  GfxLine(fb, fx1, fy0, fx1, fy1, fg);
    GfxLine(fb, fx1, fy1, fx0, fy1, fg);  GfxLine(fb, fx0, fy1, fx0, fy0, fg);
    GfxLine(fb, bx0, by0, bx1, by0, fg);  GfxLine(fb, bx1, by0, bx1, by1, fg);
    GfxLine(fb, bx1, by1, bx0, by1, fg);  GfxLine(fb, bx0, by1, bx0, by0, fg);
    GfxLine(fb, fx0, fy0, bx0, by0, fg);  GfxLine(fb, fx1, fy0, bx1, by0, fg);
    GfxLine(fb, fx1, fy1, bx1, by1, fg);  GfxLine(fb, fx0, fy1, bx0, by1, fg);
}

void icon_anim(GfxFb *fb, int x, int y, int h, GfxColor fg, GfxColor bg, void *data)
{
    (void)bg; (void)data;
    int s  = h;
    int cx = x + s / 2;
    int cy = y + s / 2;
    int step = s / 8;
    int sz[5] = { 3, 4, 5, 6, 8 };
    for (int i = 0; i < 5; i++) {
        int dx = cx - 2 * step + i * step - sz[i] / 2;
        int dy = cy + 2 * step - i * step - sz[i] / 2;
        GfxFillRect(fb, dx, dy, sz[i], sz[i], fg);
    }
}

void icon_text(GfxFb *fb, int x, int y, int h, GfxColor fg, GfxColor bg, void *data)
{
    (void)bg; (void)data;
    int s  = h;
    GfxDrawRect(fb, x, y, &(GfxRect){ .w = s, .h = s, .radius = s / 8, .color = fg });
    int m    = s / 5;
    int x0   = x + m;
    int x1   = x + s - m;
    int rows = 4;
    int step = (s - 2 * m) / (rows + 1);
    for (int i = 1; i <= rows; i++) {
        int yy    = y + m + i * step;
        /* Last line is short to suggest a paragraph break. */
        int x_end = (i == rows) ? (x0 + (x1 - x0) * 2 / 3) : x1;
        GfxHLine(fb, x0, yy, x_end - x0, fg);
    }
}

void icon_graph(GfxFb *fb, int x, int y, int h, GfxColor fg, GfxColor bg, void *data)
{
    (void)bg; (void)data;
    int s   = h;
    int m   = s / 6;
    int ax0 = x + m, ax1 = x + s - m;
    int ay0 = y + m, ay1 = y + s - m;
    GfxHLine(fb, ax0, ay1, ax1 - ax0, fg);
    GfxVLine(fb, ax0, ay0, ay1 - ay0, fg);
    int xs[5] = { ax0 + 2, ax0 + (ax1 - ax0) / 4, ax0 + (ax1 - ax0) * 2 / 4,
                  ax0 + (ax1 - ax0) * 3 / 4, ax1 - 2 };
    int ys[5] = { ay1 - 2, ay1 - (ay1 - ay0) / 3, ay1 - (ay1 - ay0) / 4,
                  ay0 + (ay1 - ay0) / 3, ay0 + 2 };
    for (int i = 0; i < 4; i++) GfxLine(fb, xs[i], ys[i], xs[i+1], ys[i+1], fg);
}

void icon_settings(GfxFb *fb, int x, int y, int h, GfxColor fg, GfxColor bg, void *data)
{
    (void)bg; (void)data;
    int s  = h;
    int cx = x + s / 2;
    int cy = y + s / 2;

    int body_r_outer = (s * 30) / 100;
    int body_r_inner = body_r_outer - 2;
    int tooth_w      = (s * 14) / 100;
    int tooth_h      = (s *  9) / 100;
    int tip_r        = (s *  6) / 100;
    int hub_r        = s / 12;
    if (body_r_inner < 2) body_r_inner = 2;
    if (tooth_w < 3) tooth_w = 3;
    if (tooth_h < 2) tooth_h = 2;
    if (tip_r   < 2) tip_r   = 2;
    if (hub_r   < 2) hub_r   = 2;

    GfxDrawCircle(fb, cx, cy, &(GfxCircle){ .radius = body_r_outer, .color = fg });
    GfxDrawCircle(fb, cx, cy, &(GfxCircle){ .radius = body_r_inner, .color = fg });

    GfxFillRect(fb, cx - tooth_w / 2,        cy - body_r_outer - tooth_h, tooth_w, tooth_h, fg);
    GfxFillRect(fb, cx - tooth_w / 2,        cy + body_r_outer,           tooth_w, tooth_h, fg);
    GfxFillRect(fb, cx - body_r_outer - tooth_h, cy - tooth_w / 2,        tooth_h, tooth_w, fg);
    GfxFillRect(fb, cx + body_r_outer,           cy - tooth_w / 2,        tooth_h, tooth_w, fg);

    int diag = ((body_r_outer + tooth_h / 2) * 707) / 1000;
    GfxDrawCircle(fb, cx + diag, cy - diag, &(GfxCircle){ .radius = tip_r, .fill = 1, .color = fg });
    GfxDrawCircle(fb, cx - diag, cy - diag, &(GfxCircle){ .radius = tip_r, .fill = 1, .color = fg });
    GfxDrawCircle(fb, cx + diag, cy + diag, &(GfxCircle){ .radius = tip_r, .fill = 1, .color = fg });
    GfxDrawCircle(fb, cx - diag, cy + diag, &(GfxCircle){ .radius = tip_r, .fill = 1, .color = fg });

    GfxDrawCircle(fb, cx, cy, &(GfxCircle){ .radius = hub_r, .color = fg });
}

void icon_clock(GfxFb *fb, int x, int y, int h, GfxColor fg, GfxColor bg, void *data)
{
    (void)bg; (void)data;
    int cx = x + h / 2;
    int cy = y + h / 2;
    int r  = h * 9 / 20;   /* face radius — 5% margin each side */

    GfxDrawCircle(fb, cx, cy, &(GfxCircle){ .radius = r, .color = fg });

    /* 10:10 pose. Angles measured clockwise from 12 o'clock.
     * Screen coords: Δx = r·sin(θ), Δy = -r·cos(θ).
     * Minute hand: 60°  → sin=866/1000, cos=500/1000  (→ upper-right)
     * Hour hand:  305°  → sin=-819/1000, cos=574/1000 (→ upper-left)  */
    int mr = r * 80 / 100;
    int hr = r * 55 / 100;
    GfxLine(fb, cx, cy, cx +  866*mr/1000, cy - 500*mr/1000, fg);
    GfxLine(fb, cx, cy, cx - 819*hr/1000, cy - 574*hr/1000, fg);

    GfxFillRect(fb, cx - 1, cy - 1, 3, 3, fg);
}
