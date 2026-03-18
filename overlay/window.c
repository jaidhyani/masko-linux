#include "window.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/shape.h>
#include <cairo/cairo-xlib.h>

static Visual *find_argb_visual(Display *dpy, int screen)
{
    XVisualInfo tpl = { .screen = screen, .depth = 32, .class = TrueColor };
    int count = 0;
    XVisualInfo *infos = XGetVisualInfo(dpy,
        VisualScreenMask | VisualDepthMask | VisualClassMask,
        &tpl, &count);
    if (!infos || count == 0)
        return NULL;

    Visual *result = infos[0].visual;
    XFree(infos);
    return result;
}

static void set_atom_property(Display *dpy, Window win,
                              const char *prop, const char *value)
{
    Atom prop_atom  = XInternAtom(dpy, prop, False);
    Atom value_atom = XInternAtom(dpy, value, False);
    XChangeProperty(dpy, win, prop_atom, XA_ATOM, 32,
                    PropModeReplace, (unsigned char *)&value_atom, 1);
}

static void recreate_buffers(MaskoWindow *w)
{
    if (w->back_cr)       cairo_destroy(w->back_cr);
    if (w->back_surface)  cairo_surface_destroy(w->back_surface);
    if (w->front_cr)      cairo_destroy(w->front_cr);
    if (w->front_surface) cairo_surface_destroy(w->front_surface);

    w->front_surface = cairo_xlib_surface_create(
        w->dpy, w->win, w->visual, w->width, w->height);
    w->front_cr = cairo_create(w->front_surface);

    w->back_surface = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32, w->width, w->height);
    w->back_cr = cairo_create(w->back_surface);
}

int masko_window_init(MaskoWindow *w, int x, int y, int width, int height)
{
    memset(w, 0, sizeof(*w));

    w->dpy = XOpenDisplay(NULL);
    if (!w->dpy) {
        fprintf(stderr, "masko-overlay: cannot open X display\n");
        return -1;
    }

    w->screen = DefaultScreen(w->dpy);

    int comp_event, comp_error;
    if (!XCompositeQueryExtension(w->dpy, &comp_event, &comp_error)) {
        fprintf(stderr,
            "masko-overlay: X Composite extension not available.\n"
            "Install a compositor (e.g. picom, compton) or enable compositing "
            "in your desktop environment.\n");
        XCloseDisplay(w->dpy);
        exit(1);
    }

    w->visual = find_argb_visual(w->dpy, w->screen);
    if (!w->visual) {
        fprintf(stderr, "masko-overlay: no 32-bit ARGB visual found\n");
        XCloseDisplay(w->dpy);
        return -1;
    }

    w->colormap = XCreateColormap(w->dpy, RootWindow(w->dpy, w->screen),
                                  w->visual, AllocNone);

    XSetWindowAttributes attrs = {
        .override_redirect = True,
        .colormap          = w->colormap,
        .border_pixel      = 0,
        .background_pixel  = 0,
        .event_mask        = ExposureMask | ButtonPressMask |
                             ButtonReleaseMask | PointerMotionMask |
                             StructureNotifyMask,
    };

    w->x      = x;
    w->y      = y;
    w->width  = width;
    w->height = height;

    w->win = XCreateWindow(
        w->dpy, RootWindow(w->dpy, w->screen),
        x, y, width, height, 0,
        32, InputOutput, w->visual,
        CWOverrideRedirect | CWColormap | CWBorderPixel |
        CWBackPixel | CWEventMask,
        &attrs);

    set_atom_property(w->dpy, w->win,
        "_NET_WM_WINDOW_TYPE", "_NET_WM_WINDOW_TYPE_DOCK");
    set_atom_property(w->dpy, w->win,
        "_NET_WM_STATE", "_NET_WM_STATE_ABOVE");

    XStoreName(w->dpy, w->win, "masko-overlay");

    recreate_buffers(w);
    return 0;
}

void masko_window_destroy(MaskoWindow *w)
{
    if (w->back_cr)       cairo_destroy(w->back_cr);
    if (w->back_surface)  cairo_surface_destroy(w->back_surface);
    if (w->front_cr)      cairo_destroy(w->front_cr);
    if (w->front_surface) cairo_surface_destroy(w->front_surface);
    if (w->win)           XDestroyWindow(w->dpy, w->win);
    if (w->colormap)      XFreeColormap(w->dpy, w->colormap);
    if (w->dpy)           XCloseDisplay(w->dpy);
    memset(w, 0, sizeof(*w));
}

void masko_window_show(MaskoWindow *w)
{
    XMapWindow(w->dpy, w->win);
    XFlush(w->dpy);
}

void masko_window_hide(MaskoWindow *w)
{
    XUnmapWindow(w->dpy, w->win);
    XFlush(w->dpy);
}

void masko_window_move(MaskoWindow *w, int x, int y)
{
    w->x = x;
    w->y = y;
    XMoveWindow(w->dpy, w->win, x, y);
    XFlush(w->dpy);
}

void masko_window_resize(MaskoWindow *w, int width, int height)
{
    w->width  = width;
    w->height = height;
    XResizeWindow(w->dpy, w->win, width, height);
    recreate_buffers(w);
    XFlush(w->dpy);
}

#define SHAPE_BLOCK 8
#define ALPHA_THRESHOLD 10

static void update_input_shape(MaskoWindow *w)
{
    cairo_surface_flush(w->back_surface);
    unsigned char *data = cairo_image_surface_get_data(w->back_surface);
    int stride = cairo_image_surface_get_stride(w->back_surface);

    int cols = (w->width  + SHAPE_BLOCK - 1) / SHAPE_BLOCK;
    int rows = (w->height + SHAPE_BLOCK - 1) / SHAPE_BLOCK;
    int max_rects = cols * rows;

    XRectangle *rects = malloc(max_rects * sizeof(XRectangle));
    if (!rects) return;
    int n = 0;

    for (int by = 0; by < rows; by++) {
        int py = by * SHAPE_BLOCK;
        int bh = SHAPE_BLOCK;
        if (py + bh > w->height) bh = w->height - py;

        for (int bx = 0; bx < cols; bx++) {
            int px = bx * SHAPE_BLOCK;
            int bw = SHAPE_BLOCK;
            if (px + bw > w->width) bw = w->width - px;

            int has_opaque = 0;
            for (int y = py; y < py + bh && !has_opaque; y++) {
                unsigned char *row = data + y * stride + px * 4;
                for (int x = 0; x < bw; x++) {
                    /* ARGB32: bytes are [B, G, R, A] on little-endian */
                    if (row[x * 4 + 3] > ALPHA_THRESHOLD) {
                        has_opaque = 1;
                        break;
                    }
                }
            }

            if (has_opaque) {
                rects[n].x      = px;
                rects[n].y      = py;
                rects[n].width  = bw;
                rects[n].height = bh;
                n++;
            }
        }
    }

    XShapeCombineRectangles(w->dpy, w->win, ShapeInput,
                            0, 0, rects, n, ShapeSet, YXBanded);
    free(rects);
}

void masko_window_flip(MaskoWindow *w)
{
    cairo_set_operator(w->front_cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(w->front_cr, w->back_surface, 0, 0);
    cairo_paint(w->front_cr);
    cairo_surface_flush(w->front_surface);
    update_input_shape(w);
    XFlush(w->dpy);
}

cairo_t *masko_window_back_cr(MaskoWindow *w)
{
    return w->back_cr;
}

int masko_window_fd(MaskoWindow *w)
{
    return ConnectionNumber(w->dpy);
}
