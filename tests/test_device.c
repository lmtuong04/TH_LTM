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

int main(void)
{
    RUN(test_light);
    RUN(test_fan);
    RUN(test_ac);
    return TEST_DONE();
}
