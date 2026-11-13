#include "protocol.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

/* Không để SIGPIPE giết chương trình khi bên kia đã đóng: send() trả EPIPE. */
#ifdef MSG_NOSIGNAL
#define SEND_FLAGS MSG_NOSIGNAL
#else
#define SEND_FLAGS 0
#endif

const char *proto_status_text(int code)
{
    switch (code) {
    case ST_OK:            return "OK";
    case ST_BAD_REQUEST:   return "BAD_REQUEST";
    case ST_AUTH_FAILED:   return "AUTH_FAILED";
    case ST_FORBIDDEN:     return "FORBIDDEN";
    case ST_NOT_FOUND:     return "NOT_FOUND";
    case ST_INVALID_VALUE: return "INVALID_VALUE";
    case ST_LOCKED:        return "LOCKED";
    default:               return "UNKNOWN";
    }
}

/* ---- gửi ---------------------------------------------------------------- */

int send_all(int fd, const void *buf, size_t len)
{
    const char *p = buf;
    while (len > 0) {
        ssize_t n = send(fd, p, len, SEND_FLAGS);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

int send_line(int fd, const char *fmt, ...)
{
    char line[PROTO_MAX_LINE + 1];   /* +1 để vsnprintf còn chỗ cho '\0' */
    va_list ap;

    va_start(ap, fmt);
    int n = vsnprintf(line, sizeof line - 1, fmt, ap);
    va_end(ap);

    if (n < 0 || (size_t)n + 1 > PROTO_MAX_LINE) {
        errno = EMSGSIZE;
        return -1;
    }
    line[n++] = '\n';
    return send_all(fd, line, (size_t)n);
}

/* ---- nhận --------------------------------------------------------------- */

void line_reader_init(LineReader *r, int fd)
{
    r->fd = fd;
    r->len = 0;
}

int recv_line(LineReader *r, char *out, size_t cap)
{
    for (;;) {
        /* Đã có trọn 1 dòng trong bộ đệm? */
        char *nl = memchr(r->buf, '\n', r->len);
        if (nl) {
            size_t line_len = (size_t)(nl - r->buf);
            size_t used = line_len + 1;
            if (line_len > 0 && r->buf[line_len - 1] == '\r')
                line_len--;

            int fits = line_len < cap;
            if (fits) {
                memcpy(out, r->buf, line_len);
                out[line_len] = '\0';
            }
            /* Bỏ dòng vừa đọc, dồn phần còn lại (dòng sau) lên đầu */
            r->len -= used;
            memmove(r->buf, r->buf + used, r->len);
            if (!fits) {
                errno = EMSGSIZE;
                return -1;
            }
            return 1;
        }

        if (r->len == sizeof r->buf) {      /* đầy mà chưa thấy '\n' */
            r->len = 0;
            errno = EMSGSIZE;
            return -1;
        }

        ssize_t n = recv(r->fd, r->buf + r->len, sizeof r->buf - r->len, 0);
        if (n == 0) {
            r->len = 0;
            return 0;
        }
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        r->len += (size_t)n;
    }
}

/* ---- tách tham số ------------------------------------------------------- */

int proto_split(char *line, char *argv[], int max)
{
    if (*line == '\0')
        return 0;

    int argc = 0;
    char *p = line;
    for (;;) {
        char *sp = strchr(p, ' ');
        if (sp == p || *p == '\0')          /* trường rỗng */
            return -1;
        if (argc == max)
            return -1;
        argv[argc++] = p;
        if (!sp)
            return argc;
        *sp = '\0';
        p = sp + 1;
    }
}
