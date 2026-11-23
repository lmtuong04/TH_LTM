/* device_sim – chạy trên Raspberry Pi (HW=gpio) hoặc laptop (bản console).
 * A6: UDP discovery :50000 trả DEVICE khi nhận SCAN.
 * A7: TCP :50001, mỗi client 1 luồng (lệnh thật thêm ở A8/A11).
 * Danh sách thiết bị đang hardcode tạm; A10 sẽ đọc devices.conf. */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "auth.h"
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
    static Device devs[5];
    static const int gpio_light1[] = {17};
    static const int gpio_light2[] = {27};
    static const int gpio_light3[] = {22};
    static const int gpio_fan[]    = {18};
    static const int gpio_ac[]     = {5, 6, 13};
    DeviceList list = { devs, 5 };
    AuthTable auth;
    UdpServer udp;
    TcpServer tcp;

    devs[0] = make_dev(DEV_LIGHT, "LIGHT-01", "Den_phong_khach", gpio_light1, 1);
    devs[1] = make_dev(DEV_LIGHT, "LIGHT-02", "Den_phong_ngu",   gpio_light2, 1);
    devs[2] = make_dev(DEV_LIGHT, "LIGHT-03", "Den_bep",         gpio_light3, 1);
    devs[3] = make_dev(DEV_FAN,   "FAN-01",   "Quat_phong_khach", gpio_fan,    1);
    devs[4] = make_dev(DEV_AC,    "AC-01",    "Dieu_hoa_phong_ngu", gpio_ac,   3);

    hw_init(&list);

    auth_table_init(&auth);
    if (udp_server_start(&udp, &list, PROTO_UDP_PORT) != 0 ||
        tcp_server_start(&tcp, &list, &auth, PROTO_TCP_PORT) != 0) {
        perror("server_start");
        hw_close();
        return 1;
    }

    printf("device_sim: UDP :%d + TCP :%d (ban console, Ctrl+C de thoat)\n",
           PROTO_UDP_PORT, PROTO_TCP_PORT);
    for (;;)
        pause();

    /* khong toi day duoc */
    tcp_server_stop(&tcp);
    udp_server_stop(&udp);
    hw_close();
    return 0;
}
