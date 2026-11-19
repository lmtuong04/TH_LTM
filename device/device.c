#include "device.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *device_type_name(DeviceType t)
{
    switch (t) {
    case DEV_LIGHT: return "light";
    case DEV_FAN:   return "fan";
    case DEV_AC:    return "ac";
    default:        return "unknown";
    }
}

const char *ac_mode_name(AcMode m)
{
    switch (m) {
    case AC_COOL: return "cool";
    case AC_HEAT: return "heat";
    case AC_DRY:  return "dry";
    default:      return "unknown";
    }
}

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

/* strtol nghiêm ngặt: phải có ít nhất 1 chữ số và hết chuỗi ngay sau số. */
static int parse_int(const char *s, long *out)
{
    char *end;
    long v;

    if (!s || !*s)
        return -1;
    v = strtol(s, &end, 10);
    if (end == s || *end != '\0')
        return -1;
    *out = v;
    return 0;
}

int device_set(Device *d, const char *key, const char *value)
{
    long v;

    if (!key || !value)
        return ST_BAD_REQUEST;

    if (strcmp(key, "power") == 0) {
        if (strcmp(value, "on") == 0)
            d->on = 1;
        else if (strcmp(value, "off") == 0)
            d->on = 0;
        else
            return ST_INVALID_VALUE;
    } else if (strcmp(key, "speed") == 0) {
        if (d->type != DEV_FAN)
            return ST_INVALID_VALUE;
        if (parse_int(value, &v) != 0 || v < 1 || v > 3)
            return ST_INVALID_VALUE;
        d->fan_speed = (int)v;
    } else if (strcmp(key, "mode") == 0) {
        if (d->type != DEV_AC)
            return ST_INVALID_VALUE;
        if (strcmp(value, "cool") == 0)
            d->ac_mode = AC_COOL;
        else if (strcmp(value, "heat") == 0)
            d->ac_mode = AC_HEAT;
        else if (strcmp(value, "dry") == 0)
            d->ac_mode = AC_DRY;
        else
            return ST_INVALID_VALUE;
    } else if (strcmp(key, "temp") == 0) {
        if (d->type != DEV_AC)
            return ST_INVALID_VALUE;
        if (parse_int(value, &v) != 0 ||
            v < DEV_AC_TEMP_MIN || v > DEV_AC_TEMP_MAX)
            return ST_INVALID_VALUE;
        d->ac_temp = (int)v;
    } else {
        return ST_BAD_REQUEST;
    }

    device_compute_power(d);
    return ST_OK;
}

int device_to_string(const Device *d, char *out, size_t cap)
{
    int n;

    switch (d->type) {
    case DEV_LIGHT:
        n = snprintf(out, cap, "type=light on=%d power=%d",
                     d->on, d->power_w);
        break;
    case DEV_FAN:
        n = snprintf(out, cap, "type=fan on=%d speed=%d power=%d",
                     d->on, d->fan_speed, d->power_w);
        break;
    case DEV_AC:
        n = snprintf(out, cap, "type=ac on=%d mode=%s temp=%d power=%d",
                     d->on, ac_mode_name(d->ac_mode), d->ac_temp, d->power_w);
        break;
    default:
        n = snprintf(out, cap, "type=unknown power=%d", d->power_w);
        break;
    }
    return (size_t)n >= cap ? -1 : n;
}
