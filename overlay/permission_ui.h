#ifndef MASKO_PERMISSION_UI_H
#define MASKO_PERMISSION_UI_H

#include <cairo/cairo.h>

typedef struct {
    int visible;
    char tool_name[256];
    char tool_input[1024];
    char perm_id[256];

    double approve_x, approve_y, approve_w, approve_h;
    double deny_x, deny_y, deny_w, deny_h;
    int hover_approve;
    int hover_deny;
} PermissionUI;

void permission_ui_init(PermissionUI *ui);
void permission_ui_show(PermissionUI *ui, const char *tool,
                        const char *input, const char *id);
void permission_ui_hide(PermissionUI *ui);
void permission_ui_draw(PermissionUI *ui, cairo_t *cr, int win_w, int win_h);

/* Returns: 0=no hit, 1=approve, 2=deny */
int  permission_ui_hit_test(PermissionUI *ui, int x, int y);
void permission_ui_hover(PermissionUI *ui, int x, int y);

#endif
