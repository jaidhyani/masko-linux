#include "ipc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>

int masko_ipc_connect(MaskoIPC *ipc, const char *sock_path)
{
    memset(ipc, 0, sizeof(*ipc));
    ipc->fd = -1;

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("masko-overlay: socket");
        return -1;
    }

    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("masko-overlay: connect");
        close(fd);
        return -1;
    }

    /* Non-blocking so recv never stalls the event loop */
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    ipc->fd = fd;
    return 0;
}

void masko_ipc_disconnect(MaskoIPC *ipc)
{
    if (ipc->fd >= 0) {
        close(ipc->fd);
        ipc->fd = -1;
    }
}

int masko_ipc_send(MaskoIPC *ipc, cJSON *msg)
{
    char *str = cJSON_PrintUnformatted(msg);
    if (!str) return -1;

    size_t len = strlen(str);

    /* Append newline as message delimiter */
    char *buf = malloc(len + 2);
    if (!buf) { free(str); return -1; }
    memcpy(buf, str, len);
    buf[len]     = '\n';
    buf[len + 1] = '\0';
    free(str);

    ssize_t written = write(ipc->fd, buf, len + 1);
    free(buf);

    return (written == (ssize_t)(len + 1)) ? 0 : -1;
}

cJSON *masko_ipc_recv(MaskoIPC *ipc)
{
    /* Try to read more data */
    int space = (int)sizeof(ipc->recv_buf) - ipc->recv_len - 1;
    if (space > 0) {
        ssize_t n = read(ipc->fd, ipc->recv_buf + ipc->recv_len, space);
        if (n > 0)
            ipc->recv_len += (int)n;
        else if (n == 0)
            return NULL;  /* peer closed */
        else if (errno != EAGAIN && errno != EWOULDBLOCK)
            return NULL;
    }

    /* Scan for newline delimiter */
    char *nl = memchr(ipc->recv_buf, '\n', ipc->recv_len);
    if (!nl) return NULL;

    *nl = '\0';
    cJSON *msg = cJSON_Parse(ipc->recv_buf);

    /* Shift remaining data to front of buffer */
    int consumed = (int)(nl - ipc->recv_buf) + 1;
    ipc->recv_len -= consumed;
    if (ipc->recv_len > 0)
        memmove(ipc->recv_buf, nl + 1, ipc->recv_len);

    return msg;
}

int masko_ipc_fd(MaskoIPC *ipc)
{
    return ipc->fd;
}
