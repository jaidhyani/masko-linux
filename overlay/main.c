#include "window.h"
#include "ipc.h"
#include "video.h"
#include "card_stack.h"
#include "render.h"
#include "permission_ui.h"
#include "toast.h"
#include "session_switcher.h"
#include "context_menu.h"
#include "hotkeys.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/select.h>
#include <X11/Xlib.h>
#include <cairo/cairo.h>
#include <cjson/cJSON.h>

static volatile sig_atomic_t running = 1;

static void on_signal(int sig)
{
    (void)sig;
    running = 0;
}

typedef struct {
    int active;
    int start_mouse_x, start_mouse_y;
    int start_win_x, start_win_y;
} DragState;

typedef struct {
    MaskoVideo video;
    int is_playing;
    int is_looping;
    double fps;
    struct timeval last_frame_time;
    int ended;      /* set when a non-looping video finishes */
    int has_played; /* set once any video has played — suppresses placeholder */
    char next_video[1024];  /* pre-loaded path for the video after this one */
    int has_next;
    int chroma_r, chroma_g, chroma_b, has_chroma;
} PlaybackState;

static double elapsed_ms(struct timeval *from, struct timeval *to)
{
    return (to->tv_sec - from->tv_sec) * 1000.0
         + (to->tv_usec - from->tv_usec) / 1000.0;
}

static void draw_placeholder(MaskoWindow *w)
{
    cairo_t *cr = masko_window_back_cr(w);

    /* Clear to fully transparent */
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);

    /* Blue circle as placeholder content */
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    double cx = w->width  / 2.0;
    double cy = w->height / 2.0;
    double r  = (w->width < w->height ? w->width : w->height) / 2.0 - 4.0;
    cairo_arc(cr, cx, cy, r, 0, 2 * 3.14159265);
    cairo_set_source_rgba(cr, 0.2, 0.4, 0.9, 0.85);
    cairo_fill(cr);

    masko_window_flip(w);
}

static void render_video_frame(MaskoWindow *w, PlaybackState *ps)
{
    cairo_surface_t *frame = masko_video_next_frame(&ps->video);
    if (!frame) {
        if (ps->is_looping) {
            masko_video_rewind(&ps->video);
            frame = masko_video_next_frame(&ps->video);
        }
        if (!frame) {
            ps->is_playing = 0;
            ps->ended = 1;
            return;
        }
    }

    cairo_t *cr = masko_window_back_cr(w);

    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    double sx = (double)w->width  / ps->video.width;
    double sy = (double)w->height / ps->video.height;
    cairo_scale(cr, sx, sy);
    cairo_set_source_surface(cr, frame, 0, 0);
    cairo_paint(cr);

    /* Reset transform so future draws aren't scaled */
    cairo_identity_matrix(cr);

    masko_window_flip(w);
    gettimeofday(&ps->last_frame_time, NULL);
}

static void stop_playback(PlaybackState *ps)
{
    if (ps->is_playing) {
        ps->is_playing = 0;
        masko_video_close(&ps->video);
    }
}

