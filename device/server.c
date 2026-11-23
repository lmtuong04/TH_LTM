#include "server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "protocol.h"

int udp_open(int port)
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    int one = 1;
    struct timeval tv = { .tv_sec = 0, .tv_usec = 200 * 1000 };
    struct sockaddr_in addr;

    if (fd < 0)
        return -1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);

    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);
    if (bind(fd, (struct sockaddr *)&addr, sizeof addr) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

void udp_handle_datagram(int sock, const DeviceList *list,
                         char *buf, ssize_t len,
                         const struct sockaddr *from, socklen_t fromlen)
{
    char line[PROTO_MAX_LINE];
    char *argv[PROTO_MAX_ARGS];
    int argc, i;

    if (len <= 0 || !list)
        return;
    if ((size_t)len >= sizeof line)
        len = (ssize_t)sizeof line - 1;
    memcpy(line, buf, (size_t)len);
    line[len] = '\0';
    if (len > 0 && line[len - 1] == '\n')
        line[--len] = '\0';
    if (len > 0 && line[len - 1] == '\r')
        line[--len] = '\0';

    argc = proto_split(line, argv, PROTO_MAX_ARGS);
    if (argc < 1 || strcmp(argv[0], "SCAN") != 0)
        return;
    /* SCAN <app_id>: app_id không cần cho discovery, bỏ qua */

    for (i = 0; i < (int)list->n; i++) {
        const Device *d = &list->items[i];
        char msg[192];
        int m = snprintf(msg, sizeof msg, "DEVICE %s %s %d %s\n",
                         d->id, device_type_name(d->type),
                         PROTO_TCP_PORT, d->name);
        if (m > 0)
            sendto(sock, msg, (size_t)m, 0, from, fromlen);
    }
}

void udp_serve_loop(int sock, const DeviceList *list, volatile int *stop)
{
    char buf[PROTO_MAX_LINE];

    for (;;) {
        struct sockaddr_storage from;
        socklen_t fromlen = sizeof from;
        ssize_t len;

        len = recvfrom(sock, buf, sizeof buf, 0,
                       (struct sockaddr *)&from, &fromlen);
        if (len < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                if (stop && !*stop)
                    break;
                continue;
            }
            break;
        }
        if (stop && !*stop)
            break;
        udp_handle_datagram(sock, list, buf, len,
                            (struct sockaddr *)&from, fromlen);
    }
}

static void *udp_thread_main(void *arg)
{
    UdpServer *srv = arg;
    udp_serve_loop(srv->fd, srv->list, &srv->running);
    return NULL;
}

int udp_server_start(UdpServer *srv, const DeviceList *list, int port)
{
    memset(srv, 0, sizeof *srv);
    srv->fd = udp_open(port);
    if (srv->fd < 0)
        return -1;
    srv->list = list;
    srv->running = 1;
    if (pthread_create(&srv->thread, NULL, udp_thread_main, srv) != 0) {
        close(srv->fd);
        srv->fd = -1;
        srv->running = 0;
        return -1;
    }
    return 0;
}

void udp_server_stop(UdpServer *srv)
{
    if (!srv || srv->fd < 0)
        return;
    srv->running = 0;
    pthread_join(srv->thread, NULL);
    close(srv->fd);
    srv->fd = -1;
}

/* ---- TCP ---------------------------------------------------------------- */

int tcp_open(int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    struct sockaddr_in addr;

    if (fd < 0)
        return -1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);

    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);
    if (bind(fd, (struct sockaddr *)&addr, sizeof addr) != 0 ||
        listen(fd, 16) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

int tcp_handle_command(int fd, const DeviceList *list, char *line)
{
    char *argv[PROTO_MAX_ARGS];
    int argc = proto_split(line, argv, PROTO_MAX_ARGS);
    (void)list;

    if (argc >= 1 && strcmp(argv[0], "QUIT") == 0)
        return 0;
    /* TODO A8: CONNECT/auth · A11: INFO/SET/STATS/PASSWD/TIMER */
    return send_line(fd, "%d %s", ST_BAD_REQUEST,
                     proto_status_text(ST_BAD_REQUEST)) == 0;
}

typedef struct {
    int fd;
    const DeviceList *list;
} TcpClient;

static void *client_main(void *arg)
{
    TcpClient *c = arg;
    LineReader r;
    char line[PROTO_MAX_LINE];

    line_reader_init(&r, c->fd);
    for (;;) {
        int rc = recv_line(&r, line, sizeof line);
        if (rc != 1)
            break;
        if (!tcp_handle_command(c->fd, c->list, line))
            break;
    }
    close(c->fd);
    free(c);
    return NULL;
}

static void *tcp_thread_main(void *arg)
{
    TcpServer *srv = arg;

    while (srv->running) {
        int cfd = accept(srv->fd, NULL, NULL);
        TcpClient *c;
        pthread_t t;

        if (cfd < 0) {
            if (errno == EINTR)
                continue;
            break;              /* stop() đã shutdown listen socket */
        }
        c = malloc(sizeof *c);
        if (!c) {
            close(cfd);
            continue;
        }
        c->fd = cfd;
        c->list = srv->list;
        if (pthread_create(&t, NULL, client_main, c) != 0) {
            free(c);
            close(cfd);
            continue;
        }
        pthread_detach(t);
    }
    return NULL;
}

int tcp_server_start(TcpServer *srv, const DeviceList *list, int port)
{
    memset(srv, 0, sizeof *srv);
    srv->fd = tcp_open(port);
    if (srv->fd < 0)
        return -1;
    srv->list = list;
    srv->running = 1;
    if (pthread_create(&srv->thread, NULL, tcp_thread_main, srv) != 0) {
        close(srv->fd);
        srv->fd = -1;
        srv->running = 0;
        return -1;
    }
    return 0;
}

void tcp_server_stop(TcpServer *srv)
{
    if (!srv || srv->fd < 0)
        return;
    srv->running = 0;
    shutdown(srv->fd, SHUT_RDWR);   /* đánh thức accept() đang chặn */
    pthread_join(srv->thread, NULL);
    close(srv->fd);
    srv->fd = -1;
}
