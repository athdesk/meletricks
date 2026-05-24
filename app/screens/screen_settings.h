#ifndef SCREENS_SCREEN_SETTINGS_H
#define SCREENS_SCREEN_SETTINGS_H

#include "chrome.h"

extern GfxScreen  s_settings_scr;
extern GfxScreen  s_debug_scr;
extern GfxScreen  s_about_scr;
extern GfxWidget *s_settings_menu;
extern GfxWidget *s_debug_menu;

void build_settings_screens(void);

#endif /* SCREENS_SCREEN_SETTINGS_H */
