#include "test.h"
#include "device.h"

#include <string.h>

static Device dev(DeviceType type)
{
    Device d;
    memset(&d, 0, sizeof d);
    d.type = type;
    return d;
}

static void test_light(void)
{
    Device d = dev(DEV_LIGHT);

    d.on = 0;
    CHECK(device_compute_power(&d) == 0);
    d.on = 1;
    CHECK(device_compute_power(&d) == DEV_LIGHT_W);
    CHECK(d.power_w == DEV_LIGHT_W);
}

static void test_fan(void)
{
    Device d = dev(DEV_FAN);

    d.on = 0;
    d.fan_speed = 3;
    CHECK(device_compute_power(&d) == 0);

    d.on = 1;
    d.fan_speed = 1;
    CHECK(device_compute_power(&d) == DEV_FAN_W1);
    d.fan_speed = 2;
    CHECK(device_compute_power(&d) == DEV_FAN_W2);
    d.fan_speed = 3;
    CHECK(device_compute_power(&d) == DEV_FAN_W3);

    /* speed ngoài 1..3 vẫn quy về mức gần nhất */
    d.fan_speed = 0;
    CHECK(device_compute_power(&d) == DEV_FAN_W1);
    d.fan_speed = 99;
    CHECK(device_compute_power(&d) == DEV_FAN_W3);
}

static void test_ac(void)
{
    Device d = dev(DEV_AC);

    d.on = 0;
    d.ac_mode = AC_COOL;
    d.ac_temp = 26;
    CHECK(device_compute_power(&d) == 0);

    /* mát: 1000 W ở 26°C, +60 W mỗi độ thấp hơn */
    d.on = 1;
    d.ac_mode = AC_COOL;
    d.ac_temp = 26;
    CHECK(device_compute_power(&d) == 1000);
    d.ac_temp = 24;
    CHECK(device_compute_power(&d) == 1120);

    /* nóng: 1200 W ở 22°C, +60 W mỗi độ cao hơn */
    d.ac_mode = AC_HEAT;
    d.ac_temp = 22;
    CHECK(device_compute_power(&d) == 1200);
    d.ac_temp = 28;
    CHECK(device_compute_power(&d) == 1560);

    /* hút ẩm: 400 W, không đổi theo nhiệt độ */
    d.ac_mode = AC_DRY;
    d.ac_temp = 18;
    CHECK(device_compute_power(&d) == DEV_AC_DRY_W);
    d.ac_temp = 30;
    CHECK(device_compute_power(&d) == DEV_AC_DRY_W);
}

static void test_set_power(void)
{
    Device d = dev(DEV_LIGHT);

    CHECK(device_set(&d, "power", "on") == ST_OK);
    CHECK(d.on == 1);
    CHECK(d.power_w == DEV_LIGHT_W);
    CHECK(device_set(&d, "power", "off") == ST_OK);
    CHECK(d.on == 0);
    CHECK(d.power_w == 0);

    CHECK(device_set(&d, "power", "maybe") == ST_INVALID_VALUE);
    CHECK(device_set(&d, "power", "ON") == ST_INVALID_VALUE);
    CHECK(d.on == 0); /* giá trị sai không đổi trạng thái */
}

static void test_set_speed(void)
{
    Device d = dev(DEV_FAN);

    CHECK(device_set(&d, "power", "on") == ST_OK);
    CHECK(device_set(&d, "speed", "1") == ST_OK);
    CHECK(d.fan_speed == 1 && d.power_w == DEV_FAN_W1);
    CHECK(device_set(&d, "speed", "3") == ST_OK);
    CHECK(d.fan_speed == 3 && d.power_w == DEV_FAN_W3);

    CHECK(device_set(&d, "speed", "0") == ST_INVALID_VALUE);
    CHECK(device_set(&d, "speed", "4") == ST_INVALID_VALUE);
    CHECK(device_set(&d, "speed", "x") == ST_INVALID_VALUE);
    CHECK(device_set(&d, "speed", "2x") == ST_INVALID_VALUE);
    CHECK(device_set(&d, "speed", "") == ST_INVALID_VALUE);
    CHECK(d.fan_speed == 3); /* lỗi không đổi trạng thái */

    /* speed chỉ áp dụng cho quạt */
    Device l = dev(DEV_LIGHT);
    CHECK(device_set(&l, "speed", "2") == ST_INVALID_VALUE);
    CHECK(device_set(&l, "mode", "cool") == ST_INVALID_VALUE);
    CHECK(device_set(&l, "temp", "24") == ST_INVALID_VALUE);
}

