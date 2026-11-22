/* device_sim – chạy trên Raspberry Pi (HW=gpio) hoặc laptop (bản console).
 * A6: UDP discovery :50000 trả DEVICE khi nhận SCAN.
 * Danh sách thiết bị đang hardcode tạm; A10 sẽ đọc devices.conf. */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "device.h"
#include "hw.h"
#include "protocol.h"
#include "server.h"

static Device make_dev(DeviceType type, const char *id, const char *name,
                       const int *gpio, int ngpio)
{
    Device d;
    memset(&d, 0, sizeof d);
    snprintf(d.id, sizeof d.id, "%s", id);
    snprintf(d.name, sizeof d.name, "%s", name);
    snprintf(d.password, sizeof d.password, "%s", "1234");
    d.type = type;
    d.fan_speed = 1;
    d.ac_mode = AC_COOL;
    d.ac_temp = 26;
    for (int i = 0; i < ngpio && i < DEV_GPIO_MAX; i++)
        d.gpio[i] = gpio[i];
    return d;
}

int main(void)
{
    static Device devs[3];
    static const int gpio_light1[] = {17};
    static const int gpio_light2[] = {27};
    static const int gpio_fan[]    = {18};
    static const int gpio_ac[]     = {5, 6, 13};
    DeviceList list = { devs, 3 };
    UdpServer udp;

    devs[0] = make_dev(DEV_LIGHT, "LIGHT-01", "Den_phong_khach", gpio_light1, 1);
    devs[1] = make_dev(DEV_FAN,   "FAN-01",   "Quat_phong_khach", gpio_fan,    1);
    devs[2] = make_dev(DEV_AC,    "AC-01",    "Dieu_hoa_phong_ngu", gpio_ac,   3);

    hw_init(&list);

    if (udp_server_start(&udp, &list, PROTO_UDP_PORT) != 0) {
        perror("udp_server_start");
        hw_close();
        return 1;
    }

    printf("device_sim: UDP discovery :%d (ban console, Ctrl+C de thoat)\n",
           PROTO_UDP_PORT);
    for (;;)
        pause();

    /* khong toi day duoc */
    udp_server_stop(&udp);
    hw_close();
    return 0;
}
