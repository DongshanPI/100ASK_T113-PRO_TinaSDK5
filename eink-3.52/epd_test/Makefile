SDK_BASE  ?= $(abspath $(CURDIR)/../..)
CC         = $(SDK_BASE)/out/t113/evb1_auto_nand/buildroot/buildroot/host/opt/ext-toolchain/bin/arm-linux-gnueabi-gcc
CFLAGS    += -O2 -Wall -Wextra -iquote $(SDK_BASE)/kernel/linux-5.4/include/uapi/linux
LDFLAGS   += -static

all: epd-test

epd-test: epd_test.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

clean:
	rm -f epd-test
