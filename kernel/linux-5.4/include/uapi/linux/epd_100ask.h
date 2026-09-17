/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _UAPI_LINUX_EPD_100ASK_H
#define _UAPI_LINUX_EPD_100ASK_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define EPD_100ASK_WIDTH       240U
#define EPD_100ASK_HEIGHT      360U
#define EPD_100ASK_FRAME_SIZE  (EPD_100ASK_WIDTH * EPD_100ASK_HEIGHT / 8U)

enum epd_100ask_refresh_mode {
	EPD_100ASK_REFRESH_GC = 0,
	EPD_100ASK_REFRESH_DU = 1,
	EPD_100ASK_REFRESH_5S = 2,
};

#define EPD_100ASK_CAP_PARTIAL  (1U << 0)

struct epd_100ask_info {
	__u16 width;
	__u16 height;
	__u32 frame_size;
	__u32 capabilities;
	__u32 full_refreshes;
	__u32 partial_refreshes;
	__u32 skipped_refreshes;
	__s32 last_error;
	__u8 refresh_mode;
	__u8 reserved[3];
};

#define EPD_IOC_MAGIC       'E'
#define EPD_IOC_CLEAR_WHITE _IO(EPD_IOC_MAGIC, 0x01)
#define EPD_IOC_CLEAR_BLACK _IO(EPD_IOC_MAGIC, 0x02)
#define EPD_IOC_SET_LUT_GC  _IO(EPD_IOC_MAGIC, 0x03)
#define EPD_IOC_SET_LUT_DU  _IO(EPD_IOC_MAGIC, 0x04)
#define EPD_IOC_SET_LUT_5S  _IO(EPD_IOC_MAGIC, 0x05)
#define EPD_IOC_GET_INFO     _IOR(EPD_IOC_MAGIC, 0x06, struct epd_100ask_info)

#endif /* _UAPI_LINUX_EPD_100ASK_H */
