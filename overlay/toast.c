#include "toast.h"
#include "render.h"
#include <stdio.h>
#include <string.h>

void toast_init(ToastUI *t)
{
    memset(t, 0, sizeof(*t));
}

void toast_show(ToastUI *t, const char *text, int duration_ms)
{
    t->visible = 1;
    snprintf(t->text, sizeof(t->text), "%s", text);
    t->duration_ms = duration_ms;
    t->elapsed_ms = 0;
}

void toast_hide(ToastUI *t)
{
    t->visible = 0;
}

void toast_draw(ToastUI *t, cairo_t *cr, int win_w, int win_h)
{
    if (!t->visible)
        return;

    double pad = 12.0;
    double toast_h = 36.0;
    double toast_w = win_w * 0.7;
    double toast_x = (win_w - toast_w) / 2.0;
    double toast_y = win_h - toast_h - pad;

    render_rounded_rect(cr, toast_x, toast_y, toast_w, toast_h, 8.0);
    cairo_set_source_rgba(cr, 0.1, 0.1, 0.15, 0.9);
    cairo_fill(cr);

    double text_pad = 10.0;
    render_text(cr, t->text,
                toast_x + text_pad, toast_y + 8,
                toast_w - 2 * text_pad,
                "Sans", 11,
                1.0, 1.0, 1.0, 1.0);
}

int toast_tick(ToastUI *t, int delta_ms)
{
    if (!t->visible)
        return 0;

    t->elapsed_ms += delta_ms;
    if (t->elapsed_ms >= t->duration_ms) {
        t->visible = 0;
        return 0;
    }
    return 1;
}
