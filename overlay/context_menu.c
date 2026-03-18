#include "context_menu.h"
#include "render.h"
#include <string.h>

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

void context_menu_init(ContextMenu *m)
{
    memset(m, 0, sizeof(*m));
    m->hovered = -1;
    m->width = MENU_WIDTH;
    m->item_height = MENU_ITEM_H;

    add_item(m, "Dashboard",        "open_dashboard",  1);
    add_item(m, "Change Mascot...", "open_settings",   0);
    add_item(m, "Reset Position",   "reset_position",  1);
    add_item(m, "Hide Mascot",      "toggle",          0);
    add_item(m, "Quit",             "quit",            0);
}

void context_menu_show(ContextMenu *m, int x, int y)
{
    m->visible = 1;
    m->x = x;
    m->y = y;
    m->hovered = -1;
}

void context_menu_hide(ContextMenu *m)
{
    m->visible = 0;
    m->hovered = -1;
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

void context_menu_draw(ContextMenu *m, cairo_t *cr, int win_x, int win_y)
{
    (void)win_x;
    (void)win_y;

    if (!m->visible) return;

    int total_h = menu_total_height(m);

    /* Dark panel background */
    render_rounded_rect(cr, m->x, m->y, m->width, total_h, MENU_RADIUS);
    cairo_set_source_rgba(cr, 0.12, 0.12, 0.14, 0.95);
    cairo_fill(cr);

    /* Border */
    render_rounded_rect(cr, m->x, m->y, m->width, total_h, MENU_RADIUS);
    cairo_set_source_rgba(cr, 0.3, 0.3, 0.35, 0.6);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    int iy = m->y + MENU_PADDING;

    for (int i = 0; i < m->count; i++) {
        /* Hover highlight */
        if (i == m->hovered) {
            double hx = m->x + 4;
            double hw = m->width - 8;
            render_rounded_rect(cr, hx, iy, hw, m->item_height, 6.0);
            cairo_set_source_rgba(cr, 0.3, 0.3, 0.35, 0.7);
            cairo_fill(cr);
        }

        double text_x = m->x + 14;
        double text_y = iy + (m->item_height - 14) / 2.0;
        render_text(cr, m->items[i].label,
                    text_x, text_y, m->width - 28,
                    "Sans", 12,
                    0.9, 0.9, 0.9, 1.0);

        iy += m->item_height;

        if (m->items[i].separator_after) {
            int sep_y = iy + MENU_PADDING / 2;
            cairo_set_source_rgba(cr, 0.4, 0.4, 0.45, 0.5);
            cairo_rectangle(cr, m->x + 12, sep_y, m->width - 24, SEPARATOR_H);
            cairo_fill(cr);
            iy += MENU_PADDING + SEPARATOR_H;
        }
    }
}

int context_menu_hit_test(ContextMenu *m, int mouse_x, int mouse_y,
                          int win_x, int win_y)
{
    (void)win_x;
    (void)win_y;

    if (!m->visible) return -1;

    int iy = m->y + MENU_PADDING;

    for (int i = 0; i < m->count; i++) {
        if (mouse_x >= m->x && mouse_x < m->x + m->width &&
            mouse_y >= iy && mouse_y < iy + m->item_height) {
            return i;
        }
        iy += m->item_height;
        if (m->items[i].separator_after)
            iy += MENU_PADDING + SEPARATOR_H;
    }
    return -1;
}

void context_menu_hover(ContextMenu *m, int mouse_x, int mouse_y,
                        int win_x, int win_y)
{
    m->hovered = context_menu_hit_test(m, mouse_x, mouse_y, win_x, win_y);
}
