#ifndef MASKO_SESSION_SWITCHER_H
#define MASKO_SESSION_SWITCHER_H

#include <cairo/cairo.h>
#include <cjson/cJSON.h>

#define MAX_SESSIONS 32

typedef struct {
    char id[128];
    char cwd[512];
    char model[64];
    char phase[32];
} SwitcherSession;

typedef struct {
    int visible;
    SwitcherSession sessions[MAX_SESSIONS];
    int count;
    int selected;
} SessionSwitcherUI;

void session_switcher_init(SessionSwitcherUI *sw);
void session_switcher_show(SessionSwitcherUI *sw, cJSON *sessions_json, int initial_selected);
void session_switcher_hide(SessionSwitcherUI *sw);
void session_switcher_nav(SessionSwitcherUI *sw, const char *direction);
void session_switcher_draw(SessionSwitcherUI *sw, cairo_t *cr, int win_w, int win_h);

/* Returns session index if clicked on a row, -1 otherwise */
int session_switcher_hit_test(SessionSwitcherUI *sw, int x, int y, int win_w, int win_h);

#endif
