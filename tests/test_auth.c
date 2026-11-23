#include "test.h"
#include "server.h"

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "protocol.h"

static Device make_dev(DeviceType type, const char *id, const char *pass)
{
    Device d;
    memset(&d, 0, sizeof d);
    d.type = type;
    snprintf(d.id, sizeof d.id, "%s", id);
    snprintf(d.name, sizeof d.name, "%s", id);
    snprintf(d.password, sizeof d.password, "%s", pass);
    return d;
}

static void test_auth_table(void)
{
    AuthTable t;
    char tok1[AUTH_TOKEN_LEN + 1], tok2[AUTH_TOKEN_LEN + 1];

    auth_table_init(&t);

    /* phát hành + kiểm tra */
    CHECK(auth_issue(&t, "app-alpha", tok1, sizeof tok1) == 0);
    CHECK(strlen(tok1) == AUTH_TOKEN_LEN);
    CHECK(auth_check(&t, "app-alpha", tok1) == 1);

    /* token gắn app_id: sai app_id -> 0 */
    CHECK(auth_check(&t, "app-beta", tok1) == 0);
    /* token sai -> 0 */
    CHECK(auth_check(&t, "app-alpha", "deadbeef") == 0);
    CHECK(auth_check(&t, NULL, tok1) == 0);
    CHECK(auth_check(&t, "app-alpha", NULL) == 0);

    /* token thứ 2 khác token 1, app_id khác */
    CHECK(auth_issue(&t, "app-beta", tok2, sizeof tok2) == 0);
    CHECK(strcmp(tok1, tok2) != 0);
    CHECK(auth_check(&t, "app-beta", tok2) == 1);
    CHECK(auth_check(&t, "app-alpha", tok2) == 0);

    /* revoke_app chỉ xoá token của app đó */
    CHECK(auth_revoke_app(&t, "app-alpha") == 1);
    CHECK(auth_check(&t, "app-alpha", tok1) == 0);
    CHECK(auth_check(&t, "app-beta", tok2) == 1);
    CHECK(auth_revoke_app(&t, "app-beta") == 1);
    CHECK(auth_revoke_app(&t, "app-beta") == 0);

    /* app_id quá dài / out nhỏ -> -1 */
    {
        char long_id[AUTH_APP_ID_LEN + 1];
        memset(long_id, 'a', sizeof long_id - 1);
        long_id[sizeof long_id - 1] = '\0';
        CHECK(auth_issue(&t, long_id, tok1, sizeof tok1) == -1);
        CHECK(auth_issue(&t, "app", tok1, 4) == -1);
        CHECK(auth_issue(&t, NULL, tok1, sizeof tok1) == -1);
    }
}

static void test_connect_info_flow(void)
{
    static Device devs[2];
    DeviceList list = { devs, 2 };
    AuthTable auth;
    TcpClient c;
    char reply[256];
    char token[AUTH_TOKEN_LEN + 1] = "";
    int sv[2];

    devs[0] = make_dev(DEV_LIGHT, "LIGHT-01", "1234");
    devs[0].on = 1;
    devs[0].power_w = DEV_LIGHT_W;
    devs[1] = make_dev(DEV_FAN, "FAN-01", "5678");

    auth_table_init(&auth);
    CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
    c.fd = sv[0];
    c.list = &list;
    c.auth = &auth;

    /* CONNECT sai số tham số -> 400 */
    {
        char line[] = "CONNECT LIGHT-01 app1";
        CHECK(tcp_handle_command(&c, line) == 1);
    }
    CHECK(recv(sv[1], reply, sizeof reply - 1, 0) > 0);
    reply[strcspn(reply, "\n")] = '\0';
    CHECK_STR(reply, "400 BAD_REQUEST");

    /* CONNECT id không tồn tại -> 404 */
    {
        char line[] = "CONNECT NOPE app1 1234";
        CHECK(tcp_handle_command(&c, line) == 1);
    }
    CHECK(recv(sv[1], reply, sizeof reply - 1, 0) > 0);
    reply[strcspn(reply, "\n")] = '\0';
    CHECK_STR(reply, "404 NOT_FOUND");

    /* CONNECT sai mật khẩu -> 401 */
    {
        char line[] = "CONNECT LIGHT-01 app1 9999";
        CHECK(tcp_handle_command(&c, line) == 1);
    }
    CHECK(recv(sv[1], reply, sizeof reply - 1, 0) > 0);
    reply[strcspn(reply, "\n")] = '\0';
    CHECK_STR(reply, "401 AUTH_FAILED");

    /* CONNECT đúng -> 200 <token 32 hex> */
    {
        char line[] = "CONNECT LIGHT-01 app-alpha 1234";
        CHECK(tcp_handle_command(&c, line) == 1);
    }
    CHECK(recv(sv[1], reply, sizeof reply - 1, 0) > 0);
    reply[strcspn(reply, "\n")] = '\0';
    CHECK(strncmp(reply, "200 ", 4) == 0);
    CHECK(strlen(reply) == 4 + AUTH_TOKEN_LEN);
    strcpy(token, reply + 4);

    /* INFO token sai -> 403 */
    {
        char line[] = "INFO LIGHT-01 app-alpha deadbeefdeadbeefdeadbeefdeadbeef";
        CHECK(tcp_handle_command(&c, line) == 1);
    }
    CHECK(recv(sv[1], reply, sizeof reply - 1, 0) > 0);
    reply[strcspn(reply, "\n")] = '\0';
    CHECK_STR(reply, "403 FORBIDDEN");

    /* INFO sai app_id -> 403 */
    {
        char line[128];
        snprintf(line, sizeof line, "INFO LIGHT-01 app-khac %s", token);
        CHECK(tcp_handle_command(&c, line) == 1);
    }
    CHECK(recv(sv[1], reply, sizeof reply - 1, 0) > 0);
    reply[strcspn(reply, "\n")] = '\0';
    CHECK_STR(reply, "403 FORBIDDEN");

    /* INFO đúng token -> 200 + mô tả thiết bị */
    {
        char line[128];
        snprintf(line, sizeof line, "INFO LIGHT-01 app-alpha %s", token);
        CHECK(tcp_handle_command(&c, line) == 1);
    }
    CHECK(recv(sv[1], reply, sizeof reply - 1, 0) > 0);
    reply[strcspn(reply, "\n")] = '\0';
    CHECK(strncmp(reply, "200 type=light on=1 power=40", 30) == 0);

    /* INFO id không tồn tại -> 404 (trước cả kiểm token) */
    {
        char line[] = "INFO NOPE app-alpha deadbeef";
        CHECK(tcp_handle_command(&c, line) == 1);
    }
    CHECK(recv(sv[1], reply, sizeof reply - 1, 0) > 0);
    reply[strcspn(reply, "\n")] = '\0';
    CHECK_STR(reply, "404 NOT_FOUND");

    close(sv[0]);
    close(sv[1]);
}

int main(void)
{
    RUN(test_auth_table);
    RUN(test_connect_info_flow);
    return TEST_DONE();
}
