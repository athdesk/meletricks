#ifndef SCREENS_SCREEN_LIFE_H
#define SCREENS_SCREEN_LIFE_H

#include "chrome.h"

extern GfxScreen  s_life_scr;
extern GfxScreen  s_life_settings_scr;
extern GfxWidget *s_life_settings_menu;
extern GfxWidget *s_life_widget;

void build_life(void);
int  life_spawn_get(void);

#endif /* SCREENS_SCREEN_LIFE_H */
