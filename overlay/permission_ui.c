#include "permission_ui.h"
#include "render.h"
#include <stdio.h>
#include <string.h>

void permission_ui_init(PermissionUI *ui)
{
    memset(ui, 0, sizeof(*ui));
}

void permission_ui_show(PermissionUI *ui, const char *tool,
                        const char *input, const char *id)
{
    ui->visible = 1;
    snprintf(ui->tool_name,  sizeof(ui->tool_name),  "%s", tool);
    snprintf(ui->tool_input, sizeof(ui->tool_input), "%s", input);
    snprintf(ui->perm_id,    sizeof(ui->perm_id),    "%s", id);
    ui->hover_approve = 0;
    ui->hover_deny = 0;
}

void permission_ui_hide(PermissionUI *ui)
{
    ui->visible = 0;
}

void permission_ui_draw(PermissionUI *ui, cairo_t *cr, int win_w, int win_h)
{
    if (!ui->visible)
        return;

    double pad = 16.0;
    double panel_x = pad;
    double panel_y = win_h / 2.0;
    double panel_w = win_w - 2 * pad;
    double panel_h = win_h / 2.0 - pad;

    /* Panel background */
    render_rounded_rect(cr, panel_x, panel_y, panel_w, panel_h, 12.0);
    cairo_set_source_rgba(cr, 0.1, 0.1, 0.15, 0.95);
    cairo_fill(cr);

    double cx = panel_x + pad;
    double cy = panel_y + pad;
    double content_w = panel_w - 2 * pad;

    /* Title */
    render_text(cr, "Permission Request", cx, cy, content_w,
                "Sans Bold", 14, 1.0, 1.0, 1.0, 1.0);
    cy += 28;

    /* Tool name badge */
    double badge_h = 24.0;
    double badge_pad = 8.0;
    render_rounded_rect(cr, cx, cy, content_w, badge_h, 4.0);
    cairo_set_source_rgba(cr, 0.2, 0.3, 0.6, 1.0);
    cairo_fill(cr);
    render_text(cr, ui->tool_name, cx + badge_pad, cy + 3, content_w - 2 * badge_pad,
                "Sans Bold", 11, 1.0, 1.0, 1.0, 1.0);
    cy += badge_h + 10;

    /* Tool input text */
    render_text(cr, ui->tool_input, cx, cy, content_w,
                "Sans", 10, 0.8, 0.8, 0.85, 1.0);
    cy += 40;

    /* Buttons */
    double btn_w = (content_w - pad) / 2.0;
    double btn_h = 32.0;
    double btn_y = panel_y + panel_h - pad - btn_h;

    ui->approve_x = cx;
    ui->approve_y = btn_y;
    ui->approve_w = btn_w;
    ui->approve_h = btn_h;

    ui->deny_x = cx + btn_w + pad;
    ui->deny_y = btn_y;
    ui->deny_w = btn_w;
    ui->deny_h = btn_h;

    render_button(cr, "Approve",
                  ui->approve_x, ui->approve_y, ui->approve_w, ui->approve_h,
                  0.2, 0.7, 0.3, ui->hover_approve);

    render_button(cr, "Deny",
                  ui->deny_x, ui->deny_y, ui->deny_w, ui->deny_h,
                  0.8, 0.2, 0.2, ui->hover_deny);
}

static int point_in_rect(int px, int py,
                         double rx, double ry, double rw, double rh)
{
    return px >= rx && px <= rx + rw && py >= ry && py <= ry + rh;
}

int permission_ui_hit_test(PermissionUI *ui, int x, int y)
{
    if (!ui->visible)
        return 0;
    if (point_in_rect(x, y, ui->approve_x, ui->approve_y, ui->approve_w, ui->approve_h))
        return 1;
    if (point_in_rect(x, y, ui->deny_x, ui->deny_y, ui->deny_w, ui->deny_h))
        return 2;
    return 0;
}

void permission_ui_hover(PermissionUI *ui, int x, int y)
{
    if (!ui->visible)
        return;
    ui->hover_approve = point_in_rect(x, y, ui->approve_x, ui->approve_y,
                                      ui->approve_w, ui->approve_h);
    ui->hover_deny = point_in_rect(x, y, ui->deny_x, ui->deny_y,
                                   ui->deny_w, ui->deny_h);
}
