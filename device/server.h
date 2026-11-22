/* Server phía device_sim: UDP discovery (cổng PROTO_UDP_PORT) trả lời
 * "DEVICE ..." khi nhận "SCAN <app_id>". TCP server thêm ở A7. */
#ifndef SERVER_H
#define SERVER_H

#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "device.h"

/* 1 tiến trình UDP discovery: socket + luồng recvfrom. */
typedef struct {
    int          fd;        /* -1 khi chưa mở/đã đóng */
    pthread_t    thread;
    volatile int running;
    const DeviceList *list;
} UdpServer;

/* Mở socket UDP, SO_REUSEADDR + timeout nhận 200ms (để stop() join nhanh).
 * Trả fd >= 0, -1 nếu lỗi (errno giữ nguyên). port=0 -> hệ điều hành chọn. */
int udp_open(int port);

/* Xử lý 1 datagram: nếu là "SCAN [app_id]" thì gửi "DEVICE <id> <type>
 * <tcp_port> <name>" cho từng thiết bị về địa chỉ from.
 * Datagram khác bị bỏ qua (UDP không có mã lỗi). */
void udp_handle_datagram(int sock, const DeviceList *list,
                         char *buf, ssize_t len,
                         const struct sockaddr *from, socklen_t fromlen);

/* Vòng lặp nhận datagram -> udp_handle_datagram. Chặn tới khi *stop == 0
 * (kiểm tra sau mỗi lần recvfrom/timeout). */
void udp_serve_loop(int sock, const DeviceList *list, volatile int *stop);

/* Mở socket + tạo luồng udp_serve_loop. 0 ok, -1 lỗi. */
int  udp_server_start(UdpServer *srv, const DeviceList *list, int port);
void udp_server_stop(UdpServer *srv);

#endif
