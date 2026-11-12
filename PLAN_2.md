# Kế hoạch 4 tuần (bản 2) – Nhà mô hình thu nhỏ điều khiển qua mạng LAN bằng C

> **Khác với PLAN.md:** bản này có **nhà mô hình thu nhỏ thật** thay cho thiết bị chỉ có trên màn hình.
> - Đèn → LED · Quạt → motor DC mini 3 tốc độ · Điều hòa → LED RGB (theo chế độ) + màn LCD hiện nhiệt độ.
> - `device_sim` chạy trên **Raspberry Pi** gắn trong mô hình và điều khiển LED/motor qua chân GPIO.
> - `smarthome_app` chạy trên laptop, điều khiển mô hình qua Wi-Fi/LAN bằng socket UDP/TCP.
> - Điện năng là **số ước tính** theo công suất giả định của thiết bị thật (đèn 40 W, điều hòa 1000 W…), không đo điện của LED/motor.
> - Khi chưa có mô hình hoặc khi chạy test/CI, `device_sim` dùng **bản console** (in bảng trạng thái ra màn hình thay vì bật GPIO). Code còn lại giữ nguyên.
>
> 2 người · 4 tuần · mỗi người 5 commit/tuần (10 commit/tuần cho cả nhóm, gấp đôi mức tối thiểu).

## Yêu cầu môn học → cách đáp ứng

| Yêu cầu | Cách làm | Kiểm tra bằng |
|---|---|---|
| Repo GitHub **public**, dán link vào sheet | Tạo ngày đầu tiên (A1), cả 2 người là collaborator | Mở link ở cửa sổ ẩn danh xem được |
| Đầy đủ tính năng | 40 task phủ hết đề bài (bảng đối chiếu ở mục 5) | Demo trên mô hình + test tích hợp |
| Unit test, **coverage > 90%** | Mỗi commit `feat` kèm test; đo bằng `gcc --coverage` + `gcovr`; CI báo fail nếu < 90% | `make coverage`, badge trên README |
| Test **khả năng chịu tải** | `tools/loadtest.c`: N client đồng thời, đo thông lượng, độ trễ p50/p95/p99, tỉ lệ lỗi | `make loadtest`, báo cáo `docs/loadtest.md` |
| Mỗi tuần ≥ 5 commit | Mỗi người 5 commit/tuần → 10 commit/tuần | `git log --since --until --oneline \| wc -l` |
| Hai commit cách nhau ≥ 30 phút | Lịch commit theo khung giờ (mục 4) + git hook chặn commit quá sớm | `tools/check_commits.sh` chạy trong CI |
| Mỗi thành viên đều có commit | A làm `device/`, B làm `app/` + phần điều hòa trên mô hình | `git shortlog -sne` |

### Lưu ý để commit được tính đúng
- Mỗi người đặt `git config user.email` **trùng email tài khoản GitHub**.
- Merge PR bằng **Rebase and merge** hoặc **Create a merge commit**, **không dùng Squash** (Squash làm mất số commit và mốc thời gian).
- Không dùng `--amend` hay `rebase` để sửa giờ commit.
- Khoảng cách 30 phút tính trên **toàn repo**, nên hai người phải chia khung giờ commit.
- Code viết trên Raspberry Pi cũng phải commit bằng tài khoản của người viết (đặt `user.email` trên Pi theo từng người, hoặc mỗi người tự `git push` từ laptop của mình).

### Chiến lược đạt coverage > 90% trong C
1. **Tách logic khỏi I/O:** tính công suất, điện năng, phân tích lệnh, auth, đổi tốc độ → % PWM là hàm thuần → test trực tiếp.
2. **Hàm xử lý lệnh nhận chuỗi, trả chuỗi:** `handle_command(ctx, "SET ...", out)` → test không cần mạng.
3. **Test phần mạng bằng `socketpair()` và `127.0.0.1`.**
4. **Menu nhận `FILE *in, FILE *out`** → test đưa vào kịch bản phím bằng `fmemopen()`.
5. **Lớp phần cứng tách riêng:** CI và test dùng `hw_console.c`. `hw_gpio.c` chỉ biên dịch trên Pi (`make HW=gpio`), viết thật mỏng (chỉ gọi thư viện), mọi tính toán nằm trong `hw_map.c` đã có test. Xin giảng viên cho loại `hw_gpio.c` và `main.c` khỏi phép đo; nếu không được thì vì chúng rất ngắn nên vẫn giữ được > 90%.
6. Đo coverage **từ tuần 1**.