static void test_set_mode_temp(void)
{
    Device d = dev(DEV_AC);

    CHECK(device_set(&d, "power", "on") == ST_OK);
    CHECK(device_set(&d, "mode", "cool") == ST_OK);
    CHECK(d.ac_mode == AC_COOL);
    CHECK(device_set(&d, "temp", "24") == ST_OK);
    CHECK(d.ac_temp == 24);
    CHECK(d.power_w == 1120);

    CHECK(device_set(&d, "mode", "heat") == ST_OK);
    CHECK(device_set(&d, "mode", "dry") == ST_OK);
    CHECK(device_set(&d, "mode", "auto") == ST_INVALID_VALUE);

    /* temp 18..30 mới hợp lệ */
    CHECK(device_set(&d, "temp", "18") == ST_OK);
    CHECK(device_set(&d, "temp", "30") == ST_OK);
    CHECK(device_set(&d, "temp", "17") == ST_INVALID_VALUE);
    CHECK(device_set(&d, "temp", "31") == ST_INVALID_VALUE);
    CHECK(device_set(&d, "temp", "24.5") == ST_INVALID_VALUE);
    CHECK(device_set(&d, "temp", "abc") == ST_INVALID_VALUE);
    CHECK(d.ac_temp == 30); /* lỗi không đổi trạng thái */
}

static void test_set_unknown(void)
{
    Device d = dev(DEV_LIGHT);

    CHECK(device_set(&d, "color", "red") == ST_BAD_REQUEST);
    CHECK(device_set(&d, NULL, "on") == ST_BAD_REQUEST);
    CHECK(device_set(&d, "power", NULL) == ST_BAD_REQUEST);
}

static void test_to_string(void)
{
    char buf[128];
    Device d;

    memset(&d, 0, sizeof d);
    d.type = DEV_LIGHT;
    d.on = 1;
    d.power_w = DEV_LIGHT_W;
    CHECK(device_to_string(&d, buf, sizeof buf) > 0);
    CHECK_STR(buf, "type=light on=1 power=40");

    d.type = DEV_FAN;
    d.on = 1;
    d.fan_speed = 2;
    d.power_w = DEV_FAN_W2;
    CHECK(device_to_string(&d, buf, sizeof buf) > 0);
    CHECK_STR(buf, "type=fan on=1 speed=2 power=45");

    d.type = DEV_AC;
    d.on = 1;
    d.ac_mode = AC_COOL;
    d.ac_temp = 24;
    d.power_w = 1120;
    CHECK(device_to_string(&d, buf, sizeof buf) > 0);
    CHECK_STR(buf, "type=ac on=1 mode=cool temp=24 power=1120");

    /* buffer quá nhỏ -> -1 */
    CHECK(device_to_string(&d, buf, 10) == -1);

    CHECK_STR(device_type_name(DEV_LIGHT), "light");
    CHECK_STR(device_type_name(DEV_FAN), "fan");
    CHECK_STR(device_type_name(DEV_AC), "ac");
    CHECK_STR(ac_mode_name(AC_COOL), "cool");
    CHECK_STR(ac_mode_name(AC_HEAT), "heat");
    CHECK_STR(ac_mode_name(AC_DRY), "dry");
}

int main(void)
{
    RUN(test_light);
    RUN(test_fan);
    RUN(test_ac);
    RUN(test_set_power);
    RUN(test_set_speed);
    RUN(test_set_mode_temp);
    RUN(test_set_unknown);
    RUN(test_to_string);
    return TEST_DONE();
}
