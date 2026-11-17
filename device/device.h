/* Mô hình thiết bị phía device_sim: struct Device + tính công suất giả định.
 * Xem bảng công suất trong PLAN_2.md mục 2. */
#ifndef DEVICE_H
#define DEVICE_H

#include <stddef.h>

#include "protocol.h"

#define DEV_ID_LEN     32
#define DEV_NAME_LEN   64
#define DEV_PASSWD_LEN 64
#define DEV_GPIO_MAX   3

#define DEV_LIGHT_W      40   /* đèn bật */
#define DEV_FAN_W1       35   /* quạt số 1 */
#define DEV_FAN_W2       45   /* quạt số 2 */
#define DEV_FAN_W3       60   /* quạt số 3 */
#define DEV_AC_COOL_W    1000 /* điều hòa mát ở 26°C */
#define DEV_AC_HEAT_W    1200 /* điều hòa nóng ở 22°C */
#define DEV_AC_DRY_W     400  /* điều hòa hút ẩm */
#define DEV_AC_STEP_W    60   /* mỗi độ lệch nhiệt độ */
#define DEV_AC_TEMP_MIN  18
#define DEV_AC_TEMP_MAX  30

typedef enum { DEV_LIGHT, DEV_FAN, DEV_AC } DeviceType;
typedef enum { AC_COOL, AC_HEAT, AC_DRY } AcMode;

typedef struct {
    char        id[DEV_ID_LEN];
    char        name[DEV_NAME_LEN];
    DeviceType  type;
    int         gpio[DEV_GPIO_MAX]; /* chân BCM trên mô hình */
    int         on;
    int         fan_speed;          /* 1..3, chỉ dùng cho quạt */
    AcMode      ac_mode;            /* chỉ dùng cho điều hòa */
    int         ac_temp;            /* 18..30 °C, chỉ dùng cho điều hòa */
    int         power_w;            /* công suất giả định hiện tại */
    char        password[DEV_PASSWD_LEN];
} Device;

/* Tính lại power_w theo trạng thái hiện tại của d rồi gán vào d->power_w.
 * Trả về power_w mới. */
int device_compute_power(Device *d);

const char *device_type_name(DeviceType t);   /* "light" | "fan" | "ac" */
const char *ac_mode_name(AcMode m);           /* "cool" | "heat" | "dry" */

/* Áp 1 cặp key/value từ lệnh SET (power|speed|mode|temp) vào d,
 * rồi tính lại công suất. Trả mã phản hồi:
 *   ST_OK (200)            thành công
 *   ST_BAD_REQUEST (400)   key không tồn tại trong giao thức
 *   ST_INVALID_VALUE (422) value sai, sai kiểu thiết bị, hoặc ngoài khoảng */
int device_set(Device *d, const char *key, const char *value);

/* Ghi mô tả trạng thái dạng "key=value ..." cho lệnh INFO, ví dụ:
 *   type=ac on=1 mode=cool temp=24 power=1120
 * Trả số byte ghi (không tính '\0'), -1 nếu out không đủ chỗ. */
int device_to_string(const Device *d, char *out, size_t cap);

#endif