## 0. Quyết định chung (thống nhất ngày đầu tiên)

| Hạng mục | Lựa chọn | Lý do |
|---|---|---|
| Ngôn ngữ | C (C11), `gcc`, `Makefile` | |
| Máy phát triển | Linux / **WSL Ubuntu** trên Windows | Socket POSIX + `pthread`, có `valgrind` |
| Bộ điều khiển mô hình | **Raspberry Pi** 3B+/4 (Raspberry Pi OS) | Chạy Linux nên cùng một code C chạy được cả trên laptop lẫn Pi; có sẵn Wi-Fi |
| Điều khiển chân | `libgpiod` (LED), PWM phần cứng qua `/sys/class/pwm` (quạt), I2C qua `/dev/i2c-1` (LCD) | Giao diện C chuẩn của Linux |
| Quét thiết bị | UDP broadcast, cổng `50000` | |
| Điều khiển | TCP, cổng `50001` | |
| Định dạng thông điệp | Giao thức văn bản, 1 dòng/thông điệp, phản hồi có mã số (`200`, `401`…) | Không cần thư viện JSON, debug bằng `nc` |
| Đa client | 1 luồng/client (`pthread`) + `pthread_mutex`; tuần 4 chuyển sang thread pool | |
| Lưu trữ | File văn bản | |
| Test | Chương trình test dùng `assert`, `make test` | |
| Git | `main` + `feature/<tên>`, PR do người kia review | |

**Cài đặt trên laptop (WSL):**
```bash
sudo apt install build-essential valgrind netcat-openbsd gdb gcovr
```

**Cài đặt trên Raspberry Pi:**
```bash
sudo apt install build-essential gdb git libgpiod-dev gpiod i2c-tools
# Bật I2C và PWM: thêm 2 dòng vào /boot/firmware/config.txt rồi khởi động lại
#   dtparam=i2c_arm=on
#   dtoverlay=pwm,pin=18,func=2
```

**Phân vai:**
- **Người A – Mô phỏng thiết bị (server):** `device_sim` trên Raspberry Pi; điều khiển đèn LED và quạt mini.
- **Người B – Ứng dụng người dùng (client):** `smarthome_app` trên laptop; tuần 4 làm thêm phần điều hòa trên mô hình (LED RGB + LCD) để cả hai đều làm phần cứng.
- **Cả hai:** lắp nhà mô hình (ngoài giờ code, xem mục 1.1).
- `common/protocol.h` là **hợp đồng chung**: A viết, B review.

**Tài liệu học:** *Beej's Guide to Network Programming* (chương 5–7), `man 2 socket`, `man 7 udp`, `man 3 pthread_create`, tài liệu `libgpiod` (`gpioset`, `gpiod_line_request`), sơ đồ chân Raspberry Pi tại pinout.xyz.

## 1. Kiến trúc

```
smart-home/
├── .github/workflows/ci.yml # build + test + coverage ≥ 90% + kiểm tra giờ commit
├── Makefile                 # make [HW=gpio], test, coverage, loadtest, itest, clean
├── tools/
│   ├── loadtest.c           # công cụ đo tải
│   ├── check_commits.sh     # kiểm tra ≥5 commit/tuần, cách nhau ≥30 phút
│   └── pre-commit           # hook chặn commit nếu chưa đủ 30 phút
├── docs/
│   ├── protocol.md
│   ├── wiring.md            # sơ đồ nối dây + ảnh mô hình
│   └── loadtest.md
├── common/
│   ├── protocol.h/.c        # cổng, mã lỗi, send_all(), recv_line(), tách tham số
│   └── util.h/.c            # chuỗi ngẫu nhiên (/dev/urandom), thời gian
├── device/                  # NGƯỜI A (chạy trên Pi)
│   ├── device.h/.c          # struct Device, Light/Fan/AC, tính công suất
│   ├── energy.h/.c          # đồng hồ điện + giờ hoạt động theo ngày
│   ├── auth.h/.c            # mật khẩu, token <-> app_id
│   ├── scheduler.h/.c       # luồng hẹn giờ
│   ├── server.h/.c          # UDP discovery + TCP server
│   ├── hw.h                 # interface phần cứng
│   ├── hw_map.c             # hàm thuần: tốc độ → % PWM, chế độ → màu RGB (có test)
│   ├── hw_console.c         # bản console: in bảng trạng thái (mặc định, test/CI)
│   ├── hw_gpio.c            # bản mô hình: GPIO/PWM/I2C (make HW=gpio)
│   ├── lcd_i2c.c            # driver LCD 1602 qua PCF8574 (NGƯỜI B)
│   ├── devices.conf
│   └── main.c
├── app/                     # NGƯỜI B (chạy trên laptop)
│   ├── model.h/.c           # House, Room, DeviceRef (mảng động)
│   ├── storage.h/.c         # lưu/đọc file, app_id
│   ├── client.h/.c          # scan(), connect_device(), send_cmd()
│   ├── ui.h/.c              # menu dòng lệnh
│   └── main.c
└── tests/
```

