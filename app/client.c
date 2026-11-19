#include "client.h"

#include "protocol.h"

#include <errno.h>
#include <netdb.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

int client_set_recv_timeout(int fd, int timeout_ms)
{
    if (fd < 0 || timeout_ms < 0) {
        errno = EINVAL;
        return -1;
    }

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    return setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
}

int client_connect(const char *host, const char *service, int timeout_ms)
{
    if (host == NULL || service == NULL || *host == '\0' || *service == '\0') {
        errno = EINVAL;
        return -1;
    }

    struct addrinfo hints;
    struct addrinfo *resolved = NULL;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int gai_status = getaddrinfo(host, service, &hints, &resolved);
    if (gai_status != 0) {
        errno = EAI_FAIL;
        return -1;
    }

    int fd = -1;
    for (struct addrinfo *p = resolved; p != NULL; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0)
            continue;
        if (connect(fd, p->ai_addr, p->ai_addrlen) == 0)
            break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(resolved);

    if (fd < 0)
        return -1;
    if (client_set_recv_timeout(fd, timeout_ms) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

int client_send(int fd, const char *line)
{
    if (fd < 0 || line == NULL) {
        errno = EINVAL;
        return -1;
    }
    size_t len = strlen(line);
    if (len == 0) {
        errno = EINVAL;
        return -1;
    }
    if (line[len - 1] == '\n')
        return send_all(fd, line, len);

    char framed[PROTO_MAX_LINE + 2];
    if (len > PROTO_MAX_LINE - 1) {
        errno = EMSGSIZE;
        return -1;
    }
    memcpy(framed, line, len);
    framed[len] = '\n';
    framed[len + 1] = '\0';
    return send_all(fd, framed, len + 1);
}

int client_recv(int fd, char *out, size_t cap)
{
    if (fd < 0 || out == NULL || cap == 0) {
        errno = EINVAL;
        return -1;
    }
    LineReader reader;
    line_reader_init(&reader, fd);
    return recv_line(&reader, out, cap);
}

int client_exchange(int fd, const char *send, char *recv, size_t cap)
{
    if (client_send(fd, send) < 0)
        return -1;
    return client_recv(fd, recv, cap);
}

void client_close(int fd)
{
    if (fd >= 0)
        close(fd);
}
