#include "epd_disp.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int epd_fd = -1;
static lv_color_t *draw_buf;
static uint8_t *mono_buf;
static lv_disp_draw_buf_t disp_buf;
static lv_disp_drv_t disp_drv;
static int last_error;
static unsigned int flush_count;

static void rgb565_to_1bpp(const lv_color_t *src, uint8_t *dst)
{
	unsigned int x, y;

	memset(dst, 0, EPD_100ASK_FRAME_SIZE);
	for (y = 0; y < EPD_HEIGHT; y++) {
		for (x = 0; x < EPD_WIDTH; x++) {
			uint16_t c = lv_color_to16(src[y * EPD_WIDTH + x]);
			unsigned int r = ((c >> 11) & 0x1f) * 255 / 31;
			unsigned int g = ((c >> 5) & 0x3f) * 255 / 63;
			unsigned int b = (c & 0x1f) * 255 / 31;
			unsigned int lum = (r * 77 + g * 150 + b * 29) >> 8;

			if (lum >= 128)
				mono_buf[(y * EPD_WIDTH + x) >> 3] |= 0x80 >> (x & 7);
		}
	}
}

static void epd_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area,
			 lv_color_t *color_p)
{
	ssize_t written;

	(void)area;
	(void)color_p;
	rgb565_to_1bpp(draw_buf, mono_buf);
	written = write(epd_fd, mono_buf, EPD_100ASK_FRAME_SIZE);
	if (written != (ssize_t)EPD_100ASK_FRAME_SIZE) {
		last_error = written < 0 ? errno : EIO;
		fprintf(stderr, "EPD write failed: %s\n", strerror(last_error));
	} else {
		last_error = 0;
	}
	flush_count++;
	lv_disp_flush_ready(drv);
}

lv_disp_t *epd_disp_init(void)
{
	epd_fd = open("/dev/epd_100ask", O_WRONLY | O_CLOEXEC);
	if (epd_fd < 0) {
		perror("open /dev/epd_100ask");
		return NULL;
	}

	draw_buf = calloc(EPD_WIDTH * EPD_HEIGHT, sizeof(*draw_buf));
	mono_buf = malloc(EPD_100ASK_FRAME_SIZE);
	if (!draw_buf || !mono_buf) {
		fprintf(stderr, "EPD framebuffer allocation failed\n");
		epd_disp_close();
		return NULL;
	}
	memset(draw_buf, 0xff, EPD_WIDTH * EPD_HEIGHT * sizeof(*draw_buf));
	memset(mono_buf, 0xff, EPD_100ASK_FRAME_SIZE);

	lv_disp_draw_buf_init(&disp_buf, draw_buf, NULL, EPD_WIDTH * EPD_HEIGHT);
	lv_disp_drv_init(&disp_drv);
	disp_drv.hor_res = EPD_WIDTH;
	disp_drv.ver_res = EPD_HEIGHT;
	disp_drv.flush_cb = epd_flush_cb;
	disp_drv.draw_buf = &disp_buf;
	disp_drv.full_refresh = 1;
	disp_drv.antialiasing = 0;
	return lv_disp_drv_register(&disp_drv);
}

void epd_disp_set_mode(enum epd_100ask_refresh_mode mode)
{
	unsigned long cmd = EPD_IOC_SET_LUT_GC;

	if (mode == EPD_100ASK_REFRESH_DU)
		cmd = EPD_IOC_SET_LUT_DU;
	else if (mode == EPD_100ASK_REFRESH_5S)
		cmd = EPD_IOC_SET_LUT_5S;
	if (ioctl(epd_fd, cmd) < 0) {
		last_error = errno;
		perror("EPD set mode");
	}
}

int epd_disp_render_now(void)
{
	uint32_t start = lv_tick_get();
	unsigned int initial_flush_count = flush_count;

	lv_obj_invalidate(lv_scr_act());
	while (lv_tick_elaps(start) < 250) {
		lv_timer_handler();
		if (flush_count != initial_flush_count || last_error)
			break;
		usleep(5000);
	}
	return last_error ? -last_error : 0;
}

int epd_disp_get_info(struct epd_100ask_info *info)
{
	if (ioctl(epd_fd, EPD_IOC_GET_INFO, info) < 0)
		return -errno;
	return 0;
}

int epd_disp_last_error(void)
{
	return last_error;
}

void epd_disp_close(void)
{
	if (epd_fd >= 0)
		close(epd_fd);
	epd_fd = -1;
	free(draw_buf);
	free(mono_buf);
	draw_buf = NULL;
	mono_buf = NULL;
}