### Luồng hoạt động

```
 Laptop                                     Raspberry Pi trong nhà mô hình
 smarthome_app                              device_sim
   │── UDP "SCAN <app_id>" (broadcast) ───▶ :50000
   │◀──── "DEVICE FAN-01 fan 50001 ..." ───│
   │── TCP "CONNECT FAN-01 <app_id> <mk>" ▶ :50001
   │◀──────────────────── "200 <token>" ───│
   │── "SET FAN-01 ... speed 2" ──────────▶ device_set() → tính điện → hw_fan()
   │◀──────────────────── "200 power=45" ──│                             │
                                                            PWM 66% → motor quay vừa
```

### Lớp phần cứng

Logic thiết bị **không gọi GPIO trực tiếp**, chỉ gọi `hw.h`. Lúc build mới chọn bản cài đặt.

```c
/* device/hw.h */
int  hw_init(const DeviceList *list);
void hw_light(const Device *d);   /* đọc d->on, d->gpio */
void hw_fan(const Device *d);     /* đọc d->on, d->fan_speed */
void hw_ac(const Device *d);      /* đọc d->on, d->ac_mode, d->ac_temp */
void hw_close(void);

/* device/hw_map.c – hàm thuần, test được */
int hw_fan_duty_percent(int on, int speed);         /* 0, 40, 70, 100 */
void hw_ac_color(int on, AcMode mode, int rgb[3]);  /* tắt=000, mát=001, nóng=100, ẩm=010 */
```

```bash
make            # bản console – laptop, WSL, CI
make HW=gpio    # bản mô hình – Raspberry Pi
```

Bản console in bảng trạng thái mỗi khi có thay đổi:
```
ID        LOAI   TRANG THAI            CONG SUAT
LIGHT-01  den    BAT                   40 W
FAN-01    quat   BAT  so 2             45 W
AC-01     dhoa   BAT  lam mat 24°C   1120 W
```

### File cấu hình thiết bị

```
# devices.conf: id  loại  tên  mật_khẩu  chân_GPIO (số BCM)
LIGHT-01  light  Den_phong_khach     1234  17
LIGHT-02  light  Den_phong_ngu       1234  27
LIGHT-03  light  Den_bep             1234  22
FAN-01    fan    Quat_phong_khach    1234  18
AC-01     ac     Dieu_hoa_phong_ngu  1234  5,6,13
```

### Gợi ý struct

```c
typedef enum { DEV_LIGHT, DEV_FAN, DEV_AC } DeviceType;
typedef enum { AC_COOL, AC_HEAT, AC_DRY } AcMode;

typedef struct {
    char        id[32];
    char        name[64];
    DeviceType  type;
    int         gpio[3];       /* chân nối trên mô hình */
    int         on;
    int         fan_speed;     /* 1..3 */
    AcMode      ac_mode;
    int         ac_temp;       /* 18..30 */
    int         power_w;       /* công suất giả định hiện tại */
    char        password[64];
    EnergyMeter meter;
    Timer       timer;
    pthread_mutex_t lock;
} Device;
```

## 1.1 Nhà mô hình

### Linh kiện

