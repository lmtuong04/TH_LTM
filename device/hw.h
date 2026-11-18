/* Lớp phần cứng: logic thiết bị không gọi GPIO trực tiếp, chỉ qua interface
 * này. Chọn bản cài đặt lúc link:
 *   make           -> hw_console.c (in bảng trạng thái, dùng cho laptop/CI)
 *   make HW=gpio   -> hw_gpio.c     (GPIO/PWM/I2C trên Raspberry Pi)
 *
 * Sau mỗi device_set() thành công, caller gọi hàm hw_* tương ứng loại
 * thiết bị để mô hình phần cứng đổi theo. */
#ifndef HW_H
#define HW_H

#include <stdio.h>

#include "device.h"

/* 0 nếu ok, -1 nếu lỗi (bản console luôn trả 0). */
int  hw_init(const DeviceList *list);
void hw_light(const Device *d);   /* đọc d->on, d->gpio */
void hw_fan(const Device *d);     /* đọc d->on, d->fan_speed */
void hw_ac(const Device *d);      /* đọc d->on, d->ac_mode, d->ac_temp */
void hw_close(void);

/* Chỉ bản console: đổi FILE* in bảng trạng thái (mặc định stdout).
 * Truyền NULL để quay về stdout. Bản gpio bỏ qua tham số này. */
void hw_set_output(FILE *out);

#endif
