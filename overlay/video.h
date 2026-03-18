#ifndef MASKO_VIDEO_H
#define MASKO_VIDEO_H

#include <cairo/cairo.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>

typedef struct {
    AVFormatContext *fmt_ctx;
    AVCodecContext *codec_ctx;
    struct SwsContext *sws_ctx;
    int stream_idx;
    int width;
    int height;
    AVFrame *frame;
    AVFrame *frame_rgba;
    AVPacket *pkt;
    uint8_t *rgba_buf;
    cairo_surface_t *surface;
    int finished;
    int key_r, key_g, key_b;
    int has_key;
} MaskoVideo;

int masko_video_open(MaskoVideo *v, const char *path);
void masko_video_close(MaskoVideo *v);
cairo_surface_t *masko_video_next_frame(MaskoVideo *v);
void masko_video_rewind(MaskoVideo *v);
double masko_video_fps(MaskoVideo *v);
void masko_video_set_chroma_key(MaskoVideo *v, int r, int g, int b);

#endif