| Linh kiện | SL | Dùng cho |
|---|---|---|
| Raspberry Pi 3B+ hoặc 4 + thẻ nhớ 16 GB + nguồn 5V 3A | 1 | Bộ điều khiển |
| Breadboard + dây cắm đực-cái, đực-đực | 1 bộ | Nối mạch |
| LED trắng/vàng 5 mm | 3 | 3 đèn |
| Điện trở 220 Ω | 6 | Hạn dòng cho LED và LED RGB |
| Motor DC 5V mini + cánh quạt | 1 | Quạt |
| MOSFET logic-level IRLZ44N (hoặc transistor 2N2222 + điện trở 1 kΩ) | 1 | Đóng/cắt motor |
| Diode 1N4007 | 1 | Chống điện áp ngược của motor |
| LED RGB cathode chung | 1 | Chế độ điều hòa |
| LCD 1602 có module I2C PCF8574 | 1 | Hiện nhiệt độ điều hòa |
| Nguồn 5V riêng cho motor (sạc dự phòng/adapter) | 1 | Không lấy dòng motor từ Pi |
| Bìa cứng/foam, keo nến | | Vỏ nhà, chia phòng |

### Sơ đồ chân (số BCM)

| Thiết bị | Chân Pi | Nối tới |
|---|---|---|
| LIGHT-01 phòng khách | GPIO17 | 220 Ω → LED → GND |
| LIGHT-02 phòng ngủ | GPIO27 | 220 Ω → LED → GND |
| LIGHT-03 bếp | GPIO22 | 220 Ω → LED → GND |
| FAN-01 | GPIO18 (PWM0) | Cực G của MOSFET; motor nối giữa +5V ngoài và cực D; cực S → GND; diode song song motor |
| AC-01 đỏ / lục / lam | GPIO5 / GPIO6 / GPIO13 | Mỗi chân 220 Ω → chân màu của LED RGB; chân chung → GND |
| LCD I2C | GPIO2 (SDA), GPIO3 (SCL), 5V, GND | Module PCF8574 |

```
Quạt:   +5V ngoài ──┬── motor ──┬── D (MOSFET)
                    └─◄ diode ──┘   S ── GND chung (nối với GND Pi)
        GPIO18 ── 220Ω ── G
```

### An toàn
- Chân GPIO chỉ chịu **3.3V, tối đa ~16 mA**. **Không** cắm motor trực tiếp vào GPIO hay chân 5V của Pi.
- Nguồn motor và Pi phải **chung GND**.
- Module I2C của LCD thường kéo SDA/SCL lên 5V: dùng module chuyển mức 3.3V↔5V, hoặc cấp nguồn LCD 3.3V (chữ có thể mờ hơn).
- Thử từng thiết bị bằng lệnh trước khi chạy code: `gpioset gpiochip0 17=1` (bật LED), `i2cdetect -y 1` (thấy địa chỉ LCD, thường là `0x27`).

### Tiến độ lắp mô hình (không tính vào commit)

| Tuần | Việc |
|---|---|
| 1 | Mua linh kiện, cài Raspberry Pi OS, bật SSH + Wi-Fi, thử `gpioset` với 1 LED |
| 2 | Làm vỏ nhà 3 phòng, gắn 3 LED đèn |
| 3 | Gắn quạt mini + MOSFET |
| 4 | Gắn LED RGB + LCD điều hòa, đi dây gọn, chụp ảnh cho `docs/wiring.md` |

## 2. Giao thức (chốt cuối tuần 1)

Trường cách nhau **1 dấu cách**, không chứa dấu cách. Dòng tối đa 512 byte.

**UDP – quét** (broadcast tới `255.255.255.255:50000`)
```
→ SCAN <app_id>
← DEVICE <device_id> <type> <tcp_port> <name>
```

**TCP – kết nối & điều khiển**
```
→ CONNECT <device_id> <app_id> <password>
← 200 <token>                  | 401 AUTH_FAILED

→ INFO <device_id> <app_id> <token>
← 200 type=ac on=1 mode=cool temp=24 power=1120

→ SET <device_id> <app_id> <token> <key> <value>
     power on|off · speed 1..3 · mode cool|heat|dry · temp 18..30
← 200 power=45                 | 422 INVALID_VALUE

→ STATS <device_id> <app_id> <token> DAY|MONTH|YEAR <YYYY-MM-DD|YYYY-MM|YYYY>
← 200 hours=3.50 kwh=0.140

→ PASSWD <device_id> <app_id> <token> <old> <new>    # đổi xong: huỷ mọi token
→ TIMER  <device_id> <app_id> <token> ON|OFF <seconds>
→ TIMER  <device_id> <app_id> <token> CANCEL
→ QUIT
```

| Mã | Ý nghĩa |
|---|---|
| 200 | Thành công |
| 400 | Sai cú pháp |
| 401 | Sai mật khẩu |
| 403 | Token sai / không khớp app_id |
| 404 | Không có device_id |
| 422 | Giá trị không hợp lệ |
| 429 | Bị khoá tạm vì sai mật khẩu nhiều lần |

