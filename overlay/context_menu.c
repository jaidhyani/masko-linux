#include "context_menu.h"
#include "render.h"
#include <string.h>
#include <stdio.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <cairo/cairo-xlib.h>

#define MENU_WIDTH     200
#define MENU_ITEM_H     32
#define MENU_PADDING     8
#define MENU_RADIUS     10
#define SEPARATOR_H      1

static void add_item(ContextMenu *m, const char *label, const char *action,
                     int separator_after)
{
    if (m->count >= MENU_MAX_ITEMS) return;
    MenuItem *it = &m->items[m->count++];
    strncpy(it->label, label, sizeof(it->label) - 1);
    it->label[sizeof(it->label) - 1] = '\0';
    strncpy(it->action, action, sizeof(it->action) - 1);
    it->action[sizeof(it->action) - 1] = '\0';
    it->separator_after = separator_after;
}

static int menu_total_height(ContextMenu *m)
{
    int h = MENU_PADDING * 2;
    for (int i = 0; i < m->count; i++) {
        h += m->item_height;
        if (m->items[i].separator_after)
            h += MENU_PADDING + SEPARATOR_H;
    }
    return h;
}

void context_menu_init(ContextMenu *m)
{
    memset(m, 0, sizeof(*m));
    m->hovered = -1;
    m->width = MENU_WIDTH;
    m->item_height = MENU_ITEM_H;
}

void context_menu_set_items(ContextMenu *m, cJSON *items)
{
    m->count = 0;
    if (!items || !cJSON_IsArray(items)) return;

    cJSON *item;
    cJSON_ArrayForEach(item, items) {
        cJSON *jid    = cJSON_GetObjectItem(item, "id");
        cJSON *jlabel = cJSON_GetObjectItem(item, "label");
        cJSON *jsep   = cJSON_GetObjectItem(item, "separator_after");

        const char *id    = (jid && cJSON_IsString(jid))       ? jid->valuestring    : "";
        const char *label = (jlabel && cJSON_IsString(jlabel)) ? jlabel->valuestring : "";
        int sep = (jsep && cJSON_IsTrue(jsep)) ? 1 : 0;

        add_item(m, label, id, sep);
    }

    /* Resize the popup window to fit new items */
    if (m->dpy && m->popup_win) {
        m->popup_w = m->width + 4;
        m->popup_h = menu_total_height(m) + 4;
        XResizeWindow(m->dpy, m->popup_win, m->popup_w, m->popup_h);
        if (m->surface) {
            cairo_xlib_surface_set_size(m->surface, m->popup_w, m->popup_h);
        }
    }
}

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

void context_menu_create_popup(ContextMenu *m, Display *dpy, int screen)
{
    m->dpy = dpy;
    m->popup_w = m->width + 4;
    m->popup_h = menu_total_height(m) + 4;

    m->visual = find_argb_visual(dpy, screen);
    if (!m->visual) {
        fprintf(stderr, "context_menu: no 32-bit ARGB visual\n");
        return;
    }

    m->colormap = XCreateColormap(dpy, RootWindow(dpy, screen),
                                  m->visual, AllocNone);

    XSetWindowAttributes attrs = {
        .override_redirect = True,
        .colormap          = m->colormap,
        .border_pixel      = 0,
        .background_pixel  = 0,
        .event_mask        = ExposureMask | ButtonPressMask |
                             ButtonReleaseMask | PointerMotionMask |
                             LeaveWindowMask,
    };

    m->popup_win = XCreateWindow(
        dpy, RootWindow(dpy, screen),
        0, 0, m->popup_w, m->popup_h, 0,
        32, InputOutput, m->visual,
        CWOverrideRedirect | CWColormap | CWBorderPixel |
        CWBackPixel | CWEventMask,
        &attrs);

    /* Keep popup above other windows */
    Atom wm_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    Atom type_dock = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
    XChangeProperty(dpy, m->popup_win, wm_type, XA_ATOM, 32,
                    PropModeReplace, (unsigned char *)&type_dock, 1);

    Atom wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
    Atom state_above = XInternAtom(dpy, "_NET_WM_STATE_ABOVE", False);
    XChangeProperty(dpy, m->popup_win, wm_state, XA_ATOM, 32,
                    PropModeReplace, (unsigned char *)&state_above, 1);

    XStoreName(dpy, m->popup_win, "masko-menu");

    m->surface = cairo_xlib_surface_create(dpy, m->popup_win,
                                           m->visual, m->popup_w, m->popup_h);
    m->cr = cairo_create(m->surface);
}

