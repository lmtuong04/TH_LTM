#include "server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
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
