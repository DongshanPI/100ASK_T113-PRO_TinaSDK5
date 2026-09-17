# 100ASK 240x360 E-Ink system dashboard
SDK_BASE    ?= $(abspath $(CURDIR)/../..)
TOOLCHAIN   ?= $(SDK_BASE)/out/t113/evb1_auto_nand/buildroot/buildroot/host/opt/ext-toolchain/bin/arm-linux-gnueabi-
CC           = $(TOOLCHAIN)gcc
LVGL_BASE    = $(SDK_BASE)/platform/thirdparty/gui/lvgl-8
LVGL_DIR     = $(LVGL_BASE)/lvgl

override CFLAGS  += -O2 -Wall -Wextra -Werror=implicit-function-declaration -DLV_CONF_INCLUDE_SIMPLE
override CFLAGS  += -I. -I$(LVGL_DIR) -I$(LVGL_DIR)/.. -iquote $(SDK_BASE)/kernel/linux-5.4/include/uapi/linux
override LDFLAGS += -lm -lpthread
STATIC ?= 1
ifeq ($(STATIC),1)
override LDFLAGS += -static
endif

APP_SRCS = main.c epd_disp.c input.c reader.c settings.c system_stats.c tick.c ui.c wifi_scan.c font_cjk_16.c
LVGL_SRCS  = $(wildcard $(LVGL_DIR)/src/core/*.c)
LVGL_SRCS += $(wildcard $(LVGL_DIR)/src/draw/*.c)
LVGL_SRCS += $(wildcard $(LVGL_DIR)/src/draw/sw/*.c)
LVGL_SRCS += $(wildcard $(LVGL_DIR)/src/font/*.c)
LVGL_SRCS += $(wildcard $(LVGL_DIR)/src/hal/*.c)
LVGL_SRCS += $(wildcard $(LVGL_DIR)/src/misc/*.c)
LVGL_SRCS += $(wildcard $(LVGL_DIR)/src/widgets/*.c)
LVGL_SRCS += $(LVGL_DIR)/src/extra/lv_extra.c

APP_OBJS  = $(patsubst %.c,build/app/%.o,$(APP_SRCS))
LVGL_OBJS = $(patsubst $(SDK_BASE)/%.c,build/sdk/%.o,$(LVGL_SRCS))
OBJS      = $(APP_OBJS) $(LVGL_OBJS)
TARGET    = eink-dashboard

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@
	@size $@
	@file $@

build/app/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/sdk/%.o: $(SDK_BASE)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build $(TARGET)
