#include "test.h"
#include "protocol.h"

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

/* Cặp socket nối sẵn với nhau, thay cho client/server thật. */
static void make_pair(int sv[2])
{
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
        perror("socketpair");
        exit(1);
    }
}

static void set_rcv_timeout_ms(int fd, int ms)
{
    struct timeval tv = { .tv_sec = 0, .tv_usec = ms * 1000 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
}

static void raw_write(int fd, const char *s)
{
    CHECK(send_all(fd, s, strlen(s)) == 0);
}

static void test_status_text(void)
{
    CHECK_STR(proto_status_text(ST_OK), "OK");
    CHECK_STR(proto_status_text(ST_BAD_REQUEST), "BAD_REQUEST");
    CHECK_STR(proto_status_text(ST_AUTH_FAILED), "AUTH_FAILED");
    CHECK_STR(proto_status_text(ST_FORBIDDEN), "FORBIDDEN");
    CHECK_STR(proto_status_text(ST_NOT_FOUND), "NOT_FOUND");
    CHECK_STR(proto_status_text(ST_INVALID_VALUE), "INVALID_VALUE");
    CHECK_STR(proto_status_text(ST_LOCKED), "LOCKED");
    CHECK_STR(proto_status_text(500), "UNKNOWN");
}

/* 2 dòng tới trong cùng 1 lần recv() phải tách thành 2 lần recv_line(). */
static void test_two_lines_stuck(void)
{
    int sv[2];
    make_pair(sv);
    LineReader r;
    line_reader_init(&r, sv[1]);
    char line[PROTO_MAX_LINE];

    raw_write(sv[0], "SCAN app1\nQUIT\n");
    CHECK(recv_line(&r, line, sizeof line) == 1);
    CHECK_STR(line, "SCAN app1");
    CHECK(recv_line(&r, line, sizeof line) == 1);
    CHECK_STR(line, "QUIT");

    /* dòng rỗng và \r\n (gõ từ telnet) */
    raw_write(sv[0], "\nINFO LIGHT-01 a t\r\n");
    CHECK(recv_line(&r, line, sizeof line) == 1);
    CHECK_STR(line, "");
    CHECK(recv_line(&r, line, sizeof line) == 1);
    CHECK_STR(line, "INFO LIGHT-01 a t");

    close(sv[0]);
    CHECK(recv_line(&r, line, sizeof line) == 0);
    close(sv[1]);
}

/* Dòng bị cắt đôi: nửa đầu tới trước, hết timeout, nửa sau tới sau. */
static void test_line_split_in_half(void)
{
    int sv[2];
    make_pair(sv);
    set_rcv_timeout_ms(sv[1], 50);
    LineReader r;
    line_reader_init(&r, sv[1]);
    char line[PROTO_MAX_LINE];

    raw_write(sv[0], "SET LIGHT-01 ap");
    errno = 0;
    CHECK(recv_line(&r, line, sizeof line) == -1);
    CHECK(errno == EAGAIN || errno == EWOULDBLOCK);
    CHECK(r.len == strlen("SET LIGHT-01 ap"));      /* phần dở vẫn giữ */

    raw_write(sv[0], "p1 tok power on\n200 po");
    CHECK(recv_line(&r, line, sizeof line) == 1);
    CHECK_STR(line, "SET LIGHT-01 app1 tok power on");

    raw_write(sv[0], "wer=40\n");
    CHECK(recv_line(&r, line, sizeof line) == 1);
    CHECK_STR(line, "200 power=40");

    /* bên kia đóng khi dòng còn dở: bỏ phần dở, báo đóng */
    raw_write(sv[0], "dang do");
    close(sv[0]);
    CHECK(recv_line(&r, line, sizeof line) == 0);
    CHECK(r.len == 0);
    close(sv[1]);
}

static void test_line_too_long(void)
{
    int sv[2];
    make_pair(sv);
    LineReader r;
    line_reader_init(&r, sv[1]);
    char line[PROTO_MAX_LINE];
    char big[PROTO_MAX_LINE + 2];

    /* vừa khít: 511 ký tự + '\n' = 512 byte */
    memset(big, 'x', PROTO_MAX_LINE - 1);
    big[PROTO_MAX_LINE - 1] = '\n';
    big[PROTO_MAX_LINE] = '\0';
    raw_write(sv[0], big);
    CHECK(recv_line(&r, line, sizeof line) == 1);
    CHECK(strlen(line) == PROTO_MAX_LINE - 1);

    /* 512 ký tự chưa có '\n' -> quá dài */
    memset(big, 'y', PROTO_MAX_LINE);
    big[PROTO_MAX_LINE] = '\n';
    big[PROTO_MAX_LINE + 1] = '\0';
    raw_write(sv[0], big);
    errno = 0;
    CHECK(recv_line(&r, line, sizeof line) == -1);
    CHECK(errno == EMSGSIZE);

    /* out quá nhỏ: báo lỗi nhưng vẫn bỏ dòng đó, dòng sau đọc bình thường */
    raw_write(sv[0], "ABCDEFGH\nOK\n");
    CHECK(recv_line(&r, line, sizeof line) == 1);   /* phần dư "\n" của dòng quá dài */
    CHECK_STR(line, "");
    char small[4];
    errno = 0;
    CHECK(recv_line(&r, small, sizeof small) == -1);
    CHECK(errno == EMSGSIZE);
    CHECK(recv_line(&r, small, sizeof small) == 1);
    CHECK_STR(small, "OK");

    close(sv[0]);
    close(sv[1]);
}

static void test_recv_error(void)
{
    LineReader r;
    line_reader_init(&r, -1);
    char line[16];
    errno = 0;
    CHECK(recv_line(&r, line, sizeof line) == -1);
    CHECK(errno == EBADF);
}

static void test_send_line(void)
{
    int sv[2];
    make_pair(sv);
    LineReader r;
    line_reader_init(&r, sv[1]);
    char line[PROTO_MAX_LINE];

    CHECK(send_line(sv[0], "%d %s", ST_AUTH_FAILED,
                    proto_status_text(ST_AUTH_FAILED)) == 0);
    CHECK(recv_line(&r, line, sizeof line) == 1);
    CHECK_STR(line, "401 AUTH_FAILED");

    /* 511 ký tự được, 512 thì từ chối và không gửi gì */
    char big[PROTO_MAX_LINE + 1];
    memset(big, 'z', PROTO_MAX_LINE);
    big[PROTO_MAX_LINE] = '\0';
    errno = 0;
    CHECK(send_line(sv[0], "%s", big) == -1);
    CHECK(errno == EMSGSIZE);
    big[PROTO_MAX_LINE - 1] = '\0';
    CHECK(send_line(sv[0], "%s", big) == 0);
    CHECK(recv_line(&r, line, sizeof line) == 1);
    CHECK_STR(line, big);

    close(sv[0]);
    close(sv[1]);
}

/* ---- send_all với dữ liệu lớn hơn bộ đệm socket ---------------------- */

#define BIG_LEN (1024 * 1024)

struct reader_arg {
    int    fd;
    size_t got;
    int    ok;
};

static void *reader_thread(void *p)
{
    struct reader_arg *a = p;
    unsigned char buf[4096];
    ssize_t n;
    a->ok = 1;
    while ((n = recv(a->fd, buf, sizeof buf, 0)) > 0) {
        for (ssize_t i = 0; i < n; i++)
            if (buf[i] != (unsigned char)((a->got + (size_t)i) % 251))
                a->ok = 0;
        a->got += (size_t)n;
    }
    return NULL;
}

static void test_send_all_big(void)
{
    int sv[2];
    make_pair(sv);
    unsigned char *data = malloc(BIG_LEN);
    for (size_t i = 0; i < BIG_LEN; i++)
        data[i] = (unsigned char)(i % 251);

    struct reader_arg a = { .fd = sv[1] };
    pthread_t th;
    pthread_create(&th, NULL, reader_thread, &a);

    CHECK(send_all(sv[0], data, BIG_LEN) == 0);
    close(sv[0]);
    pthread_join(th, NULL);

    CHECK(a.got == BIG_LEN);
    CHECK(a.ok);
    free(data);
    close(sv[1]);
}

static void test_send_errors(void)
{
    int sv[2];
    make_pair(sv);
    close(sv[1]);
    /* bên kia đã đóng: trả EPIPE chứ không chết vì SIGPIPE */
    errno = 0;
    CHECK(send_all(sv[0], "x\n", 2) == -1);
    CHECK(errno == EPIPE);
    close(sv[0]);

    errno = 0;
    CHECK(send_all(-1, "x", 1) == -1);
    CHECK(errno == EBADF);
    CHECK(send_all(-1, "", 0) == 0);                /* không có gì để gửi */
}

/* ---- tách tham số ------------------------------------------------------ */

static void test_split(void)
{
    char *argv[PROTO_MAX_ARGS];

    char l1[] = "SET LIGHT-01 app1 tok power on";
    CHECK(proto_split(l1, argv, PROTO_MAX_ARGS) == 6);
    CHECK_STR(argv[0], "SET");
    CHECK_STR(argv[1], "LIGHT-01");
    CHECK_STR(argv[4], "power");
    CHECK_STR(argv[5], "on");

    char l2[] = "QUIT";
    CHECK(proto_split(l2, argv, PROTO_MAX_ARGS) == 1);
    CHECK_STR(argv[0], "QUIT");

    char l3[] = "";
    CHECK(proto_split(l3, argv, PROTO_MAX_ARGS) == 0);

    /* trường rỗng = sai cú pháp */
    char l4[] = "SET  LIGHT-01";
    CHECK(proto_split(l4, argv, PROTO_MAX_ARGS) == -1);
    char l5[] = " QUIT";
    CHECK(proto_split(l5, argv, PROTO_MAX_ARGS) == -1);
    char l6[] = "QUIT ";
    CHECK(proto_split(l6, argv, PROTO_MAX_ARGS) == -1);
    char l7[] = " ";
    CHECK(proto_split(l7, argv, PROTO_MAX_ARGS) == -1);

    /* đúng max trường thì được, quá max thì lỗi */
    char l8[] = "a b c";
    CHECK(proto_split(l8, argv, 3) == 3);
    CHECK_STR(argv[2], "c");
    char l9[] = "a b c d";
    CHECK(proto_split(l9, argv, 3) == -1);
}

int main(void)
{
    RUN(test_status_text);
    RUN(test_two_lines_stuck);
    RUN(test_line_split_in_half);
    RUN(test_line_too_long);
    RUN(test_recv_error);
    RUN(test_send_line);
    RUN(test_send_all_big);
    RUN(test_send_errors);
    RUN(test_split);
    return TEST_DONE();
}