**Bảng công suất giả định và cách thể hiện trên mô hình:**

| Thiết bị | Trạng thái | Công suất giả định | Trên mô hình |
|---|---|---|---|
| Đèn | tắt / bật | 0 / 40 W | LED tắt / sáng |
| Quạt | tắt / số 1 / 2 / 3 | 0 / 35 / 45 / 60 W | PWM 0 / 40 / 70 / 100% |
| Điều hòa | làm mát | 1000 W ở 26°C, +60 W mỗi độ thấp hơn | LED xanh lam + LCD `COOL 24C` |
| | làm nóng | 1200 W ở 22°C, +60 W mỗi độ cao hơn | LED đỏ + LCD `HEAT 28C` |
| | hút ẩm | 400 W | LED xanh lục + LCD `DRY` |
| | nhiệt độ hợp lệ | 18–30°C | |

**Tính điện mỗi khi có lệnh điều khiển:**
`energy_wh += power_w × (now − last_change) / 3600.0` → rồi mới đặt `power_w` mới → rồi gọi `hw_*()` để mô hình đổi theo.
Thời gian bật được tách theo ngày (bật 23:00–01:00 thì mỗi ngày tính 1 giờ).

---

## 3. Lịch 4 tuần

Mỗi task = 1 commit, mỗi commit `feat` có test đi kèm.

### Tuần 1 – Nền tảng: socket TCP, chuỗi, struct, Makefile, Git, lớp phần cứng

**Mục tiêu học:** TCP và UDP khác nhau thế nào; `socket → bind → listen → accept → recv/send`; vì sao `recv` có thể trả nửa dòng hoặc 2 dòng dính nhau; `malloc/realloc/free`; Makefile; ý tưởng tách interface (`hw.h`) để thay phần cứng bằng bản giả.
**Kết quả:** thiết bị chạy được ở bản console; app quản lý nhà/phòng; giao thức đã chốt; đã mua linh kiện.

| # | Người | Task / commit | Học | Xong khi |
|---|---|---|---|---|
| A1 | A | `chore: repo public, cấu trúc thư mục, Makefile (test/coverage/HW), CI GitHub Actions` | Makefile, `--coverage`, `gcovr`, GitHub Actions | Link repo đã dán vào sheet; CI xanh |
| A2 | A | `feat(common): send_all() + recv_line() có bộ đệm, tách tham số` | TCP là luồng byte | Test: 2 dòng dính nhau tách đúng; dòng bị cắt đôi ghép đúng |
| A3 | A | `feat(device): struct Device + device_compute_power()` | `enum`, `switch` | Đèn bật → 40 W; quạt số 3 → 60 W |
| A4 | A | `feat(device): device_set() kiểm tra hợp lệ + device_to_string()` | `strtol`, kiểm tra lỗi | `temp 31` → 422 |
| A5 | A | `feat(device): hw.h + hw_console.c (bảng trạng thái) + test bảng công suất` | Interface trong C, con trỏ hàm / chọn file lúc link | Mỗi lần `device_set()` bảng console in lại; test phủ cả bảng công suất |
| B1 | B | `feat(app): model House / Room / DeviceRef bằng mảng động` | `malloc`, `realloc` | Thêm 100 phòng không lỗi |
| B2 | B | `feat(app): lưu/đọc file + sinh app_id ngẫu nhiên 1 lần` | `fopen/fgets`, `/dev/urandom` | Chạy lại app thì app_id không đổi |
| B3 | B | `feat(app): menu thêm/sửa/xoá nhà, phòng (FILE *in/out)` | `fgets` thay `scanf` | CRUD xong, khởi động lại vẫn còn dữ liệu |
| B4 | B | `feat(app): client.c – kết nối TCP, gửi/nhận 1 dòng` | `getaddrinfo`, `connect`, `SO_RCVTIMEO` | Gửi/nhận đúng với `nc -l 50001` |
| B5 | B | `chore: check_commits.sh + pre-commit hook; test model, không rò bộ nhớ` | Bash, `git log --format=%ct`, `valgrind` | Hook chặn commit khi chưa đủ 30 phút; coverage ≥ 90% |

### Tuần 2 – Quét & xác thực: UDP broadcast, pthread, token, GPIO đầu tiên

