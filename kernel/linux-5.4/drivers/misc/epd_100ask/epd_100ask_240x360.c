// SPDX-License-Identifier: GPL-2.0-only
/*
 * 100ASK 240x360 SPI EPD Driver for Linux 5.4
 * Compatible: "100ask,epd-240x360"  /dev/epd_100ask
 *
 * LUT tables and init sequence from ESP32 verified reference driver.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/spi/spi.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/mutex.h>
#include <linux/epd_100ask.h>

#define EPD_WIDTH        EPD_100ASK_WIDTH
#define EPD_HEIGHT       EPD_100ASK_HEIGHT
#define EPD_BUF_SIZE     EPD_100ASK_FRAME_SIZE

#define LUT_GC  0
#define LUT_DU  1
#define LUT_5S  2

/* ---- LUT tables from ESP32 verified reference ---- */
static const u8 lut_R20_GC[56] = {
	0x01,0x0f,0x0f,0x0f,0x01,0x01,0x01,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
static const u8 lut_R21_GC[42] = {
	0x01,0x4f,0x8f,0x0f,0x01,0x01,0x01,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
static const u8 lut_R22_GC[56] = {
	0x01,0x0f,0x8f,0x0f,0x01,0x01,0x01,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
static const u8 lut_R23_GC[42] = {
	0x01,0x4f,0x8f,0x4f,0x01,0x01,0x01,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
static const u8 lut_R24_GC[42] = {
	0x01,0x0f,0x8f,0x4f,0x01,0x01,0x01,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};

static const u8 lut_R20_DU[56] = {
	0x01,0x0f,0x01,0x00,0x00,0x01,0x01,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
static const u8 lut_R21_DU[42] = {
	0x01,0x0f,0x01,0x00,0x00,0x01,0x01,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
static const u8 lut_R22_DU[56] = {
	0x01,0x8f,0x01,0x00,0x00,0x01,0x01,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
static const u8 lut_R23_DU[42] = {
	0x01,0x4f,0x01,0x00,0x00,0x01,0x01,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
static const u8 lut_R24_DU[42] = {
	0x01,0x0f,0x01,0x00,0x00,0x01,0x01,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
static const u8 lut_vcom[42] = {
	0x01,0x19,0x19,0x19,0x19,0x01,0x01,
	0x01,0x19,0x19,0x19,0x01,0x01,0x01,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
static const u8 lut_ww[42] = {
	0x01,0x59,0x99,0x59,0x99,0x01,0x01,
	0x01,0x59,0x99,0x19,0x01,0x01,0x01,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
static const u8 lut_bw[42] = {
	0x01,0x59,0x99,0x59,0x99,0x01,0x01,
	0x01,0x59,0x99,0x19,0x01,0x01,0x01,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
static const u8 lut_wb[42] = {
	0x01,0x19,0x99,0x59,0x99,0x01,0x01,
	0x01,0x59,0x99,0x59,0x01,0x01,0x01,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
static const u8 lut_bb[42] = {
	0x01,0x19,0x99,0x59,0x99,0x01,0x01,
	0x01,0x59,0x99,0x59,0x01,0x01,0x01,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};

struct epd_100ask {
	struct spi_device *spi;
	struct miscdevice miscdev;
	struct gpio_desc *dc_gpio;
	struct gpio_desc *busy_gpio;
	struct gpio_desc *reset_gpio;
	u8 *fb;
	int lut_flag;
	int lut_swap;  /* toggles R22/R23 order per ESP32 logic */
	u32 full_refreshes;
	u32 partial_refreshes;
	u32 skipped_refreshes;
	int last_error;
	struct mutex lock;
};

static int epd_spi_write_cmd(struct epd_100ask *epd, u8 cmd)
{
	struct spi_transfer t = { .tx_buf = &cmd, .len = 1 };
	gpiod_set_value_cansleep(epd->dc_gpio, 0);
	return spi_sync_transfer(epd->spi, &t, 1);
}

static int epd_spi_write_data(struct epd_100ask *epd, const u8 *data, size_t len)
{
	struct spi_transfer t = { .tx_buf = data, .len = len };
	gpiod_set_value_cansleep(epd->dc_gpio, 1);
	return spi_sync_transfer(epd->spi, &t, 1);
}

static int epd_spi_write_byte(struct epd_100ask *epd, u8 data)
{
	return epd_spi_write_data(epd, &data, 1);
}

static int epd_wait_busy(struct epd_100ask *epd)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(30000);
	int val;

	do {
		val = gpiod_get_value_cansleep(epd->busy_gpio);
		if (val < 0)
			return val;
		if (val == 1)  /* BUSY=1 means idle */
			return 0;
		msleep(10);
	} while (time_before(jiffies, timeout));

	dev_err(&epd->spi->dev, "busy timeout\n");
	return -ETIMEDOUT;
}

static int epd_reset(struct epd_100ask *epd)
{
	if (epd->reset_gpio) {
		gpiod_set_value_cansleep(epd->reset_gpio, 1);
		msleep(20);
		gpiod_set_value_cansleep(epd->reset_gpio, 0);
		msleep(20);
		gpiod_set_value_cansleep(epd->reset_gpio, 1);
		msleep(20);
	} else {
		dev_info(&epd->spi->dev, "no reset-gpios, relying on power-on reset\n");
		msleep(50);
	}
	return 0;
}

/* LUT download - matches ESP32 reference driver sequence exactly */
static int epd_lut_GC(struct epd_100ask *epd)
{
	int ret;
	ret = epd_spi_write_cmd(epd, 0x20);
	if (ret) return ret;
	ret = epd_spi_write_data(epd, lut_R20_GC, sizeof(lut_R20_GC));
	if (ret) return ret;
	ret = epd_spi_write_cmd(epd, 0x21);
	if (ret) return ret;
	ret = epd_spi_write_data(epd, lut_R21_GC, sizeof(lut_R21_GC));
	if (ret) return ret;
	ret = epd_spi_write_cmd(epd, 0x24);
	if (ret) return ret;
	ret = epd_spi_write_data(epd, lut_R24_GC, sizeof(lut_R24_GC));
	if (ret) return ret;

	if (epd->lut_swap == 0) {
		ret = epd_spi_write_cmd(epd, 0x22);
		if (ret) return ret;
		ret = epd_spi_write_data(epd, lut_R22_GC, sizeof(lut_R22_GC));
		if (ret) return ret;
		ret = epd_spi_write_cmd(epd, 0x23);
		if (ret) return ret;
		ret = epd_spi_write_data(epd, lut_R23_GC, sizeof(lut_R23_GC));
		if (ret) return ret;
		epd->lut_swap = 1;
	} else {
		ret = epd_spi_write_cmd(epd, 0x22);
		if (ret) return ret;
		ret = epd_spi_write_data(epd, lut_R23_GC, sizeof(lut_R23_GC));
		if (ret) return ret;
		ret = epd_spi_write_cmd(epd, 0x23);
		if (ret) return ret;
		ret = epd_spi_write_data(epd, lut_R22_GC, sizeof(lut_R22_GC));
		if (ret) return ret;
		epd->lut_swap = 0;
	}
	return ret;
}

static int epd_lut_DU(struct epd_100ask *epd)
{
	int ret;
	ret = epd_spi_write_cmd(epd, 0x20);
	if (ret) return ret;
	ret = epd_spi_write_data(epd, lut_R20_DU, sizeof(lut_R20_DU));
	if (ret) return ret;
	ret = epd_spi_write_cmd(epd, 0x21);
	if (ret) return ret;
	ret = epd_spi_write_data(epd, lut_R21_DU, sizeof(lut_R21_DU));
	if (ret) return ret;
	ret = epd_spi_write_cmd(epd, 0x24);
	if (ret) return ret;
	ret = epd_spi_write_data(epd, lut_R24_DU, sizeof(lut_R24_DU));
	if (ret) return ret;

	if (epd->lut_swap == 0) {
		ret = epd_spi_write_cmd(epd, 0x22);
		if (ret) return ret;
		ret = epd_spi_write_data(epd, lut_R22_DU, sizeof(lut_R22_DU));
		if (ret) return ret;
		ret = epd_spi_write_cmd(epd, 0x23);
		if (ret) return ret;
		ret = epd_spi_write_data(epd, lut_R23_DU, sizeof(lut_R23_DU));
		if (ret) return ret;
		epd->lut_swap = 1;
	} else {
		ret = epd_spi_write_cmd(epd, 0x22);
		if (ret) return ret;
		ret = epd_spi_write_data(epd, lut_R23_DU, sizeof(lut_R23_DU));
		if (ret) return ret;
		ret = epd_spi_write_cmd(epd, 0x23);
		if (ret) return ret;
		ret = epd_spi_write_data(epd, lut_R22_DU, sizeof(lut_R22_DU));
		if (ret) return ret;
		epd->lut_swap = 0;
	}
	return ret;
}

static int epd_lut_5S(struct epd_100ask *epd)
{
	int ret;
	/* vcom */
	ret = epd_spi_write_cmd(epd, 0x20);
	if (ret) return ret;
	ret = epd_spi_write_data(epd, lut_vcom, sizeof(lut_vcom));
	if (ret) return ret;
	/* ww */
	ret = epd_spi_write_cmd(epd, 0x21);
	if (ret) return ret;
	ret = epd_spi_write_data(epd, lut_ww, sizeof(lut_ww));
	if (ret) return ret;
	/* bb */
	ret = epd_spi_write_cmd(epd, 0x24);
	if (ret) return ret;
	ret = epd_spi_write_data(epd, lut_bb, sizeof(lut_bb));
	if (ret) return ret;

	if (epd->lut_swap == 0) {
		ret = epd_spi_write_cmd(epd, 0x22);
		if (ret) return ret;
		ret = epd_spi_write_data(epd, lut_bw, sizeof(lut_bw));
		if (ret) return ret;
		ret = epd_spi_write_cmd(epd, 0x23);
		if (ret) return ret;
		ret = epd_spi_write_data(epd, lut_wb, sizeof(lut_wb));
		if (ret) return ret;
		epd->lut_swap = 1;
	} else {
		ret = epd_spi_write_cmd(epd, 0x22);
		if (ret) return ret;
		ret = epd_spi_write_data(epd, lut_wb, sizeof(lut_wb));
		if (ret) return ret;
		ret = epd_spi_write_cmd(epd, 0x23);
		if (ret) return ret;
		ret = epd_spi_write_data(epd, lut_bw, sizeof(lut_bw));
		if (ret) return ret;
		epd->lut_swap = 0;
	}
	return ret;
}

static int epd_load_lut(struct epd_100ask *epd)
{
	switch (epd->lut_flag) {
	case LUT_GC: return epd_lut_GC(epd);
	case LUT_DU: return epd_lut_DU(epd);
	case LUT_5S: return epd_lut_5S(epd);
	default:     return epd_lut_GC(epd);
	}
}

/* Init sequence matching ESP32 reference exactly */
static int epd_init(struct epd_100ask *epd)
{
	int ret;

	epd->lut_swap = 0;

	ret = epd_reset(epd);
	if (ret) return ret;

	ret = epd_wait_busy(epd);
	if (ret) return ret;

	/* PSR: panel setting */
	ret = epd_spi_write_cmd(epd, 0x00);
	if (ret) return ret;
	epd_spi_write_byte(epd, 0xFF);
	epd_spi_write_byte(epd, 0x01);

	/* PWR: power setting */
	ret = epd_spi_write_cmd(epd, 0x01);
	if (ret) return ret;
	epd_spi_write_byte(epd, 0x03);
	epd_spi_write_byte(epd, 0x10);
	epd_spi_write_byte(epd, 0x3F);
	epd_spi_write_byte(epd, 0x3F);
	epd_spi_write_byte(epd, 0x03);

	/* BTST: booster soft start */
	ret = epd_spi_write_cmd(epd, 0x06);
	if (ret) return ret;
	epd_spi_write_byte(epd, 0x37);
	epd_spi_write_byte(epd, 0x3D);
	epd_spi_write_byte(epd, 0x3D);

	/* TCON */
	ret = epd_spi_write_cmd(epd, 0x60);
	if (ret) return ret;
	epd_spi_write_byte(epd, 0x22);

	/* VCOM_DC */
	ret = epd_spi_write_cmd(epd, 0x82);
	if (ret) return ret;
	epd_spi_write_byte(epd, 0x07);

	/* PLL */
	ret = epd_spi_write_cmd(epd, 0x30);
	if (ret) return ret;
	epd_spi_write_byte(epd, 0x09);

	/* Temperature sensor */
	ret = epd_spi_write_cmd(epd, 0xE3);
	if (ret) return ret;
	epd_spi_write_byte(epd, 0x88);

	/* Resolution: 240x360 */
	ret = epd_spi_write_cmd(epd, 0x61);
	if (ret) return ret;
	epd_spi_write_byte(epd, 0xF0);
	epd_spi_write_byte(epd, 0x01);
	epd_spi_write_byte(epd, 0x68);

	/* Border */
	ret = epd_spi_write_cmd(epd, 0x50);
	if (ret) return ret;
	epd_spi_write_byte(epd, 0xB7);

	ret = epd_spi_write_cmd(epd, 0x50);
	if (ret) return ret;
	epd_spi_write_byte(epd, 0xD7);

	msleep(10);
	return 0;
}

static int epd_refresh(struct epd_100ask *epd)
{
	int ret;

	ret = epd_load_lut(epd);
	if (ret) return ret;

	/* Display update sequence */
	ret = epd_spi_write_cmd(epd, 0x17);
	if (ret) return ret;
	ret = epd_spi_write_byte(epd, 0xA5);
	if (ret) return ret;

	ret = epd_wait_busy(epd);
	if (ret) return ret;

	msleep(200);
	return 0;
}

static int epd_write_new_frame(struct epd_100ask *epd, const u8 *buf)
{
	int ret;

	/* The verified reference image path writes new SRAM only. */
	ret = epd_spi_write_cmd(epd, 0x13);
	if (ret) return ret;
	ret = epd_spi_write_data(epd, buf, EPD_BUF_SIZE);
	if (ret) return ret;

	return epd_refresh(epd);
}

static int epd_write_both_planes(struct epd_100ask *epd, const u8 *buf)
{
	int ret;

	ret = epd_spi_write_cmd(epd, 0x10);
	if (ret) return ret;
	ret = epd_spi_write_data(epd, buf, EPD_BUF_SIZE);
	if (ret) return ret;
	ret = epd_spi_write_cmd(epd, 0x13);
	if (ret) return ret;
	return epd_spi_write_data(epd, buf, EPD_BUF_SIZE);
}

static int epd_partial_update(struct epd_100ask *epd, const u8 *buf)
{
	const unsigned int stride = EPD_WIDTH / 8;
	unsigned int min_xb = stride, max_xb = 0;
	unsigned int min_y = EPD_HEIGHT, max_y = 0;
	unsigned int x, y, width_bytes, height;
	u8 window[7];
	u8 *region;
	int saved_lut = epd->lut_flag;
	int ret = 0;

	for (y = 0; y < EPD_HEIGHT; y++) {
		for (x = 0; x < stride; x++) {
			unsigned int off = y * stride + x;

			if (buf[off] == epd->fb[off])
				continue;
			if (x < min_xb) min_xb = x;
			if (x > max_xb) max_xb = x;
			if (y < min_y) min_y = y;
			if (y > max_y) max_y = y;
		}
	}

	if (min_y == EPD_HEIGHT) {
		epd->skipped_refreshes++;
		return 0;
	}

	width_bytes = max_xb - min_xb + 1;
	height = max_y - min_y + 1;
	region = kmalloc(width_bytes * height, GFP_KERNEL);
	if (!region)
		return -ENOMEM;

	for (y = 0; y < height; y++)
		memcpy(region + y * width_bytes,
		       buf + (min_y + y) * stride + min_xb, width_bytes);

	/* Follow the controller vendor's partial-window sequence. */
	ret = epd_init(epd);
	if (ret) goto out;
	ret = epd_spi_write_cmd(epd, 0x50);
	if (ret) goto out;
	ret = epd_spi_write_byte(epd, 0xF7);
	if (ret) goto out;
	ret = epd_spi_write_cmd(epd, 0x00);
	if (ret) goto out;
	ret = epd_spi_write_byte(epd, 0xFF);
	if (ret) goto out;
	ret = epd_spi_write_byte(epd, 0x01);
	if (ret) goto out;
	ret = epd_spi_write_cmd(epd, 0x91);
	if (ret) goto out;

	window[0] = min_xb * 8;
	window[1] = (max_xb + 1) * 8 - 1;
	window[2] = min_y >> 8;
	window[3] = min_y & 0xff;
	window[4] = max_y >> 8;
	window[5] = max_y & 0xff;
	window[6] = 0x01;
	ret = epd_spi_write_cmd(epd, 0x90);
	if (ret) goto out;
	ret = epd_spi_write_data(epd, window, sizeof(window));
	if (ret) goto out;
	ret = epd_spi_write_cmd(epd, 0x13);
	if (ret) goto out;
	ret = epd_spi_write_data(epd, region, width_bytes * height);
	if (ret) goto out;

	epd->lut_flag = LUT_DU;
	ret = epd_refresh(epd);
	if (ret) goto out;
	epd->partial_refreshes++;

	/* Exit partial mode and restore the normal full-screen settings. */
	ret = epd_init(epd);
out:
	epd->lut_flag = saved_lut;
	kfree(region);
	return ret;
}

static int epd_write_frame(struct epd_100ask *epd, const u8 *buf)
{
	int ret;

	if (!memcmp(epd->fb, buf, EPD_BUF_SIZE)) {
		epd->skipped_refreshes++;
		return 0;
	}

	if (epd->lut_flag == LUT_DU)
		ret = epd_partial_update(epd, buf);
	else {
		ret = epd_write_new_frame(epd, buf);
		if (!ret)
			epd->full_refreshes++;
	}

	if (!ret)
		memcpy(epd->fb, buf, EPD_BUF_SIZE);
	epd->last_error = ret;
	return ret;
}

static int epd_clear(struct epd_100ask *epd, u8 color)
{
	u8 *frame;
	int ret;

	frame = kmalloc(EPD_BUF_SIZE, GFP_KERNEL);
	if (!frame)
		return -ENOMEM;

	memset(frame, color, EPD_BUF_SIZE);
	ret = epd_write_both_planes(epd, frame);
	if (!ret)
		ret = epd_refresh(epd);
	if (!ret) {
		memcpy(epd->fb, frame, EPD_BUF_SIZE);
		epd->full_refreshes++;
	}
	epd->last_error = ret;
	kfree(frame);
	return ret;
}

/* ---- file ops ---- */
static ssize_t epd_write(struct file *filp, const char __user *buf,
			 size_t count, loff_t *f_pos)
{
	struct miscdevice *misc = filp->private_data;
	struct epd_100ask *epd = container_of(misc, struct epd_100ask, miscdev);
	u8 *kbuf;
	int ret;

	if (count != EPD_BUF_SIZE)
		return -EMSGSIZE;

	kbuf = kmalloc(EPD_BUF_SIZE, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	if (copy_from_user(kbuf, buf, EPD_BUF_SIZE)) {
		kfree(kbuf);
		return -EFAULT;
	}

	mutex_lock(&epd->lock);
	ret = epd_write_frame(epd, kbuf);
	mutex_unlock(&epd->lock);
	kfree(kbuf);

	return ret ? ret : EPD_BUF_SIZE;
}

static long epd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct miscdevice *misc = filp->private_data;
	struct epd_100ask *epd = container_of(misc, struct epd_100ask, miscdev);
	int ret;
	struct epd_100ask_info info;

	mutex_lock(&epd->lock);
	switch (cmd) {
	case EPD_IOC_CLEAR_WHITE: ret = epd_clear(epd, 0xFF); break;
	case EPD_IOC_CLEAR_BLACK: ret = epd_clear(epd, 0x00); break;
	case EPD_IOC_SET_LUT_GC:  epd->lut_flag = LUT_GC; ret = 0; break;
	case EPD_IOC_SET_LUT_DU:  epd->lut_flag = LUT_DU; ret = 0; break;
	case EPD_IOC_SET_LUT_5S:  epd->lut_flag = LUT_5S; ret = 0; break;
	case EPD_IOC_GET_INFO:
		memset(&info, 0, sizeof(info));
		info.width = EPD_WIDTH;
		info.height = EPD_HEIGHT;
		info.frame_size = EPD_BUF_SIZE;
		info.capabilities = EPD_100ASK_CAP_PARTIAL;
		info.full_refreshes = epd->full_refreshes;
		info.partial_refreshes = epd->partial_refreshes;
		info.skipped_refreshes = epd->skipped_refreshes;
		info.last_error = epd->last_error;
		info.refresh_mode = epd->lut_flag;
		ret = copy_to_user((void __user *)arg, &info, sizeof(info)) ? -EFAULT : 0;
		break;
	default: ret = -ENOTTY; break;
	}
	mutex_unlock(&epd->lock);
	return ret;
}

static const struct file_operations epd_fops = {
	.owner          = THIS_MODULE,
	.write          = epd_write,
	.unlocked_ioctl = epd_ioctl,
	.llseek         = no_llseek,
};

/* ---- spi probe / remove ---- */
static int epd_probe(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	struct epd_100ask *epd;
	int ret;

	epd = devm_kzalloc(dev, sizeof(*epd), GFP_KERNEL);
	if (!epd)
		return -ENOMEM;

	epd->spi = spi;
	epd->lut_flag = LUT_GC;
	mutex_init(&epd->lock);
	spi_set_drvdata(spi, epd);

	epd->dc_gpio = devm_gpiod_get(dev, "dc", GPIOD_OUT_HIGH);
	if (IS_ERR(epd->dc_gpio)) {
		dev_err(dev, "failed to get dc-gpios\n");
		return PTR_ERR(epd->dc_gpio);
	}

	epd->busy_gpio = devm_gpiod_get(dev, "busy", GPIOD_IN);
	if (IS_ERR(epd->busy_gpio)) {
		dev_err(dev, "failed to get busy-gpios\n");
		return PTR_ERR(epd->busy_gpio);
	}

	epd->reset_gpio = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(epd->reset_gpio)) {
		dev_err(dev, "failed to get reset-gpios\n");
		return PTR_ERR(epd->reset_gpio);
	}

	spi->mode = SPI_MODE_0;
	spi->bits_per_word = 8;
	if (!spi->max_speed_hz)
		spi->max_speed_hz = 10000000;

	ret = spi_setup(spi);
	if (ret) {
		dev_err(dev, "spi_setup failed: %d\n", ret);
		return ret;
	}

	epd->fb = devm_kzalloc(dev, EPD_BUF_SIZE, GFP_KERNEL);
	if (!epd->fb)
		return -ENOMEM;
	/* The panel normally powers up white; use that as the first old frame. */
	memset(epd->fb, 0xff, EPD_BUF_SIZE);

	ret = epd_init(epd);
	if (ret) {
		dev_err(dev, "panel init failed: %d\n", ret);
		return ret;
	}

	/* Establish a known white old/new SRAM state without flashing the panel. */
	ret = epd_write_both_planes(epd, epd->fb);
	if (ret) {
		dev_err(dev, "failed to initialize panel SRAM: %d\n", ret);
		return ret;
	}

	epd->miscdev.minor = MISC_DYNAMIC_MINOR;
	epd->miscdev.name  = "epd_100ask";
	epd->miscdev.fops  = &epd_fops;
	epd->miscdev.parent = dev;

	ret = misc_register(&epd->miscdev);
	if (ret) {
		dev_err(dev, "misc_register failed: %d\n", ret);
		return ret;
	}

	dev_info(dev, "100ASK EPD 240x360 registered as /dev/epd_100ask\n");
	return 0;
}

static int epd_remove(struct spi_device *spi)
{
	struct epd_100ask *epd = spi_get_drvdata(spi);
	misc_deregister(&epd->miscdev);
	return 0;
}

static const struct of_device_id epd_of_match[] = {
	{ .compatible = "100ask,epd-240x360" },
	{ }
};
MODULE_DEVICE_TABLE(of, epd_of_match);

static struct spi_driver epd_spi_driver = {
	.driver = {
		.name  = "epd_100ask_240x360",
		.of_match_table = epd_of_match,
	},
	.probe  = epd_probe,
	.remove = epd_remove,
};
module_spi_driver(epd_spi_driver);

MODULE_DESCRIPTION("100ASK 240x360 SPI EPD Driver");
MODULE_LICENSE("GPL");
