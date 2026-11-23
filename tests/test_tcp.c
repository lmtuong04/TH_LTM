#include "test.h"
#include "server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "protocol.h"

static Device make_dev(DeviceType type, const char *id)
{
    Device d;
    memset(&d, 0, sizeof d);
    d.type = type;
    snprintf(d.id, sizeof d.id, "%s", id);
    return d;
}

static int bound_port(int fd)
{
    struct sockaddr_in addr;
    socklen_t len = sizeof addr;
    CHECK(getsockname(fd, (struct sockaddr *)&addr, &len) == 0);
    return ntohs(addr.sin_port);
}

static int tcp_client(int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    struct timeval tv = { .tv_sec = 2, .tv_usec = 0 };

    CHECK(fd >= 0);
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((uint16_t)port);
    CHECK(connect(fd, (struct sockaddr *)&addr, sizeof addr) == 0);
    return fd;
}

static void send_cmd(int fd, const char *s)
{
    CHECK(send_all(fd, s, strlen(s)) == 0);
}

/* Đọc 1 dòng phản hồi; trả 0 khi peer đóng, -1 khi timeout. */
static int recv_reply(int fd, char *buf, size_t cap)
{
    LineReader r;
    line_reader_init(&r, fd);
    return recv_line(&r, buf, cap);
}

static void test_handle_command_unit(void)
{
    static Device devs[1];
    DeviceList list = { devs, 1 };
    char reply[128];
    int sv[2];

    devs[0] = make_dev(DEV_LIGHT, "LIGHT-01");
    CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

    /* lệnh lạ -> 400 BAD_REQUEST, client vẫn tiếp tục */
    {
        char line[] = "FOO bar";
        CHECK(tcp_handle_command(sv[0], &list, line) == 1);
    }
    CHECK(recv_reply(sv[1], reply, sizeof reply) == 1);
    CHECK_STR(reply, "400 BAD_REQUEST");

    /* dòng rỗng -> 400 */
    {
        char line[] = "";
        CHECK(tcp_handle_command(sv[0], &list, line) == 1);
    }
    CHECK(recv_reply(sv[1], reply, sizeof reply) == 1);
    CHECK_STR(reply, "400 BAD_REQUEST");

    /* QUIT -> báo ngắt, không gửi phản hồi */
    {
        char line[] = "QUIT";
        CHECK(tcp_handle_command(sv[0], &list, line) == 0);
    }

    close(sv[0]);
    close(sv[1]);
}

static void test_two_clients(void)
{
    static Device devs[1];
    DeviceList list = { devs, 1 };
    TcpServer srv;
    char reply[128];
    int c1, c2;

    devs[0] = make_dev(DEV_LIGHT, "LIGHT-01");
    CHECK(tcp_server_start(&srv, &list, 0) == 0);

    /* 2 client cùng lúc: c2 không bị chặn khi c1 còn mở */
    c1 = tcp_client(bound_port(srv.fd));
    c2 = tcp_client(bound_port(srv.fd));

    send_cmd(c1, "PING\n");
    send_cmd(c2, "PING\n");
    CHECK(recv_reply(c1, reply, sizeof reply) == 1);
    CHECK_STR(reply, "400 BAD_REQUEST");
    CHECK(recv_reply(c2, reply, sizeof reply) == 1);
    CHECK_STR(reply, "400 BAD_REQUEST");

    /* QUIT -> server đóng kết nối đó, client kia vẫn sống */
    send_cmd(c1, "QUIT\n");
    CHECK(recv(c1, reply, sizeof reply, 0) == 0);   /* EOF (hoặc -1 nếu timeout) */

    send_cmd(c2, "PING\n");
    CHECK(recv_reply(c2, reply, sizeof reply) == 1);
    CHECK_STR(reply, "400 BAD_REQUEST");
    send_cmd(c2, "QUIT\n");

    tcp_server_stop(&srv);
    CHECK(srv.fd == -1);
    close(c1);
    close(c2);
}

int main(void)
{
    RUN(test_handle_command_unit);
    RUN(test_two_clients);
    return TEST_DONE();
}