**Mục tiêu học:** `SO_BROADCAST`, `sendto/recvfrom`, `SO_REUSEADDR`; `pthread_create/detach`; sinh token; **GPIO trên Pi bằng `libgpiod`**.
**Kết quả:** app trên laptop quét thấy Raspberry Pi, kết nối bằng mật khẩu, đổi mật khẩu; **3 LED trên mô hình bật/tắt được từ app** (qua lệnh tạm hoặc `nc`).

| # | Người | Task / commit | Học | Xong khi |
|---|---|---|---|---|
| A6 | A | `feat(device): luồng UDP trả DEVICE khi nhận SCAN` | UDP server | Laptop quét thấy đủ thiết bị trên Pi |
| A7 | A | `feat(device): TCP server, mỗi client 1 luồng, phân lệnh` | `pthread` | 2 client `nc` cùng lúc không chặn nhau |
| A8 | A | `feat(device): auth – kiểm tra mật khẩu, cấp token gắn app_id` | So sánh chuỗi, bảng token | Sai mật khẩu → 401; token sai → 403 |
| A9 | A | `feat(device): PASSWD, huỷ token cũ, ghi lại devices.conf` | Ghi file tạm rồi `rename()` | Đổi xong token cũ bị từ chối; khởi động lại vẫn dùng mật khẩu mới |
| A10 | A | `feat(hw): main.c đọc devices.conf (có cột GPIO) + hw_gpio.c bật/tắt LED đèn` | `libgpiod`, `getopt` | `./device_sim` (bản `HW=gpio`) trên Pi; gửi `SET LIGHT-01 ... power on` → LED sáng |
| B6 | B | `feat(app): scan() – broadcast, gom phản hồi trong 2 giây` | `select()` có timeout | Danh sách `(id, type, ip, port)` không trùng |
| B7 | B | `feat(app): menu quét → chọn thiết bị → thêm vào phòng` | | Lưu kèm IP/cổng |
| B8 | B | `feat(app): connect_device() gửi password + app_id, lưu token` | | Token dùng lại cho lệnh sau |
| B9 | B | `feat(app): menu đổi mật khẩu thiết bị` | `termios` ẩn mật khẩu (tuỳ chọn) | Phải kết nối lại bằng mật khẩu mới |
| B10 | B | `fix(app): xử lý timeout, mất kết nối, SIGPIPE, sai mật khẩu` | `signal(SIGPIPE, SIG_IGN)` | Rút điện Pi giữa chừng, app báo lỗi chứ không crash |

### Tuần 3 – Điều khiển, tính điện năng, quạt PWM

**Mục tiêu học:** race condition và `pthread_mutex`; `struct tm`, `mktime` để tách theo ngày; **PWM** (duty cycle quyết định tốc độ motor); MOSFET làm công tắc.
**Kết quả:** điều khiển đủ 3 loại thiết bị từ menu; quạt mô hình quay đúng 3 tốc độ; xem được giờ hoạt động và kWh ước tính.

| # | Người | Task / commit | Học | Xong khi |
|---|---|---|---|---|
| A11 | A | `feat(device): xử lý SET đủ các key + test server thật qua 127.0.0.1` | Khởi động server trong luồng test | Lệnh đúng trả 200 + công suất mới; lệnh sai 400/422 |
| A12 | A | `feat(device): energy_meter_update() cộng dồn Wh, test bằng đồng hồ giả lập` | Truyền `now` vào hàm | Bật 40 W trong 30 phút → 20 Wh |
| A13 | A | `feat(device): giờ hoạt động theo ngày/tháng/năm, tách qua nửa đêm` | `struct tm`, `mktime` | Bật 23:00–01:00 → mỗi ngày 1 giờ |
| A14 | A | `feat(device): INFO + STATS, mutex bảo vệ từng thiết bị` | Race condition | 2 client gửi `SET` liên tục, số liệu không lệch |
| A15 | A | `feat(hw): quạt mini 3 tốc độ – PWM phần cứng + hw_map.c có test` | `/sys/class/pwm`, duty cycle | Quạt mô hình quay chậm/vừa/nhanh theo `speed 1/2/3`; `hw_fan_duty_percent` có test |
| B11 | B | `feat(app): send_set / get_info / get_stats, đọc mã phản hồi` | Phân tích `key=value` | Mỗi hàm trả mã lỗi rõ ràng |
| B12 | B | `feat(app): menu điều khiển theo loại thiết bị` | | Quạt chỉ hiện tốc độ; điều hòa hiện mode + 18–30°C |
| B13 | B | `feat(app): xem chi tiết thiết bị (trạng thái, công suất)` | `printf` căn cột | Hiển thị gọn |
| B14 | B | `feat(app): thống kê ngày/tháng/năm + ước tính tiền điện` | Giá điện bậc thang (tuỳ chọn) | In giờ, kWh, tiền |
| B15 | B | `test: script tích hợp – chạy sim (console), quét, kết nối, điều khiển, thống kê` | Bash + `nc` | `make itest` báo PASS, chạy được trong CI |

