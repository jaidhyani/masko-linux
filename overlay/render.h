#ifndef MASKO_RENDER_H
#define MASKO_RENDER_H

#include <cairo/cairo.h>

void render_rounded_rect(cairo_t *cr, double x, double y,
                         double w, double h, double radius);

void render_text(cairo_t *cr, const char *text,
                 double x, double y, double max_width,
                 const char *font, double size,
                 double r, double g, double b, double a);

void render_button(cairo_t *cr, const char *label,
                   double x, double y, double w, double h,
                   double bg_r, double bg_g, double bg_b,
                   int hovered);

#endif
