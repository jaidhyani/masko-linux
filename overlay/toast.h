#ifndef MASKO_TOAST_H
#define MASKO_TOAST_H

#include <cairo/cairo.h>

typedef struct {
    int visible;
    char text[512];
    int duration_ms;
    int elapsed_ms;
} ToastUI;

void toast_init(ToastUI *t);
void toast_show(ToastUI *t, const char *text, int duration_ms);
void toast_hide(ToastUI *t);
void toast_draw(ToastUI *t, cairo_t *cr, int win_w, int win_h);

/* Returns 1 if still visible after tick, 0 if expired */
int toast_tick(ToastUI *t, int delta_ms);

#endif