### Tuần 4 – Hẹn giờ, điều hòa trên mô hình, đo tải, hoàn thiện

**Mục tiêu học:** `pthread_cond_timedwait`; `sigaction` để lưu dữ liệu và tắt GPIO an toàn khi thoát; **I2C** (`ioctl(I2C_SLAVE)`); đo tải (thông lượng, phân vị, `ulimit -n`); thread pool.
**Kết quả:** demo đầy đủ trên nhà mô hình; coverage > 90%; báo cáo đo tải; báo cáo + slide.

| # | Người | Task / commit | Học | Xong khi |
|---|---|---|---|---|
| A16 | A | `feat(device): scheduler – TIMER ON/OFF/CANCEL` | `pthread_cond_timedwait` | Hẹn tắt 10 giây → LED/quạt trên mô hình tự tắt, điện tính đúng tới lúc tắt |
| A17 | A | `feat(device): SIGINT lưu dữ liệu + tắt hết GPIO, token hết hạn, khoá sau 5 lần sai` | `sigaction` | Ctrl+C: mọi LED/motor tắt, chạy lại không mất số liệu; sai 5 lần → 429 |
| A18 | A | `test(load): tools/loadtest.c – N client đồng thời CONNECT + SET` | `clock_gettime`, phân vị | `./loadtest -c 1000 -n 100` in lệnh/giây, p50/p95/p99, % lỗi |
| A19 | A | `perf(device): thread pool + giới hạn kết nối theo kết quả đo tải` | Hàng đợi + `pthread_cond` | Lỗi ≈ 0% ở 1000 client; bảng so sánh trước/sau |
| A20 | A | `docs: báo cáo phần thiết bị, giao thức, wiring.md, kết quả đo tải` | | Có sơ đồ tuần tự, sơ đồ nối dây, ảnh mô hình |
| B16 | B | `feat(app): màn hình tổng quan (màu ANSI) + menu hẹn giờ` | Mã ANSI | Thấy trạng thái mọi thiết bị; đặt/huỷ hẹn giờ được |
| B17 | B | `test(app): test menu bằng kịch bản nhập qua fmemopen()` | `fmemopen` | Coverage `app/ui.c` ≥ 90% |
| B18 | B | `feat(hw): điều hòa trên mô hình – LED RGB theo chế độ + lcd_i2c.c hiện nhiệt độ` | I2C, PCF8574, `ioctl` | Đổi mode từ app → LED đổi màu; đổi nhiệt độ → LCD hiện `COOL 24C`; `hw_ac_color` có test |
| B19 | B | `test: bù coverage tổng > 90%, test scan() qua loopback, valgrind sạch` | `gcovr --html` | CI xanh với ngưỡng 90% |
| B20 | B | `docs: báo cáo phần ứng dụng, hướng dẫn chạy, badge coverage, slide, video demo` | | Người ngoài làm theo README chạy được |

**Kịch bản đo tải** (chạy `device_sim` bản console trên laptop để đo phần mạng, và chạy 1 lần trên Pi để so sánh):

| Kịch bản | Tham số | Ghi lại |
|---|---|---|
| Nhiều client | 10 → 100 → 500 → 1000 client × 100 lệnh `SET` | lệnh/giây, p50/p95/p99, % lỗi |
| Bão xác thực | 1000 `CONNECT` sai mật khẩu | Không sập, khoá đúng sau 5 lần sai |
| Kết nối lâu | 200 client giữ 10 phút | RAM, số luồng, không rò bộ nhớ |
| Quét dồn dập | 100 `SCAN`/giây | Tỉ lệ có phản hồi |
| Laptop vs Pi | Kịch bản 1 với 100 client | So sánh hiệu năng 2 máy |

---

## 4. Nhịp làm việc mỗi tuần

