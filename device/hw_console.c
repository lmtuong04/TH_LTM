/* Bản console của lớp phần cứng: không đụng GPIO, chỉ in bảng trạng thái
 * ra FILE* mỗi khi có thiết bị thay đổi. Dùng cho laptop, WSL và CI.
 * Bản mô hình thật là hw_gpio.c (make HW=gpio). */
#include "hw.h"

static const DeviceList *g_list;
static FILE *g_out;

static FILE *out(void)
{
    return g_out ? g_out : stdout;
}

static const char *type_vn(DeviceType t)
{
    switch (t) {
    case DEV_LIGHT: return "den";
    case DEV_FAN:   return "quat";
    case DEV_AC:    return "dhoa";
    default:        return "?";
    }
}

static const char *ac_mode_vn(AcMode m)
{
    switch (m) {
    case AC_COOL: return "lam mat";
    case AC_HEAT: return "nong";
    case AC_DRY:  return "hut am";
    default:      return "?";
    }
}

static void status_vn(const Device *d, char *buf, size_t cap)
{
    if (!d->on) {
        snprintf(buf, cap, "TAT");
    } else if (d->type == DEV_FAN) {
        snprintf(buf, cap, "BAT  so %d", d->fan_speed);
    } else if (d->type == DEV_AC) {
        snprintf(buf, cap, "BAT  %s %d°C", ac_mode_vn(d->ac_mode), d->ac_temp);
    } else {
        snprintf(buf, cap, "BAT");
    }
}

static void print_table(void)
{
    char st[64];
    size_t i;

    fprintf(out(), "%-10s %-6s %-22s %s\n", "ID", "LOAI", "TRANG THAI", "CONG SUAT");
    if (!g_list)
        return;
    for (i = 0; i < g_list->n; i++) {
        const Device *d = &g_list->items[i];
        status_vn(d, st, sizeof st);
        fprintf(out(), "%-10s %-6s %-22s %d W\n",
                d->id, type_vn(d->type), st, d->power_w);
    }
}

int hw_init(const DeviceList *list)
{
    g_list = list;
    return 0;
}

void hw_set_output(FILE *f)
{
    g_out = f;
}

void hw_light(const Device *d)
{
    (void)d;
    print_table();
}

void hw_fan(const Device *d)
{
    (void)d;
    print_table();
}

void hw_ac(const Device *d)
{
    (void)d;
    print_table();
}

void hw_close(void)
{
    g_list = NULL;
    g_out = NULL;
}
