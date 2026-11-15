# Smart Home – nhà mô hình điều khiển qua LAN
#
#   make              bản console (laptop, WSL, CI)
#   make HW=gpio      bản mô hình (Raspberry Pi, cần libgpiod)
#   make test         chạy unit test
#   make coverage     đo coverage (gcovr), fail nếu < 90%
#   make loadtest     build công cụ đo tải (tools/loadtest.c)
#   make itest        chạy test tích hợp (tests/itest.sh)
#   make clean

CC       ?= gcc
CFLAGS   ?= -std=c11 -Wall -Wextra -g -O0
CFLAGS   += -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -Icommon -Idevice -Iapp -MMD -MP
CFLAGS   += $(EXTRA_CFLAGS)
LDFLAGS  += $(EXTRA_CFLAGS)
LDLIBS   += -pthread

HW       ?= console
BUILD    ?= build
OBJ      := $(BUILD)/obj
COV_MIN  ?= 90

# ---- nguồn ---------------------------------------------------------------
COMMON_SRC := $(wildcard common/*.c)
HW_ALL     := device/hw_console.c device/hw_gpio.c device/lcd_i2c.c
DEVICE_LIB := $(filter-out device/main.c $(HW_ALL),$(wildcard device/*.c))
APP_LIB    := $(filter-out app/main.c,$(wildcard app/*.c))
TEST_SRC   := $(wildcard tests/test_*.c)

ifeq ($(HW),gpio)
  HW_SRC := $(wildcard device/hw_gpio.c device/lcd_i2c.c)
  LDLIBS += -lgpiod
else
  HW_SRC := $(wildcard device/hw_console.c)
endif
# Test luôn dùng bản console, không đụng tới GPIO
TEST_HW_SRC := $(wildcard device/hw_console.c)

obj = $(patsubst %.c,$(OBJ)/%.o,$(1))

DEVICE_BIN := $(BUILD)/device_sim
APP_BIN    := $(BUILD)/smarthome_app
TEST_BINS  := $(patsubst tests/%.c,$(BUILD)/tests/%,$(TEST_SRC))
TEST_DEPS  := $(call obj,$(COMMON_SRC) $(DEVICE_LIB) $(TEST_HW_SRC) $(APP_LIB))

.PHONY: all test coverage memcheck loadtest itest clean
.SECONDARY:

all: $(DEVICE_BIN) $(APP_BIN)

$(DEVICE_BIN): $(call obj,device/main.c $(COMMON_SRC) $(DEVICE_LIB) $(HW_SRC))
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(APP_BIN): $(call obj,app/main.c $(COMMON_SRC) $(APP_LIB))
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD)/tests/%: $(OBJ)/tests/%.o $(TEST_DEPS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(OBJ)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

test: $(TEST_BINS)
	@set -e; for t in $(TEST_BINS); do echo "== $$t"; ./$$t; done
	@echo "Tat ca test PASS"

memcheck: test
	@set -e; for t in $(TEST_BINS); do echo "== valgrind $$t"; \
	    valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1 ./$$t; \
	done

coverage:
	rm -rf $(BUILD)/cov
	$(MAKE) --no-print-directory BUILD=$(BUILD)/cov EXTRA_CFLAGS=--coverage test
	gcovr -r . --object-directory $(BUILD)/cov \
	    --exclude 'tests/.*' --exclude 'tools/.*' \
	    --exclude 'device/main\.c' --exclude 'app/main\.c' \
	    --exclude 'device/hw_gpio\.c' --exclude 'device/lcd_i2c\.c' \
	    --print-summary --fail-under-line $(COV_MIN) \
	    --html-details $(BUILD)/cov/coverage.html

loadtest:
	@if [ -f tools/loadtest.c ]; then \
	    $(MAKE) --no-print-directory $(BUILD)/loadtest; \
	else echo "Chua co tools/loadtest.c (task A18)"; fi

$(BUILD)/loadtest: $(call obj,tools/loadtest.c $(COMMON_SRC))
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

itest: all
	@if [ -f tests/itest.sh ]; then bash tests/itest.sh; \
	else echo "Chua co tests/itest.sh (task B15)"; fi

clean:
	rm -rf $(BUILD)

-include $(shell find $(OBJ) -name '*.d' 2>/dev/null)