| Ngày | Việc |
|---|---|
| Thứ 2 | Họp 15 phút: chốt task tuần, đọc phần **Học** |
| Thứ 3–7 | 1 commit/ngày/người theo khung giờ bên dưới, mở PR, người kia review trong 24 giờ |
| Thứ 6 | Chạy thử chung trên nhà mô hình |
| Thứ 7 | **Đổi vai 30 phút:** A giải thích code của mình cho B và ngược lại |
| CN | Lắp mô hình / bù việc chậm; chạy `tools/check_commits.sh` |

**Khung giờ commit (cách nhau ≥ 30 phút trên toàn repo):**

| Ngày | Người A | Người B |
|---|---|---|
| Thứ 3 | 19:00 | 20:00 |
| Thứ 4 | 20:00 | 19:00 |
| Thứ 5 | 19:00 | 20:00 |
| Thứ 6 | 20:00 | 19:00 |
| Thứ 7 | 19:00 | 20:00 |

Lệch giờ không sao, miễn là hai commit cách nhau ≥ 30 phút. Hook `tools/pre-commit` chặn commit nếu commit mới nhất trên `origin/main` chưa đủ 30 phút, nên phải `git fetch` trước khi commit.

**Quy ước commit:** `feat:` / `fix:` / `test:` / `docs:` / `chore:` / `perf:` + phạm vi `(device)`, `(app)`, `(common)`, `(hw)`.

**Mốc kiểm tra:**
- Hết tuần 1: thiết bị (bản console) và model chạy độc lập; giao thức đã chốt; đã có linh kiện.
- Hết tuần 2: laptop quét + kết nối Pi; 3 LED đèn bật/tắt được.
- Hết tuần 3: điều khiển + thống kê qua menu; quạt mô hình chạy 3 tốc độ.
- Hết tuần 4: điều hòa trên mô hình, hẹn giờ, coverage > 90%, báo cáo đo tải, báo cáo cuối.

**Mẹo debug:**
- Thử server bằng tay: `nc <ip_pi> 50001` rồi gõ `CONNECT FAN-01 test 1234`.
- Thử quét: `echo "SCAN test" | nc -u -b -w1 255.255.255.255 50000`.
- Thử phần cứng trước khi chạy code: `gpioset gpiochip0 17=1`, `i2cdetect -y 1`.
- Trên Pi 5 tên chip GPIO khác (`gpiochip4`); xem bằng `gpiodetect`.
- Laptop và Pi phải cùng mạng Wi-Fi. Wi-Fi trường học thường chặn broadcast giữa các máy, nên khi demo hãy phát hotspot từ điện thoại.
- WSL2 dùng mạng NAT nên broadcast không ra LAN thật. Muốn chạy app trên WSL để điều khiển Pi, bật `networkingMode=mirrored` trong `.wslconfig`, hoặc chạy app trên Linux thật.
- Lỗi `Address already in use`: đặt `SO_REUSEADDR` trước `bind`.
- Đo tải 1000 client: `ulimit -n 4096` trước.
- Biên dịch với `-Wall -Wextra -g`; crash thì `gdb ./device_sim` rồi `bt`.

## 5. Đối chiếu yêu cầu đề bài → task

| Yêu cầu trong đề | Task |
|---|---|
| Mô phỏng đèn, quạt nhiều tốc độ, điều hòa (mát / nóng / hút ẩm, 18–30°C) | A3, A4, A11 · trên mô hình: A10, A15, B18 |
| Hẹn giờ bật/tắt | A16, B16 |
| Mỗi thiết bị có ID duy nhất | A3, A10 |
| Công suất theo mức điều chỉnh | A3, A5 |
| Lắng nghe thông điệp quét trên một cổng | A6 |
| Kiểm tra mật khẩu, gửi thông tin thiết bị, token ngẫu nhiên gắn ID ứng dụng | A8 |
| Mỗi lệnh điều khiển: tính điện năng rồi đặt công suất mới | A12, A14 |
| App có ID ngẫu nhiên | B2 |
| Quản lý nhà, phòng, thiết bị trong phòng | B1, B3, B7 |
| Quét, hiển thị loại + ID, thêm vào phòng | B6, B7 |
| Kết nối: gửi mật khẩu + ID ứng dụng | B8 |
| Đổi mật khẩu thiết bị | A9, B9 |
| Điều khiển thiết bị | B11, B12 |
| Xem trạng thái, giờ hoạt động theo ngày/tháng/năm, điện năng ước tính | A13, B13, B14 |
