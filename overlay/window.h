#ifndef MASKO_WINDOW_H
#define MASKO_WINDOW_H

#include <X11/Xlib.h>
#include <cairo/cairo.h>

typedef struct {
    Display *dpy;
    Window   win;
    int      screen;
    Visual  *visual;
    Colormap colormap;

    int width, height;
    int x, y;

    /* Double buffering: draw to back, flip copies to front (X11 surface) */
    cairo_surface_t *front_surface;
    cairo_t         *front_cr;
    cairo_surface_t *back_surface;
    cairo_t         *back_cr;
} MaskoWindow;

int  masko_window_init(MaskoWindow *w, int x, int y, int width, int height);
void masko_window_destroy(MaskoWindow *w);
void masko_window_show(MaskoWindow *w);
void masko_window_hide(MaskoWindow *w);
void masko_window_move(MaskoWindow *w, int x, int y);
void masko_window_resize(MaskoWindow *w, int width, int height);
void masko_window_flip(MaskoWindow *w);

/* Get the back-buffer Cairo context for drawing */
cairo_t *masko_window_back_cr(MaskoWindow *w);

/* X11 connection fd for use with select() */
int masko_window_fd(MaskoWindow *w);

#endif
