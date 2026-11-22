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

static Device make_dev(DeviceType type, const char *id, const char *name)
{
    Device d;
    memset(&d, 0, sizeof d);
    snprintf(d.id, sizeof d.id, "%s", id);
    snprintf(d.name, sizeof d.name, "%s", name);
    d.type = type;
    return d;
}

static int bound_port(int fd)
{
    struct sockaddr_in addr;
    socklen_t len = sizeof addr;
    CHECK(getsockname(fd, (struct sockaddr *)&addr, &len) == 0);
    return ntohs(addr.sin_port);
}

/* Client UDP nối tới 127.0.0.1:port, timeout nhận 300ms. */
static int udp_client(int port)
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct timeval tv = { .tv_sec = 0, .tv_usec = 300 * 1000 };
    struct sockaddr_in addr;

    CHECK(fd >= 0);
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((uint16_t)port);
    CHECK(connect(fd, (struct sockaddr *)&addr, sizeof addr) == 0);
    return fd;
}

static void udp_send(int fd, const char *s)
{
    CHECK(send(fd, s, strlen(s), 0) == (ssize_t)strlen(s));
}

/* Nhận 1 datagram, bỏ '\n' cuối. Trả độ dài, -1 nếu timeout/lỗi. */
static int udp_recv(int fd, char *buf, size_t cap)
{
    ssize_t n = recv(fd, buf, cap - 1, 0);
    if (n < 0)
        return -1;
    buf[n] = '\0';
    if (n > 0 && buf[n - 1] == '\n')
        buf[--n] = '\0';
    return (int)n;
}

/* Đẩy 1 datagram từ client vào sock server rồi gọi handler trực tiếp. */
static void pump_once(int srv, int cli, const DeviceList *list)
{
    char buf[PROTO_MAX_LINE];
    struct sockaddr_storage from;
    socklen_t fromlen = sizeof from;
    ssize_t len = recvfrom(srv, buf, sizeof buf, 0,
                           (struct sockaddr *)&from, &fromlen);
    CHECK(len > 0);
    udp_handle_datagram(srv, list, buf, len,
                        (struct sockaddr *)&from, fromlen);
}

static void test_scan_replies(void)
{
    static Device devs[3];
    DeviceList list = { devs, 3 };
    char buf[256];
    int srv, cli;

    devs[0] = make_dev(DEV_LIGHT, "LIGHT-01", "Den_phong_khach");
    devs[1] = make_dev(DEV_FAN,   "FAN-01",   "Quat_phong_khach");
    devs[2] = make_dev(DEV_AC,    "AC-01",    "Dieu_hoa_phong_ngu");

    srv = udp_open(0);
    CHECK(srv >= 0);
    cli = udp_client(bound_port(srv));

    udp_send(cli, "SCAN test-app-123\n");
    pump_once(srv, cli, &list);

    CHECK(udp_recv(cli, buf, sizeof buf) > 0);
    CHECK_STR(buf, "DEVICE LIGHT-01 light 50001 Den_phong_khach");
    CHECK(udp_recv(cli, buf, sizeof buf) > 0);
    CHECK_STR(buf, "DEVICE FAN-01 fan 50001 Quat_phong_khach");
    CHECK(udp_recv(cli, buf, sizeof buf) > 0);
    CHECK_STR(buf, "DEVICE AC-01 ac 50001 Dieu_hoa_phong_ngu");

    /* "SCAN" không cần app_id cũng trả lời */
    udp_send(cli, "SCAN\n");
    pump_once(srv, cli, &list);
    CHECK(udp_recv(cli, buf, sizeof buf) > 0);
    CHECK(strncmp(buf, "DEVICE LIGHT-01 ", 17) == 0);
    /* hút nốt 2 phản hồi còn lại để socket sạch */
    CHECK(udp_recv(cli, buf, sizeof buf) > 0);
    CHECK(udp_recv(cli, buf, sizeof buf) > 0);

    close(cli);
    close(srv);
}

static void test_scan_ignores_others(void)
{
    static Device devs[1];
    DeviceList list = { devs, 1 };
    char buf[256];
    int srv, cli;

    devs[0] = make_dev(DEV_LIGHT, "LIGHT-01", "Den");

    srv = udp_open(0);
    cli = udp_client(bound_port(srv));

    /* Datagram không phải SCAN -> không phản hồi */
    udp_send(cli, "HELLO\n");
    pump_once(srv, cli, &list);
    CHECK(udp_recv(cli, buf, sizeof buf) == -1);
    CHECK(errno == EAGAIN || errno == EWOULDBLOCK);

    /* SCAN sai cú pháp (2 dấu cách) -> proto_split -1 -> bỏ qua */
    udp_send(cli, "SCAN  app\n");
    pump_once(srv, cli, &list);
    CHECK(udp_recv(cli, buf, sizeof buf) == -1);

    close(cli);
    close(srv);
}

static void test_server_thread(void)
{
    static Device devs[1];
    DeviceList list = { devs, 1 };
    UdpServer srv;
    char buf[256];
    int cli;

    devs[0] = make_dev(DEV_LIGHT, "LIGHT-01", "Den");

    /* port 0 -> ephemeral, kiểm tra qua getsockname */
    CHECK(udp_server_start(&srv, &list, 0) == 0);
    cli = udp_client(bound_port(srv.fd));

    udp_send(cli, "SCAN app-x\n");
    CHECK(udp_recv(cli, buf, sizeof buf) > 0);
    CHECK_STR(buf, "DEVICE LIGHT-01 light 50001 Den");

    udp_server_stop(&srv);
    CHECK(srv.fd == -1);
    close(cli);
}

int main(void)
{
    RUN(test_scan_replies);
    RUN(test_scan_ignores_others);
    RUN(test_server_thread);
    return TEST_DONE();
}
