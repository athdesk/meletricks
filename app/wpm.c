#include "wpm.h"
#include "timer.h"

/* Span-based WPM:
 *   wpm = (N - 1) * 12000 / (now - oldest_ms)
 * where 12000 = 60000 ms/min / 5 chars/word. Short ring = reactive
 * to bursts and to stopping; long ring = smoother readout. */
#define WPM_RING_LEN  8u
/* Without an explicit decay the span-based value tapers slowly
 * forever — visible WPM would hang for tens of seconds after the
 * last keystroke. Ramp linearly to 0 over this window then pin. */
#define WPM_DECAY_MS  1000u

static u32 s_times[WPM_RING_LEN];
static u8  s_count;
static u8  s_head;

void wpm_record(u32 now_ms)
{
    s_times[s_head] = now_ms;
    s_head = (u8)((s_head + 1u) % WPM_RING_LEN);
    if (s_count < WPM_RING_LEN) s_count++;
}

u32 wpm_current(void)
{
    if (s_count < 2) return 0;
    u8 oldest = (s_count < WPM_RING_LEN) ? 0 : s_head;
    u8 newest = (u8)((s_head + WPM_RING_LEN - 1u) % WPM_RING_LEN);
    u32 now   = fr_millis();
    u32 idle  = now - s_times[newest];
    if (idle >= WPM_DECAY_MS) return 0;
    u32 span = now - s_times[oldest];
    if (span < 1u) return 0;
    u32 base = (u32)(s_count - 1u) * 12000u / span;
    return base * (WPM_DECAY_MS - idle) / WPM_DECAY_MS;
}

int wpm_history[WPM_HISTORY_LEN];
static u32        s_history_last_ms;
static GfxWidget *s_graph_widget;

void wpm_set_graph_widget(GfxWidget *w) { s_graph_widget = w; }

void wpm_history_tick(void)
{
    u32 now = fr_millis();
    if (s_history_last_ms == 0) { s_history_last_ms = now; return; }
    if (now - s_history_last_ms < 1000u) return;
    s_history_last_ms = now;
    for (int i = 0; i < WPM_HISTORY_LEN - 1; i++) {
        wpm_history[i] = wpm_history[i + 1];
    }
    wpm_history[WPM_HISTORY_LEN - 1] = (int)wpm_current();
    if (s_graph_widget) GfxMarkDirty(s_graph_widget);
}
