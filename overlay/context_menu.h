#ifndef MASKO_CONTEXT_MENU_H
#define MASKO_CONTEXT_MENU_H

#include <X11/Xlib.h>
#include <cairo/cairo.h>
#include <cjson/cJSON.h>

#define MENU_MAX_ITEMS 10

typedef struct {
    char label[64];
    char action[64];  /* sent via IPC as {"event":"menu","action":"..."} */
    int separator_after;
} MenuItem;

typedef struct {
    int visible;
    MenuItem items[MENU_MAX_ITEMS];
    int count;
    int hovered;
    int width;
    int item_height;
    /* Popup window */
    Display *dpy;
    Window popup_win;
    Visual *visual;
    Colormap colormap;
    cairo_surface_t *surface;
    cairo_t *cr;
    int popup_w, popup_h;
    int popup_mapped;
} ContextMenu;

void context_menu_init(ContextMenu *m);
void context_menu_create_popup(ContextMenu *m, Display *dpy, int screen);
void context_menu_destroy_popup(ContextMenu *m);
void context_menu_show_at(ContextMenu *m, int screen_x, int screen_y);
void context_menu_hide(ContextMenu *m);
void context_menu_redraw(ContextMenu *m);
/* Returns item index or -1; coords are popup-local */
int context_menu_hit_test(ContextMenu *m, int x, int y);
void context_menu_hover(ContextMenu *m, int x, int y);
/* Replace menu items from a JSON array received via IPC */
void context_menu_set_items(ContextMenu *m, cJSON *items);

#endif
