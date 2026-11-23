/* Xác thực phía device_sim: CONNECT kiểm tra mật khẩu rồi cấp token ngẫu
 * nhiên gắn với app_id. Mọi lệnh sau đó mang <app_id> <token> và được kiểm
 * bằng auth_check(). Token lưu trong bảng cố định, bảo vệ bằng mutex vì
 * mỗi client TCP chạy một luồng. */
#ifndef AUTH_H
#define AUTH_H

#include <pthread.h>
#include <stddef.h>

#define AUTH_TOKEN_LEN   32   /* 16 byte ngẫu nhiên -> 32 ký tự hex */
#define AUTH_APP_ID_LEN  33
#define AUTH_MAX_TOKENS  128

typedef struct {
    char token[AUTH_TOKEN_LEN + 1];
    char app_id[AUTH_APP_ID_LEN + 1];
} AuthToken;

typedef struct {
    AuthToken        tokens[AUTH_MAX_TOKENS];
    size_t           n;
    pthread_mutex_t  lock;
} AuthTable;

void auth_table_init(AuthTable *t);

/* Phát hành token mới gắn app_id, ghi vào out (cần >= AUTH_TOKEN_LEN+1).
 * Trả 0 ok, -1 nếu bảng đầy / app_id quá dài / không sinh được ngẫu nhiên. */
int auth_issue(AuthTable *t, const char *app_id, char *out, size_t cap);

/* 1 nếu tồn tại token khớp cả token lẫn app_id, ngược lại 0. */
int auth_check(AuthTable *t, const char *app_id, const char *token);

/* Xoá mọi token của app_id (gọi khi PASSWD đổi mật khẩu, A9).
 * Trả số token đã huỷ. */
int auth_revoke_app(AuthTable *t, const char *app_id);

#endif
