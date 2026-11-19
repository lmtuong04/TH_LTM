#include "test.h"
#include "hw.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Chạy fn() với bảng trạng thái ghi vào memstream, trả về buffer đã đọc.
 * Caller phải free(*out). */
static char *capture(const DeviceList *list, void (*trigger)(const Device *),
                     const Device *arg, size_t *out_len)
{
    char *buf = NULL;
    size_t size = 0;
    FILE *f;

    hw_init(list);
    f = open_memstream(&buf, &size);
    CHECK(f != NULL);
    hw_set_output(f);
    trigger(arg);
    fclose(f);
    hw_set_output(NULL);
    hw_close();

    *out_len = size;
    return buf;
}

static Device make_dev(DeviceType type, const char *id)
{
    Device d;
    memset(&d, 0, sizeof d);
    d.type = type;
    snprintf(d.id, sizeof d.id, "%s", id);
    return d;
}

static void test_table_all_types(void)
{
    Device devs[3];
    DeviceList list = { devs, 3 };
    char *out;
    size_t len;

    devs[0] = make_dev(DEV_LIGHT, "LIGHT-01");
    CHECK(device_set(&devs[0], "power", "on") == ST_OK);

    devs[1] = make_dev(DEV_FAN, "FAN-01");
    CHECK(device_set(&devs[1], "power", "on") == ST_OK);
    CHECK(device_set(&devs[1], "speed", "2") == ST_OK);

    devs[2] = make_dev(DEV_AC, "AC-01");
    CHECK(device_set(&devs[2], "power", "on") == ST_OK);
    CHECK(device_set(&devs[2], "mode", "cool") == ST_OK);
    CHECK(device_set(&devs[2], "temp", "24") == ST_OK);

    /* Mỗi lần device_set() xong, caller gọi hw_* -> bảng in lại */
    out = capture(&list, hw_light, &devs[0], &len);
    CHECK(out != NULL);
    CHECK(strstr(out, "ID") != NULL);
    CHECK(strstr(out, "CONG SUAT") != NULL);
    CHECK(strstr(out, "LIGHT-01") != NULL);
    CHECK(strstr(out, "den") != NULL);
    CHECK(strstr(out, "40 W") != NULL);
    CHECK(strstr(out, "FAN-01") != NULL);
    CHECK(strstr(out, "quat") != NULL);
    CHECK(strstr(out, "so 2") != NULL);
    CHECK(strstr(out, "45 W") != NULL);
    CHECK(strstr(out, "AC-01") != NULL);
    CHECK(strstr(out, "dhoa") != NULL);
    CHECK(strstr(out, "lam mat 24") != NULL);
    CHECK(strstr(out, "1120 W") != NULL);
    free(out);
}

static void test_table_off_and_ac_modes(void)
{
    Device devs[2];
    DeviceList list = { devs, 2 };
    char *out;
    size_t len;

    devs[0] = make_dev(DEV_LIGHT, "LIGHT-02"); /* tắt mặc định */

    devs[1] = make_dev(DEV_AC, "AC-02");
    CHECK(device_set(&devs[1], "power", "on") == ST_OK);
    CHECK(device_set(&devs[1], "mode", "heat") == ST_OK);
    CHECK(device_set(&devs[1], "temp", "28") == ST_OK);

    out = capture(&list, hw_ac, &devs[1], &len);
    CHECK(strstr(out, "TAT") != NULL);
    CHECK(strstr(out, "0 W") != NULL);
    CHECK(strstr(out, "nong 28") != NULL);
    CHECK(strstr(out, "1560 W") != NULL);
    free(out);

    /* quạt tắt */
    {
        Device fan = make_dev(DEV_FAN, "FAN-02");
        DeviceList l2 = { &fan, 1 };
        out = capture(&l2, hw_fan, &fan, &len);
        CHECK(strstr(out, "TAT") != NULL);
        CHECK(strstr(out, "0 W") != NULL);
        free(out);
    }
}

static void test_empty_and_close(void)
{
    DeviceList empty = { NULL, 0 };
    char *out;
    size_t len;

    out = capture(&empty, hw_light, NULL, &len);
    CHECK(strstr(out, "CONG SUAT") != NULL);
    CHECK(strstr(out, "LIGHT") == NULL);
    free(out);

    /* hw_init trả 0; hw_close reset con trỏ, gọi lại không crash */
    CHECK(hw_init(&empty) == 0);
    hw_close();
    hw_light(NULL); /* in ra stdout, không capture */
}

int main(void)
{
    RUN(test_table_all_types);
    RUN(test_table_off_and_ac_modes);
    RUN(test_empty_and_close);
    return TEST_DONE();
}
