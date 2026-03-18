#include "session_switcher.h"
#include "render.h"
#include <stdio.h>
#include <string.h>

#define ROW_HEIGHT 36.0
#define PANEL_PAD  16.0
#define DOT_RADIUS  4.0

void session_switcher_init(SessionSwitcherUI *sw)
{
    memset(sw, 0, sizeof(*sw));
}

static void parse_session(SwitcherSession *s, cJSON *item)
{
    cJSON *jid    = cJSON_GetObjectItem(item, "id");
    cJSON *jcwd   = cJSON_GetObjectItem(item, "cwd");
    cJSON *jmodel = cJSON_GetObjectItem(item, "model");
    cJSON *jphase = cJSON_GetObjectItem(item, "phase");

    snprintf(s->id,    sizeof(s->id),    "%s", jid    && cJSON_IsString(jid)    ? jid->valuestring    : "");
    snprintf(s->cwd,   sizeof(s->cwd),   "%s", jcwd   && cJSON_IsString(jcwd)   ? jcwd->valuestring   : "");
    snprintf(s->model, sizeof(s->model), "%s", jmodel && cJSON_IsString(jmodel) ? jmodel->valuestring : "");
    snprintf(s->phase, sizeof(s->phase), "%s", jphase && cJSON_IsString(jphase) ? jphase->valuestring : "");
}

void session_switcher_show(SessionSwitcherUI *sw, cJSON *sessions_json, int initial_selected)
{
    sw->visible = 1;
    sw->count = 0;
    sw->selected = initial_selected;

    if (!sessions_json || !cJSON_IsArray(sessions_json))
        return;

    int n = cJSON_GetArraySize(sessions_json);
    if (n > MAX_SESSIONS) n = MAX_SESSIONS;

    for (int i = 0; i < n; i++)
        parse_session(&sw->sessions[i], cJSON_GetArrayItem(sessions_json, i));

    sw->count = n;
    if (sw->selected >= n) sw->selected = n - 1;
    if (sw->selected < 0)  sw->selected = 0;
}

void session_switcher_hide(SessionSwitcherUI *sw)
{
    sw->visible = 0;
}

void session_switcher_nav(SessionSwitcherUI *sw, const char *direction)
{
    if (!sw->visible || sw->count == 0) return;

    if (strcmp(direction, "next") == 0) {
        sw->selected = (sw->selected + 1) % sw->count;
    } else if (strcmp(direction, "prev") == 0) {
        sw->selected = (sw->selected - 1 + sw->count) % sw->count;
    }
}

/* Panel geometry helpers */
static void panel_rect(int win_w, int win_h, int row_count,
                        double *px, double *py, double *pw, double *ph)
{
    *pw = win_w - 2 * PANEL_PAD;
    *ph = PANEL_PAD * 2 + ROW_HEIGHT * row_count;
    double max_h = win_h - 2 * PANEL_PAD;
    if (*ph > max_h) *ph = max_h;
    *px = PANEL_PAD;
    *py = (win_h - *ph) / 2.0;
}

static const char *cwd_basename(const char *path)
{
    if (!path || !path[0]) return "";
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

/* Phase string to dot color */
static void phase_color(const char *phase, double *r, double *g, double *b)
{
    if (strcmp(phase, "idle") == 0) {
        *r = 0.5; *g = 0.5; *b = 0.5;
    } else if (strcmp(phase, "thinking") == 0) {
        *r = 1.0; *g = 0.8; *b = 0.2;
    } else if (strcmp(phase, "tool_use") == 0) {
        *r = 0.3; *g = 0.7; *b = 1.0;
    } else {
        *r = 0.2; *g = 0.9; *b = 0.4;
    }
}

void session_switcher_draw(SessionSwitcherUI *sw, cairo_t *cr, int win_w, int win_h)
{
    if (!sw->visible || sw->count == 0)
        return;

    double px, py, pw, ph;
    panel_rect(win_w, win_h, sw->count, &px, &py, &pw, &ph);

    /* Panel background */
    render_rounded_rect(cr, px, py, pw, ph, 12.0);
    cairo_set_source_rgba(cr, 0.1, 0.1, 0.15, 0.95);
    cairo_fill(cr);

    double row_x = px + PANEL_PAD;
    double row_w = pw - 2 * PANEL_PAD;

    for (int i = 0; i < sw->count; i++) {
        double ry = py + PANEL_PAD + i * ROW_HEIGHT;

        /* Selected row highlight */
        if (i == sw->selected) {
            render_rounded_rect(cr, row_x, ry, row_w, ROW_HEIGHT - 2, 6.0);
            cairo_set_source_rgba(cr, 0.25, 0.35, 0.6, 0.8);
            cairo_fill(cr);
        }

        SwitcherSession *s = &sw->sessions[i];

        /* Phase indicator dot */
        double dot_r, dot_g, dot_b;
        phase_color(s->phase, &dot_r, &dot_g, &dot_b);
        double dot_cx = row_x + DOT_RADIUS + 4;
        double dot_cy = ry + ROW_HEIGHT / 2.0;
        cairo_arc(cr, dot_cx, dot_cy, DOT_RADIUS, 0, 2 * 3.14159265);
        cairo_set_source_rgba(cr, dot_r, dot_g, dot_b, 1.0);
        cairo_fill(cr);

        /* cwd basename */
        double text_x = dot_cx + DOT_RADIUS + 8;
        double text_y = ry + 4;
        double label_w = row_w - (text_x - row_x) - 8;
        render_text(cr, cwd_basename(s->cwd), text_x, text_y, label_w,
                    "Sans Bold", 11, 1.0, 1.0, 1.0, 1.0);

        /* Model name (smaller, below basename) */
        render_text(cr, s->model, text_x, text_y + 16, label_w,
                    "Sans", 9, 0.6, 0.6, 0.7, 1.0);
    }
}

int session_switcher_hit_test(SessionSwitcherUI *sw, int x, int y, int win_w, int win_h)
{
    if (!sw->visible || sw->count == 0)
        return -1;

    double px, py, pw, ph;
    panel_rect(win_w, win_h, sw->count, &px, &py, &pw, &ph);

    double row_x = px + PANEL_PAD;
    double row_w = pw - 2 * PANEL_PAD;

    for (int i = 0; i < sw->count; i++) {
        double ry = py + PANEL_PAD + i * ROW_HEIGHT;
        if (x >= row_x && x <= row_x + row_w &&
            y >= ry && y <= ry + ROW_HEIGHT)
            return i;
    }
    return -1;
}
