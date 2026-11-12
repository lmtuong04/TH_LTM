# Smart Home – nhà mô hình điều khiển qua LAN bằng C

![CI](https://github.com/<owner>/<repo>/actions/workflows/ci.yml/badge.svg)

- `device_sim`: chạy trên Raspberry Pi trong nhà mô hình, điều khiển LED đèn, quạt mini (PWM), điều hòa (LED RGB + LCD).
- `smarthome_app`: chạy trên laptop, quét thiết bị qua UDP broadcast (cổng 50000) và điều khiển qua TCP (cổng 50001).

Kế hoạch chi tiết: [PLAN_2.md](PLAN_2.md).

## Thành viên

| Người | Phần việc |
|---|---|
| A – _tên_ | `device/`, `common/` (server, chạy trên Pi) |
| B – _tên_ | `app/` (client) + điều hòa trên mô hình |

## Build & test (Linux / WSL Ubuntu)

```bash
sudo apt install build-essential valgrind netcat-openbsd gdb gcovr

make              # bản console -> build/device_sim, build/smarthome_app
make test         # unit test
make coverage     # coverage, fail nếu < 90% (báo cáo: build/cov/coverage.html)
make HW=gpio      # bản mô hình, chỉ build trên Raspberry Pi (cần libgpiod-dev)
```

## Cấu trúc

```
common/   giao thức, tiện ích dùng chung
device/   mô phỏng thiết bị + lớp phần cứng (hw_console.c / hw_gpio.c)
app/      ứng dụng người dùng (model, lưu trữ, client, menu)
tests/    unit test (make test)
tools/    đo tải, kiểm tra giờ commit
docs/     giao thức, sơ đồ nối dây, kết quả đo tải
```

## Quy ước Git

- Nhánh `feature/<tên>`, mở PR, người kia review; merge bằng **Rebase and merge** hoặc **merge commit** (không Squash).
- Commit: `feat|fix|test|docs|chore|perf(<device|app|common|hw>): ...`
- Hai commit trên toàn repo cách nhau ≥ 30 phút; `git config user.email` trùng email GitHub.
