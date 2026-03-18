#ifndef MASKO_IPC_H
#define MASKO_IPC_H

#include <cjson/cJSON.h>

typedef struct {
    int  fd;
    char recv_buf[65536];
    int  recv_len;
} MaskoIPC;

int    masko_ipc_connect(MaskoIPC *ipc, const char *sock_path);
void   masko_ipc_disconnect(MaskoIPC *ipc);
int    masko_ipc_send(MaskoIPC *ipc, cJSON *msg);
cJSON *masko_ipc_recv(MaskoIPC *ipc);
int    masko_ipc_fd(MaskoIPC *ipc);

#endif
