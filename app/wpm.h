#ifndef WPM_H
#define WPM_H

#include "gfx.h"

#define WPM_HISTORY_LEN  600   /* 10 minutes at 1 sample/sec */

extern int wpm_history[WPM_HISTORY_LEN];

void wpm_record(u32 now_ms);
u32  wpm_current(void);

/* Per-loop tick: shifts the history buffer once a second and dirties
 * the registered graph widget (if any). Runs regardless of screen so
 * the buffer keeps filling in the background. */
void wpm_history_tick(void);
void wpm_set_graph_widget(GfxWidget *w);

#endif
