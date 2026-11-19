#include "test.h"
#include "client.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

struct server_args {
    int listen_fd;
};

static void *echo_server(void *arg)
{
    struct server_args *args = arg;
    int conn = accept(args->listen_fd, NULL, NULL);
    if (conn < 0)
        return NULL;

    char buf[128];
    ssize_t n = recv(conn, buf, sizeof buf - 1, 0);
    if (n > 0) {
        buf[n] = '\0';
        char *newline = strchr(buf, '\n');
        if (newline)
            *newline = '\0';
        send(conn, "200 OK\n", 7, 0);
    }
    close(conn);
    return NULL;
}

static void test_invalid_arguments(void)
{
    CHECK(client_connect(NULL, "50001", 100) == -1);
    CHECK(errno == EINVAL);
    CHECK(client_connect("127.0.0.1", "", 100) == -1);
    CHECK(client_set_recv_timeout(-1, 100) == -1);
    CHECK(client_send(-1, "PING") == -1);
}

static void test_tcp_exchange(void)
{
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    CHECK(listen_fd >= 0);
    if (listen_fd < 0)
        return;

    int reuse = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof reuse);
    struct sockaddr_in address;
    memset(&address, 0, sizeof address);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    CHECK(bind(listen_fd, (struct sockaddr *)&address, sizeof address) == 0);
    CHECK(listen(listen_fd, 1) == 0);

    socklen_t address_len = sizeof address;
    CHECK(getsockname(listen_fd, (struct sockaddr *)&address, &address_len) == 0);

    struct server_args args = { listen_fd };
    pthread_t thread;
    CHECK(pthread_create(&thread, NULL, echo_server, &args) == 0);

    char service[16];
    snprintf(service, sizeof service, "%u", ntohs(address.sin_port));
    int fd = client_connect("127.0.0.1", service, 500);
    CHECK(fd >= 0);
    if (fd >= 0) {
        char response[128];
        CHECK(client_exchange(fd, "PING", response, sizeof response) == 1);
        CHECK_STR(response, "200 OK");
        client_close(fd);
    }

    pthread_join(thread, NULL);
    close(listen_fd);
}

static void test_recv_timeout(void)
{
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    CHECK(listen_fd >= 0);
    if (listen_fd < 0)
        return;

    struct sockaddr_in address;
    memset(&address, 0, sizeof address);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    CHECK(bind(listen_fd, (struct sockaddr *)&address, sizeof address) == 0);
    CHECK(listen(listen_fd, 1) == 0);

    socklen_t address_len = sizeof address;
    CHECK(getsockname(listen_fd, (struct sockaddr *)&address, &address_len) == 0);

    char service[16];
    snprintf(service, sizeof service, "%u", ntohs(address.sin_port));
    int fd = client_connect("127.0.0.1", service, 50);
    if (fd >= 0) {
        int conn = accept(listen_fd, NULL, NULL);
        CHECK(conn >= 0);
        char response[128];
        errno = 0;
        CHECK(client_recv(fd, response, sizeof response) == -1);
        CHECK(errno == EAGAIN || errno == EWOULDBLOCK);
        if (conn >= 0)
            close(conn);
        client_close(fd);
    }
    close(listen_fd);
}

int main(void)
{
    RUN(test_invalid_arguments);
    RUN(test_tcp_exchange);
    RUN(test_recv_timeout);
    return TEST_DONE();
}