static void handle_ipc_message(MaskoWindow *w, PlaybackState *ps,
                               CardStack *cards, PermissionUI *perm,
                               ToastUI *toast, SessionSwitcherUI *switcher,
                               ContextMenu *menu, cJSON *msg)
{
    cJSON *cmd = cJSON_GetObjectItem(msg, "cmd");
    if (!cmd || !cJSON_IsString(cmd)) return;

    const char *c = cmd->valuestring;

    if (strcmp(c, "show") == 0) {
        masko_window_show(w);
    } else if (strcmp(c, "hide") == 0) {
        masko_window_hide(w);
    } else if (strcmp(c, "move") == 0) {
        cJSON *jx = cJSON_GetObjectItem(msg, "x");
        cJSON *jy = cJSON_GetObjectItem(msg, "y");
        if (jx && jy)
            masko_window_move(w, jx->valueint, jy->valueint);
    } else if (strcmp(c, "resize") == 0) {
        cJSON *jw = cJSON_GetObjectItem(msg, "width");
        cJSON *jh = cJSON_GetObjectItem(msg, "height");
        if (jw && jh) {
            masko_window_resize(w, jw->valueint, jh->valueint);
            if (!ps->is_playing)
                draw_placeholder(w);
        }
    } else if (strcmp(c, "play") == 0) {
        cJSON *jvideo = cJSON_GetObjectItem(msg, "video");
        if (!jvideo || !cJSON_IsString(jvideo)) return;

        stop_playback(ps);

        if (masko_video_open(&ps->video, jvideo->valuestring) < 0) {
            fprintf(stderr, "masko-overlay: failed to open video: %s\n",
                    jvideo->valuestring);
            return;
        }

        cJSON *jkey = cJSON_GetObjectItem(msg, "chroma_key");
        if (jkey) {
            int kr = cJSON_GetObjectItem(jkey, "r")->valueint;
            int kg = cJSON_GetObjectItem(jkey, "g")->valueint;
            int kb = cJSON_GetObjectItem(jkey, "b")->valueint;
            ps->chroma_r = kr;
            ps->chroma_g = kg;
            ps->chroma_b = kb;
            ps->has_chroma = 1;
            masko_video_set_chroma_key(&ps->video, kr, kg, kb);
        }

        /* Parse next_video for seamless transition */
        cJSON *jnext = cJSON_GetObjectItem(msg, "next_video");
        if (jnext && cJSON_IsString(jnext)) {
            strncpy(ps->next_video, jnext->valuestring, sizeof(ps->next_video) - 1);
            ps->next_video[sizeof(ps->next_video) - 1] = '\0';
            ps->has_next = 1;
        } else {
            ps->has_next = 0;
            ps->next_video[0] = '\0';
        }

        cJSON *jloop = cJSON_GetObjectItem(msg, "loop");
        ps->is_looping = (jloop && cJSON_IsTrue(jloop)) ? 1 : 0;
        ps->fps = masko_video_fps(&ps->video);
        ps->is_playing = 1;
        ps->has_played = 1;
        gettimeofday(&ps->last_frame_time, NULL);

        render_video_frame(w, ps);
    } else if (strcmp(c, "stop") == 0) {
        stop_playback(ps);
        draw_placeholder(w);
    } else if (strcmp(c, "permission") == 0) {
        cJSON *jtool  = cJSON_GetObjectItem(msg, "tool");
        cJSON *jinput = cJSON_GetObjectItem(msg, "input");
        cJSON *jid    = cJSON_GetObjectItem(msg, "id");
        fprintf(stderr, "masko-overlay: permission IPC received "
                "(tool=%s, id=%s, has_input=%d)\n",
                jtool  ? (cJSON_IsString(jtool) ? jtool->valuestring : "<non-str>") : "<null>",
                jid    ? (cJSON_IsString(jid)   ? jid->valuestring   : "<non-str>") : "<null>",
                jinput != NULL);
        if (!jtool || !jinput || !jid) {
            fprintf(stderr, "masko-overlay: permission missing fields, ignoring\n");
            return;
        }

        permission_ui_show(perm,
            cJSON_IsString(jtool)  ? jtool->valuestring  : "",
            cJSON_IsString(jinput) ? jinput->valuestring : "",
            cJSON_IsString(jid)    ? jid->valuestring    : "");
        card_stack_push(cards, CARD_PERMISSION);
        fprintf(stderr, "masko-overlay: permission_ui_show done, perm->visible=%d, "
                "card_top=%d\n", perm->visible, card_stack_top(cards));
    } else if (strcmp(c, "dismiss_permission") == 0) {
        permission_ui_hide(perm);
        card_stack_pop(cards, CARD_PERMISSION);
    } else if (strcmp(c, "toast") == 0) {
        cJSON *jtext = cJSON_GetObjectItem(msg, "text");
        cJSON *jdur  = cJSON_GetObjectItem(msg, "duration");
        if (!jtext || !cJSON_IsString(jtext)) return;
        int dur = (jdur && cJSON_IsNumber(jdur)) ? jdur->valueint : 3000;
        toast_show(toast, jtext->valuestring, dur);
        card_stack_push(cards, CARD_TOAST);
    } else if (strcmp(c, "session_switcher") == 0) {
        cJSON *jsessions = cJSON_GetObjectItem(msg, "sessions");
        cJSON *jselected = cJSON_GetObjectItem(msg, "selected");
        int sel = (jselected && cJSON_IsNumber(jselected)) ? jselected->valueint : 0;
        session_switcher_show(switcher, jsessions, sel);
        card_stack_push(cards, CARD_SESSION_SWITCHER);
    } else if (strcmp(c, "set_menu") == 0) {
        cJSON *items = cJSON_GetObjectItem(msg, "items");
        context_menu_set_items(menu, items);
    } else if (strcmp(c, "session_switcher_nav") == 0) {
        cJSON *jdir = cJSON_GetObjectItem(msg, "direction");
        if (jdir && cJSON_IsString(jdir))
            session_switcher_nav(switcher, jdir->valuestring);
    }
}

