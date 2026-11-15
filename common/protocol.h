/* Hợp đồng chung giữa device_sim và smarthome_app (xem PLAN_2.md mục 2).
 *
 * TCP là luồng byte: một lần recv() có thể trả nửa dòng, hoặc 2 dòng dính
 * nhau. Vì vậy mọi việc đọc lệnh/phản hồi đi qua LineReader + recv_line(),
 * phần dư sau '\n' được giữ lại cho lần gọi sau. */
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stddef.h>

#define PROTO_UDP_PORT  50000      /* quét: SCAN / DEVICE */
#define PROTO_TCP_PORT  50001      /* kết nối & điều khiển */
#define PROTO_MAX_LINE  512        /* tính cả '\n' */
#define PROTO_MAX_ARGS  8          /* lệnh dài nhất (SET, PASSWD, STATS) có 6 trường */

/* Mã phản hồi */
#define ST_OK             200
#define ST_BAD_REQUEST    400
#define ST_AUTH_FAILED    401
#define ST_FORBIDDEN      403
#define ST_NOT_FOUND      404
#define ST_INVALID_VALUE  422
#define ST_LOCKED         429

/* Chữ đi kèm mã lỗi: 401 -> "AUTH_FAILED". Mã lạ -> "UNKNOWN". */
const char *proto_status_text(int code);

/* Gửi đủ len byte (lặp tới khi hết, bỏ qua EINTR).
 * Trả 0 nếu xong, -1 nếu lỗi (errno giữ nguyên, ví dụ EPIPE). */
int send_all(int fd, const void *buf, size_t len);

/* Định dạng kiểu printf rồi gửi kèm '\n'.
 * Trả -1 với errno = EMSGSIZE nếu dòng dài quá PROTO_MAX_LINE. */
int send_line(int fd, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

typedef struct {
    int    fd;
    char   buf[PROTO_MAX_LINE];
    size_t len;                    /* số byte đang chờ trong buf */
} LineReader;

void line_reader_init(LineReader *r, int fd);

/* Đọc 1 dòng vào out (bỏ '\n' và '\r' ở cuối).
 * Trả  1: có 1 dòng
 *      0: bên kia đóng kết nối (phần dòng dở dang bị bỏ)
 *     -1: lỗi; errno = EAGAIN nếu hết SO_RCVTIMEO (phần đã nhận vẫn giữ lại),
 *         EMSGSIZE nếu dòng dài quá PROTO_MAX_LINE hoặc không vừa out. */
int recv_line(LineReader *r, char *out, size_t cap);

/* Tách line tại chỗ theo đúng 1 dấu cách, ghi con trỏ vào argv.
 * Trả số trường (0 nếu dòng rỗng), -1 nếu sai cú pháp: có trường rỗng
 * (2 dấu cách liền, dấu cách đầu/cuối dòng) hoặc nhiều hơn max trường. */
int proto_split(char *line, char *argv[], int max);

#endif
