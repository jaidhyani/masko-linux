/* Stub render functions for unit tests that don't test drawing */
#include "../render.h"

void render_rounded_rect(cairo_t *cr, double x, double y,
                         double w, double h, double radius)
{
    (void)cr; (void)x; (void)y; (void)w; (void)h; (void)radius;
}

void render_text(cairo_t *cr, const char *text,
                 double x, double y, double max_width,
                 const char *font, double size,
                 double r, double g, double b, double a)
{
    (void)cr; (void)text; (void)x; (void)y; (void)max_width;
    (void)font; (void)size; (void)r; (void)g; (void)b; (void)a;
}

void render_button(cairo_t *cr, const char *label,
                   double x, double y, double w, double h,
                   double bg_r, double bg_g, double bg_b,
                   int hovered)
{
    (void)cr; (void)label; (void)x; (void)y; (void)w; (void)h;
    (void)bg_r; (void)bg_g; (void)bg_b; (void)hovered;
}
