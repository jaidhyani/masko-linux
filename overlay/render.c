#include "render.h"
#include <pango/pangocairo.h>
#include <stdio.h>

void render_rounded_rect(cairo_t *cr, double x, double y,
                         double w, double h, double radius)
{
    double r = radius;
    if (r > w / 2.0) r = w / 2.0;
    if (r > h / 2.0) r = h / 2.0;

    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r,     r, -G_PI / 2, 0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0,          G_PI / 2);
    cairo_arc(cr, x + r,     y + h - r, r, G_PI / 2,   G_PI);
    cairo_arc(cr, x + r,     y + r,     r, G_PI,        3 * G_PI / 2);
    cairo_close_path(cr);
}

void render_text(cairo_t *cr, const char *text,
                 double x, double y, double max_width,
                 const char *font, double size,
                 double r, double g, double b, double a)
{
    PangoLayout *layout = pango_cairo_create_layout(cr);

    char font_desc[256];
    snprintf(font_desc, sizeof(font_desc), "%s %.0f", font, size);
    PangoFontDescription *desc = pango_font_description_from_string(font_desc);
    pango_layout_set_font_description(layout, desc);
    pango_font_description_free(desc);

    if (max_width > 0) {
        pango_layout_set_width(layout, (int)(max_width * PANGO_SCALE));
        pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
    }

    pango_layout_set_text(layout, text, -1);

    cairo_set_source_rgba(cr, r, g, b, a);
    cairo_move_to(cr, x, y);
    pango_cairo_show_layout(cr, layout);

    g_object_unref(layout);
}

void render_button(cairo_t *cr, const char *label,
                   double x, double y, double w, double h,
                   double bg_r, double bg_g, double bg_b,
                   int hovered)
{
    double lighten = hovered ? 0.15 : 0.0;
    double fr = bg_r + lighten;
    double fg = bg_g + lighten;
    double fb = bg_b + lighten;
    if (fr > 1.0) fr = 1.0;
    if (fg > 1.0) fg = 1.0;
    if (fb > 1.0) fb = 1.0;

    render_rounded_rect(cr, x, y, w, h, 6.0);
    cairo_set_source_rgba(cr, fr, fg, fb, 1.0);
    cairo_fill(cr);

    /* Center text inside the button */
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *desc = pango_font_description_from_string("Sans Bold 12");
    pango_layout_set_font_description(layout, desc);
    pango_font_description_free(desc);
    pango_layout_set_text(layout, label, -1);

    int tw, th;
    pango_layout_get_pixel_size(layout, &tw, &th);

    double tx = x + (w - tw) / 2.0;
    double ty = y + (h - th) / 2.0;

    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
    cairo_move_to(cr, tx, ty);
    pango_cairo_show_layout(cr, layout);

    g_object_unref(layout);
}
