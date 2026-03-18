CC = gcc
CFLAGS = -Wall -Wextra -O2 $(shell pkg-config --cflags x11 xext xcomposite xfixes cairo pango pangocairo libavformat libavcodec libavutil libswscale libcjson 2>/dev/null)
LDFLAGS = $(shell pkg-config --libs x11 xext xcomposite xfixes cairo pango pangocairo libavformat libavcodec libavutil libswscale libcjson 2>/dev/null) -lm
SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)
TARGET = masko-overlay

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
