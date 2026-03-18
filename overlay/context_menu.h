#ifndef MASKO_CONTEXT_MENU_H
#define MASKO_CONTEXT_MENU_H

#include <cairo/cairo.h>

#define MENU_MAX_ITEMS 10

typedef struct {
    char label[64];
    char action[64];  /* sent via IPC as {"event":"menu","action":"..."} */
    int separator_after;
} MenuItem;

typedef struct {
    int visible;
    int x, y;       /* screen position of menu */
    MenuItem items[MENU_MAX_ITEMS];
    int count;
    int hovered;    /* -1 = none */
    int width;
    int item_height;
} ContextMenu;

void context_menu_init(ContextMenu *m);
void context_menu_show(ContextMenu *m, int x, int y);
void context_menu_hide(ContextMenu *m);
void context_menu_draw(ContextMenu *m, cairo_t *cr, int win_x, int win_y);
/* Returns item index or -1 */
int context_menu_hit_test(ContextMenu *m, int mouse_x, int mouse_y, int win_x, int win_y);
void context_menu_hover(ContextMenu *m, int mouse_x, int mouse_y, int win_x, int win_y);

#endif