static void close_context_menu(ContextMenu *menu, CardStack *cards)
{
    context_menu_hide(menu);
    card_stack_pop(cards, CARD_CONTEXT_MENU);
}

static void send_position_event(MaskoIPC *ipc, int x, int y)
{
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "event", "position");
    cJSON_AddNumberToObject(msg, "x", x);
    cJSON_AddNumberToObject(msg, "y", y);
    masko_ipc_send(ipc, msg);
    cJSON_Delete(msg);
}

static void send_hotkey_event(MaskoIPC *ipc, const char *key)
{
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "event", "hotkey");
    cJSON_AddStringToObject(msg, "key", key);
    masko_ipc_send(ipc, msg);
    cJSON_Delete(msg);
}

int main(int argc, char **argv)
{
    int test_mode = 0;
    const char *sock_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--test-mode") == 0) {
            test_mode = 1;
        } else if (strcmp(argv[i], "--sock") == 0 && i + 1 < argc) {
            sock_path = argv[++i];
        }
    }

    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);

    MaskoWindow win;
    if (masko_window_init(&win, 100, 100, 200, 200) < 0)
        return 1;

    masko_window_show(&win);
    draw_placeholder(&win);

    MaskoIPC ipc;
    int ipc_connected = 0;

    if (!test_mode && sock_path) {
        if (masko_ipc_connect(&ipc, sock_path) == 0) {
            ipc_connected = 1;
            cJSON *ready = cJSON_CreateObject();
            cJSON_AddStringToObject(ready, "event", "ready");
            masko_ipc_send(&ipc, ready);
            cJSON_Delete(ready);
        } else {
            fprintf(stderr, "masko-overlay: IPC connect failed, running without IPC\n");
        }
    }

    Hotkeys hotkeys;
    hotkeys_init(&hotkeys, win.dpy);
    hotkeys_grab(&hotkeys);

    DragState drag = {0};
    PlaybackState playback = {0};
    CardStack cards;
    PermissionUI perm;
    ToastUI toast;
    SessionSwitcherUI switcher;
    ContextMenu ctx_menu;
    card_stack_init(&cards);
    permission_ui_init(&perm);
    toast_init(&toast);
    session_switcher_init(&switcher);
    context_menu_init(&ctx_menu);
    context_menu_create_popup(&ctx_menu, win.dpy, win.screen);
    int need_redraw = 0;
    int iterations = 0;
    struct timeval last_tick;
    gettimeofday(&last_tick, NULL);

    while (running) {
        if (test_mode && ++iterations > 50)
            break;

        fd_set rfds;
        FD_ZERO(&rfds);

        int x11_fd = masko_window_fd(&win);
        FD_SET(x11_fd, &rfds);
        int maxfd = x11_fd;

        if (ipc_connected) {
            int ipc_fd = masko_ipc_fd(&ipc);
            FD_SET(ipc_fd, &rfds);
            if (ipc_fd > maxfd) maxfd = ipc_fd;
        }

        /* Short timeout when playing video or toast is active */
        long timeout_us = 100000;
        if (toast.visible)
            timeout_us = 50000;
        if (playback.is_playing) {
            double frame_ms = 1000.0 / playback.fps;
            struct timeval now;
            gettimeofday(&now, NULL);
            double wait = frame_ms - elapsed_ms(&playback.last_frame_time, &now);
            if (wait < 1.0) wait = 1.0;
            timeout_us = (long)(wait * 1000);
        }

        struct timeval tv = {
            .tv_sec  = timeout_us / 1000000,
            .tv_usec = timeout_us % 1000000,
        };
        select(maxfd + 1, &rfds, NULL, NULL, &tv);

        /* Process all pending X11 events */
        while (XPending(win.dpy)) {
            XEvent ev;
            XNextEvent(win.dpy, &ev);

            /* Events on the popup menu window */
            if (ctx_menu.popup_win &&
                (ev.type == ButtonPress || ev.type == ButtonRelease ||
                 ev.type == MotionNotify || ev.type == LeaveNotify ||
                 ev.type == Expose) &&
                ev.xany.window == ctx_menu.popup_win) {

                if (ev.type == MotionNotify) {
                    context_menu_hover(&ctx_menu, ev.xmotion.x, ev.xmotion.y);
                } else if (ev.type == LeaveNotify) {
                    int prev = ctx_menu.hovered;
                    ctx_menu.hovered = -1;
                    if (prev != -1)
                        context_menu_redraw(&ctx_menu);
                } else if (ev.type == Expose) {
                    context_menu_redraw(&ctx_menu);
                } else if (ev.type == ButtonPress && ev.xbutton.button == 1) {
                    int hit = context_menu_hit_test(&ctx_menu,
                                  ev.xbutton.x, ev.xbutton.y);
                    if (hit >= 0) {
                        const char *act = ctx_menu.items[hit].action;
                        if (strcmp(act, "quit") == 0)
                            running = 0;
                        if (ipc_connected) {
                            cJSON *resp = cJSON_CreateObject();
                            cJSON_AddStringToObject(resp, "event", "menu");
                            cJSON_AddStringToObject(resp, "action", act);
                            masko_ipc_send(&ipc, resp);
                            cJSON_Delete(resp);
                        }
                    }
                    close_context_menu(&ctx_menu, &cards);
                }
                continue;
            }

            /*
             * Grabbed pointer: clicks outside the popup arrive as
             * ButtonPress on the main window (or root). Dismiss.
             */
            if (ctx_menu.visible && ev.type == ButtonPress &&
                ev.xany.window != ctx_menu.popup_win) {
                close_context_menu(&ctx_menu, &cards);
                /* Re-process this event below only if it's a right-click
                   on the main window (to reopen the menu). */
                if (ev.xany.window != win.win)
                    continue;
            }

            switch (ev.type) {
            case Expose:
                need_redraw = 1;
                break;

            case ButtonPress:
                if (ev.xbutton.button == 3) {
                    if (!ctx_menu.visible) {
                        context_menu_show_at(&ctx_menu,
                            ev.xbutton.x_root, ev.xbutton.y_root);
                        card_stack_push(&cards, CARD_CONTEXT_MENU);
                    }
                } else if (ev.xbutton.button == 1) {
                    int sw_hit = session_switcher_hit_test(&switcher,
                                     ev.xbutton.x, ev.xbutton.y,
                                     win.width, win.height);
                    int perm_hit = permission_ui_hit_test(&perm,
                                       ev.xbutton.x, ev.xbutton.y);

                    if (sw_hit >= 0) {
                        if (ipc_connected) {
                            cJSON *resp = cJSON_CreateObject();
                            cJSON_AddStringToObject(resp, "event", "hotkey");
                            cJSON_AddStringToObject(resp, "key", "session_confirm");
                            cJSON_AddNumberToObject(resp, "index", sw_hit);
                            masko_ipc_send(&ipc, resp);
                            cJSON_Delete(resp);
                        }
                        session_switcher_hide(&switcher);
                        card_stack_pop(&cards, CARD_SESSION_SWITCHER);
                        need_redraw = 1;
                    } else if (perm_hit) {
                        const char *action = (perm_hit == 1) ? "approve" : "deny";
                        if (ipc_connected) {
                            cJSON *resp = cJSON_CreateObject();
                            cJSON_AddStringToObject(resp, "event", "permission_response");
                            cJSON_AddStringToObject(resp, "id", perm.perm_id);
                            cJSON_AddStringToObject(resp, "action", action);
                            masko_ipc_send(&ipc, resp);
                            cJSON_Delete(resp);
                        }
                        permission_ui_hide(&perm);
                        card_stack_pop(&cards, CARD_PERMISSION);
                        need_redraw = 1;
                    } else {
                        drag.active = 1;
                        drag.start_mouse_x = ev.xbutton.x_root;
                        drag.start_mouse_y = ev.xbutton.y_root;
                        drag.start_win_x   = win.x;
                        drag.start_win_y   = win.y;
                    }
                }
                break;

            case MotionNotify:
                if (drag.active) {
                    int nx = drag.start_win_x + (ev.xmotion.x_root - drag.start_mouse_x);
                    int ny = drag.start_win_y + (ev.xmotion.y_root - drag.start_mouse_y);
                    masko_window_move(&win, nx, ny);
                } else if (perm.visible) {
                    permission_ui_hover(&perm, ev.xmotion.x, ev.xmotion.y);
                    need_redraw = 1;
                }
                break;

            case ButtonRelease:
                if (ev.xbutton.button == 1 && drag.active) {
                    drag.active = 0;
                    if (ipc_connected)
                        send_position_event(&ipc, win.x, win.y);
                }
                break;

            case KeyRelease: {
                char *action = hotkeys_handle_release(&hotkeys, &ev.xkey);
                if (action && ipc_connected)
                    send_hotkey_event(&ipc, action);
                free(action);
                break;
            }

            case KeyPress: {
                char *action = hotkeys_handle(&hotkeys, &ev.xkey);
                if (!action) break;

                if (strcmp(action, "dismiss") == 0) {
                    CardType top = card_stack_top(&cards);
                    if (top == CARD_CONTEXT_MENU) {
                        close_context_menu(&ctx_menu, &cards);
                        need_redraw = 1;
                    } else if (top == CARD_SESSION_SWITCHER) {
                        session_switcher_hide(&switcher);
                        card_stack_pop(&cards, CARD_SESSION_SWITCHER);
                        need_redraw = 1;
                    } else if (top == CARD_PERMISSION) {
                        permission_ui_hide(&perm);
                        card_stack_pop(&cards, CARD_PERMISSION);
                        need_redraw = 1;
                    }
                    if (ipc_connected)
                        send_hotkey_event(&ipc, "dismiss");
                } else if (strcmp(action, "approve") == 0) {
                    if (perm.visible && ipc_connected) {
                        cJSON *resp = cJSON_CreateObject();
                        cJSON_AddStringToObject(resp, "event", "permission_response");
                        cJSON_AddStringToObject(resp, "id", perm.perm_id);
                        cJSON_AddStringToObject(resp, "action", "approve");
                        masko_ipc_send(&ipc, resp);
                        cJSON_Delete(resp);
                        permission_ui_hide(&perm);
                        card_stack_pop(&cards, CARD_PERMISSION);
                        need_redraw = 1;
                    }
                } else if (strcmp(action, "deny") == 0) {
                    if (perm.visible && ipc_connected) {
                        cJSON *resp = cJSON_CreateObject();
                        cJSON_AddStringToObject(resp, "event", "permission_response");
                        cJSON_AddStringToObject(resp, "id", perm.perm_id);
                        cJSON_AddStringToObject(resp, "action", "deny");
                        masko_ipc_send(&ipc, resp);
                        cJSON_Delete(resp);
                        permission_ui_hide(&perm);
                        card_stack_pop(&cards, CARD_PERMISSION);
                        need_redraw = 1;
                    }
                } else if (ipc_connected) {
                    send_hotkey_event(&ipc, action);
                }

                free(action);
                break;
            }
            }
        }

        /* Process IPC messages */
        if (ipc_connected) {
            cJSON *msg;
            while ((msg = masko_ipc_recv(&ipc)) != NULL) {
                handle_ipc_message(&win, &playback, &cards, &perm, &toast, &switcher, &ctx_menu, msg);
                need_redraw = 1;
                cJSON_Delete(msg);
            }
        }

        /* Compute delta since last tick */
        struct timeval tick_now;
        gettimeofday(&tick_now, NULL);
        int delta_ms = (int)elapsed_ms(&last_tick, &tick_now);
        last_tick = tick_now;

        /* Advance video frame when it's time */
        if (playback.is_playing) {
            double frame_ms = 1000.0 / playback.fps;
            if (elapsed_ms(&playback.last_frame_time, &tick_now) >= frame_ms) {
                render_video_frame(&win, &playback);
                need_redraw = 1;
            }
        }

        /* Non-looping video ended — switch to pre-sent next video or notify backend */
        if (playback.ended) {
            playback.ended = 0;
            masko_video_close(&playback.video);

            if (playback.has_next && playback.next_video[0]) {
                /* Instant switch to pre-sent next video */
                if (masko_video_open(&playback.video, playback.next_video) == 0) {
                    /* Apply stored chroma key */
                    if (playback.has_chroma)
                        masko_video_set_chroma_key(&playback.video,
                            playback.chroma_r, playback.chroma_g, playback.chroma_b);
                    playback.is_looping = 1;
                    playback.is_playing = 1;
                    playback.fps = masko_video_fps(&playback.video);
                    gettimeofday(&playback.last_frame_time, NULL);
                    render_video_frame(&win, &playback);
                    need_redraw = 1;
                }
                playback.has_next = 0;
            }

            /* Still notify backend so state machine advances */
            if (ipc_connected) {
                cJSON *msg = cJSON_CreateObject();
                cJSON_AddStringToObject(msg, "event", "video_ended");
                masko_ipc_send(&ipc, msg);
                cJSON_Delete(msg);
            }
        }

        /* Tick toast timer */
        if (toast.visible) {
            if (!toast_tick(&toast, delta_ms)) {
                toast_hide(&toast);
                card_stack_pop(&cards, CARD_TOAST);
                need_redraw = 1;
            } else {
                need_redraw = 1;
            }
        }

        if (need_redraw) {
            need_redraw = 0;

            /* Base content: keep last frame visible between videos
               to avoid placeholder flash during transitions */
            if (!playback.is_playing && !playback.has_played)
                draw_placeholder(&win);

            cairo_t *cr = masko_window_back_cr(&win);

            /* Draw order: video/placeholder -> toast -> permission -> session_switcher */
            if (toast.visible)
                toast_draw(&toast, cr, win.width, win.height);

            if (perm.visible) {
                fprintf(stderr, "masko-overlay: drawing permission UI "
                        "(win %dx%d, tool='%s')\n",
                        win.width, win.height, perm.tool_name);
                permission_ui_draw(&perm, cr, win.width, win.height);
            }

            if (switcher.visible)
                session_switcher_draw(&switcher, cr, win.width, win.height);

            masko_window_flip(&win);
        }
    }

    hotkeys_ungrab(&hotkeys);
    stop_playback(&playback);
    context_menu_destroy_popup(&ctx_menu);
    if (ipc_connected)
        masko_ipc_disconnect(&ipc);
    masko_window_destroy(&win);
    return 0;
}
