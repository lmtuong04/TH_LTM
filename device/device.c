#include "device.h"

static int ac_power(Device *d)
{
    if (!d->on)
        return 0;
    if (d->ac_mode == AC_COOL)
        return DEV_AC_COOL_W + DEV_AC_STEP_W * (26 - d->ac_temp);
    if (d->ac_mode == AC_HEAT)
        return DEV_AC_HEAT_W + DEV_AC_STEP_W * (d->ac_temp - 22);
    return DEV_AC_DRY_W;
}

int device_compute_power(Device *d)
{
    int w;

    switch (d->type) {
    case DEV_LIGHT:
        w = d->on ? DEV_LIGHT_W : 0;
        break;
    case DEV_FAN:
        if (!d->on)
            w = 0;
        else if (d->fan_speed <= 1)
            w = DEV_FAN_W1;
        else if (d->fan_speed == 2)
            w = DEV_FAN_W2;
        else
            w = DEV_FAN_W3;
        break;
    case DEV_AC:
        w = ac_power(d);
        break;
    default:
        w = 0;
        break;
    }
    d->power_w = w;
    return w;
}