void context_menu_destroy_popup(ContextMenu *m)
{
    if (m->cr)       cairo_destroy(m->cr);
    if (m->surface)  cairo_surface_destroy(m->surface);
    if (m->popup_win) XDestroyWindow(m->dpy, m->popup_win);
    if (m->colormap) XFreeColormap(m->dpy, m->colormap);
    m->cr = NULL;
    m->surface = NULL;
    m->popup_win = 0;
    m->colormap = 0;
}

void context_menu_show_at(ContextMenu *m, int screen_x, int screen_y)
{
    m->visible = 1;
    m->hovered = -1;
    m->popup_mapped = 1;

    XMoveWindow(m->dpy, m->popup_win, screen_x, screen_y);
    XMapRaised(m->dpy, m->popup_win);

    /* Grab pointer so clicks outside the popup are caught */
    XGrabPointer(m->dpy, m->popup_win, True,
                 ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                 GrabModeAsync, GrabModeAsync, None, None, CurrentTime);

    XFlush(m->dpy);
    context_menu_redraw(m);
}

void context_menu_hide(ContextMenu *m)
{
    m->visible = 0;
    m->hovered = -1;
    if (m->popup_mapped) {
        XUngrabPointer(m->dpy, CurrentTime);
        XUnmapWindow(m->dpy, m->popup_win);
        XFlush(m->dpy);
        m->popup_mapped = 0;
    }
}

void context_menu_redraw(ContextMenu *m)
{
    if (!m->cr) return;

    int total_h = menu_total_height(m);
    int w = m->width;

    /* Clear to transparent */
    cairo_set_operator(m->cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(m->cr);
    cairo_set_operator(m->cr, CAIRO_OPERATOR_OVER);

    /* Dark panel background */
    render_rounded_rect(m->cr, 2, 2, w, total_h, MENU_RADIUS);
    cairo_set_source_rgba(m->cr, 0.12, 0.12, 0.14, 0.95);
    cairo_fill(m->cr);

    /* Border */
    render_rounded_rect(m->cr, 2, 2, w, total_h, MENU_RADIUS);
    cairo_set_source_rgba(m->cr, 0.3, 0.3, 0.35, 0.6);
    cairo_set_line_width(m->cr, 1.0);
    cairo_stroke(m->cr);

    int iy = 2 + MENU_PADDING;

    for (int i = 0; i < m->count; i++) {
        if (i == m->hovered) {
            double hx = 2 + 4;
            double hw = w - 8;
            render_rounded_rect(m->cr, hx, iy, hw, m->item_height, 6.0);
            cairo_set_source_rgba(m->cr, 0.3, 0.3, 0.35, 0.7);
            cairo_fill(m->cr);
        }

        double text_x = 2 + 14;
        double text_y = iy + (m->item_height - 14) / 2.0;
        render_text(m->cr, m->items[i].label,
                    text_x, text_y, w - 28,
                    "Sans", 12,
                    0.9, 0.9, 0.9, 1.0);

        iy += m->item_height;

        if (m->items[i].separator_after) {
            int sep_y = iy + MENU_PADDING / 2;
            cairo_set_source_rgba(m->cr, 0.4, 0.4, 0.45, 0.5);
            cairo_rectangle(m->cr, 2 + 12, sep_y, w - 24, SEPARATOR_H);
            cairo_fill(m->cr);
            iy += MENU_PADDING + SEPARATOR_H;
        }
    }

    cairo_surface_flush(m->surface);
    XFlush(m->dpy);
}

int context_menu_hit_test(ContextMenu *m, int x, int y)
{
    if (!m->visible) return -1;

    int iy = 2 + MENU_PADDING;

    for (int i = 0; i < m->count; i++) {
        if (x >= 2 && x < 2 + m->width &&
            y >= iy && y < iy + m->item_height) {
            return i;
        }
        iy += m->item_height;
        if (m->items[i].separator_after)
            iy += MENU_PADDING + SEPARATOR_H;
    }
    return -1;
}

void context_menu_hover(ContextMenu *m, int x, int y)
{
    int prev = m->hovered;
    m->hovered = context_menu_hit_test(m, x, y);
    if (m->hovered != prev)
        context_menu_redraw(m);
}
