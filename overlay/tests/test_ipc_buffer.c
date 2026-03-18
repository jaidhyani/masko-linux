#include "test_harness.h"
#include "../ipc.h"
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

/*
 * masko_ipc_recv calls read() on ipc->fd before scanning the buffer.
 * We use a non-blocking pipe so read() returns EAGAIN (which recv
 * treats as "no new data, but check buffer anyway").
 */
static void make_test_ipc(MaskoIPC *ipc, int *write_fd)
{
    int pipefd[2];
    pipe(pipefd);
    fcntl(pipefd[0], F_SETFL, O_NONBLOCK);
    memset(ipc, 0, sizeof(*ipc));
    ipc->fd = pipefd[0];
    *write_fd = pipefd[1];
}

static void cleanup_ipc(MaskoIPC *ipc, int write_fd)
{
    if (ipc->fd >= 0) close(ipc->fd);
    if (write_fd >= 0) close(write_fd);
}

void test_recv_single_message(void)
{
    MaskoIPC ipc;
    int wfd;
    make_test_ipc(&ipc, &wfd);

    const char *msg = "{\"cmd\":\"show\"}\n";
    write(wfd, msg, strlen(msg));

    cJSON *result = masko_ipc_recv(&ipc);
    ASSERT(result != NULL, "parsed message");
    if (result) {
        cJSON *cmd = cJSON_GetObjectItem(result, "cmd");
        ASSERT(cmd != NULL, "has cmd field");
        ASSERT_STR_EQ(cmd->valuestring, "show", "cmd is show");
        cJSON_Delete(result);
    }
    ASSERT_EQ(ipc.recv_len, 0, "buffer consumed");
    cleanup_ipc(&ipc, wfd);
}

void test_recv_partial_message(void)
{
    MaskoIPC ipc;
    int wfd;
    make_test_ipc(&ipc, &wfd);

    const char *partial = "{\"cmd\":\"hi";
    write(wfd, partial, strlen(partial));

    cJSON *result = masko_ipc_recv(&ipc);
    ASSERT(result == NULL, "no complete message yet");
    ASSERT_EQ(ipc.recv_len, (int)strlen(partial), "buffer preserved");
    cleanup_ipc(&ipc, wfd);
}

void test_recv_two_messages(void)
{
    MaskoIPC ipc;
    int wfd;
    make_test_ipc(&ipc, &wfd);

    const char *msgs = "{\"a\":1}\n{\"b\":2}\n";
    write(wfd, msgs, strlen(msgs));

    cJSON *r1 = masko_ipc_recv(&ipc);
    ASSERT(r1 != NULL, "first message parsed");
    if (r1) {
        ASSERT(cJSON_GetObjectItem(r1, "a") != NULL, "first has 'a'");
        cJSON_Delete(r1);
    }

    cJSON *r2 = masko_ipc_recv(&ipc);
    ASSERT(r2 != NULL, "second message parsed");
    if (r2) {
        ASSERT(cJSON_GetObjectItem(r2, "b") != NULL, "second has 'b'");
        cJSON_Delete(r2);
    }

    ASSERT_EQ(ipc.recv_len, 0, "buffer fully consumed");
    cleanup_ipc(&ipc, wfd);
}

void test_recv_message_plus_partial(void)
{
    MaskoIPC ipc;
    int wfd;
    make_test_ipc(&ipc, &wfd);

    const char *data = "{\"x\":1}\n{\"y\":";
    write(wfd, data, strlen(data));

    cJSON *r1 = masko_ipc_recv(&ipc);
    ASSERT(r1 != NULL, "complete message parsed");
    if (r1) cJSON_Delete(r1);

    cJSON *r2 = masko_ipc_recv(&ipc);
    ASSERT(r2 == NULL, "partial message not parsed");
    ASSERT(ipc.recv_len > 0, "partial data preserved in buffer");
    cleanup_ipc(&ipc, wfd);
}

void test_recv_empty_buffer(void)
{
    MaskoIPC ipc;
    int wfd;
    make_test_ipc(&ipc, &wfd);

    cJSON *result = masko_ipc_recv(&ipc);
    ASSERT(result == NULL, "nothing to parse");
    cleanup_ipc(&ipc, wfd);
}

int main(void)
{
    printf("ipc_buffer tests:\n");
    RUN_TEST(test_recv_single_message);
    RUN_TEST(test_recv_partial_message);
    RUN_TEST(test_recv_two_messages);
    RUN_TEST(test_recv_message_plus_partial);
    RUN_TEST(test_recv_empty_buffer);
    TEST_REPORT();
}
