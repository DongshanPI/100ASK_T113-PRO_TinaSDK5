# 100ASK 240x360 墨水屏 Linux 5.4 驱动改造需求文档

## 1. 目标

将 ESP32 上已经验证可用的 `100ASK 240x360 SPI 墨水屏驱动` 改造成 **Allwinner T113 / TinaSDK / Linux 5.4 内核驱动**。

目标输出为 Linux 内核 misc 字符设备驱动：

```text
/dev/epd_100ask
```

用户空间通过写入一帧 `240x360 1bpp` 图像数据来刷新墨水屏：

```bash
cat image_240x360_1bpp.bin > /dev/epd_100ask
```

单帧数据大小固定为：

```text
240 * 360 / 8 = 10800 bytes
```

---

## 2. 硬件连接

当前 T113 主控信号如下：

```text
VCC_3V3      -> 墨水屏 VCC
GND          -> 墨水屏 GND
SPI1_MOSI    -> PD12
SPI1_SCK     -> PD11
SPI1_CS      -> PD10
DC_CTRL      -> PD22
ELINK_BUSY   -> PB3
LCD_RESET    -> 当前接 3V3
```

说明：

- `LCD_RESET` 当前没有接 GPIO，直接拉到 `3V3`。
- Linux 驱动里 `reset-gpios` 必须设计成 optional。
- 如果后续需要硬复位，我会自己飞线到其他 GPIO，例如 `PB4`。
- `BUSY` 逻辑：`0 = busy`，`1 = idle`。

---

## 3. 需要新增的内核目录结构

在 Linux 5.4 内核源码中新增：

```text
kernel/linux-5.4/drivers/misc/epd_100ask/
├── Kconfig
├── Makefile
└── epd_100ask_240x360.c
```

并修改上级文件：

```text
kernel/linux-5.4/drivers/misc/Kconfig
kernel/linux-5.4/drivers/misc/Makefile
```

---

## 4. Kconfig 要求

新增文件：

```text
drivers/misc/epd_100ask/Kconfig
```

内容：

```makefile
config EPD_100ASK_240X360
	tristate "100ASK 240x360 SPI EPD driver"
	depends on SPI
	depends on GPIOLIB
	help
	  100ASK 240x360 SPI e-paper display driver for Linux 5.4.
```

修改：

```text
drivers/misc/Kconfig
```

添加：

```makefile
source "drivers/misc/epd_100ask/Kconfig"
```

---

## 5. Makefile 要求

新增文件：

```text
drivers/misc/epd_100ask/Makefile
```

内容：

```makefile
obj-$(CONFIG_EPD_100ASK_240X360) += epd_100ask_240x360.o
```

修改：

```text
drivers/misc/Makefile
```

添加：

```makefile
obj-$(CONFIG_EPD_100ASK_240X360) += epd_100ask/
```

---

## 6. config-5.4 要求

在 TinaSDK 的配置文件中开启：

```text
CONFIG_SPI=y
CONFIG_GPIOLIB=y
CONFIG_EPD_100ASK_240X360=y
```

如果希望编译成模块，则使用：

```text
CONFIG_EPD_100ASK_240X360=m
```

---

## 7. 设备树要求

修改：

```text
device/config/chips/t113/configs/evb1_auto_nand/linux-5.4/board.dts
```

添加或合并以下节点。

### 7.1 PIO 节点

```dts
&pio {
	epd_dc_pin: epd-dc-pin {
		pins = "PD22";
		function = "gpio_out";
		drive-strength = <20>;
		bias-disable;
	};

	epd_busy_pin: epd-busy-pin {
		pins = "PB3";
		function = "gpio_in";
		bias-pull-up;
	};

	/*
	 * 可选 RESET 引脚。
	 * 当前 LCD_RESET 直接接 3V3，因此默认不要在屏幕节点里启用 reset-gpios。
	 * 如果后续飞线到 PB4，再启用。
	 */
	epd_reset_pin: epd-reset-pin {
		pins = "PB4";
		function = "gpio_out";
		drive-strength = <20>;
		bias-pull-up;
	};

	spi1_pins: spi1-pins {
		pins = "PD11", "PD12";
		function = "spi1";
		drive-strength = <20>;
		bias-disable;
	};

	spi1_cs_pin: spi1-cs-pin {
		pins = "PD10";
		function = "gpio_out";
		drive-strength = <20>;
		bias-pull-up;
	};
};
```

### 7.2 SPI1 节点

```dts
&spi1 {
	status = "okay";
	pinctrl-names = "default";
	pinctrl-0 = <&spi1_pins &spi1_cs_pin>;

	cs-gpios = <&pio 3 10 GPIO_ACTIVE_LOW>; /* PD10 */

	epd_100ask@0 {
		compatible = "100ask,epd-240x360";
		reg = <0>;
		spi-max-frequency = <10000000>;

		dc-gpios = <&pio 3 22 GPIO_ACTIVE_HIGH>;   /* PD22 */
		busy-gpios = <&pio 1 3 GPIO_ACTIVE_HIGH>;  /* PB3 */

		/*
		 * 当前 RESET 接 3V3，不要打开。
		 * 如果 LCD_RESET 飞线到 PB4，再打开：
		 */
		/* reset-gpios = <&pio 1 4 GPIO_ACTIVE_HIGH>; */

		status = "okay";
	};
};
```

注意：

- SPI mode 使用 mode 0。
- 不要在设备树里写 `spi-cpol` 或 `spi-cpha`。
- 如果 T113 的 pinctrl 名称已存在，需要复用现有 SPI1 pinctrl，避免重复定义冲突。

---

## 8. Linux 驱动功能要求

驱动文件：

```text
drivers/misc/epd_100ask/epd_100ask_240x360.c
```

必须实现：

```text
1. SPI 驱动 probe/remove
2. misc 字符设备 /dev/epd_100ask
3. 写入 10800 bytes 图像数据后刷新屏幕
4. 默认使用 GC 全刷 LUT
5. 支持 DU、5S LUT 逻辑
6. 支持 ioctl 清白屏、清黑屏、切换 LUT
7. 支持 optional reset-gpios
8. BUSY 等待超时保护
```

---

## 9. 驱动关键参数

```c
#define EPD_WIDTH        240
#define EPD_HEIGHT       360
#define EPD_BUF_SIZE     (EPD_WIDTH * EPD_HEIGHT / 8)
```

即：

```text
EPD_BUF_SIZE = 10800
```

---

## 10. 驱动兼容字符串

设备树和驱动必须匹配：

```c
compatible = "100ask,epd-240x360";
```

Linux 驱动中：

```c
static const struct of_device_id epd_of_match[] = {
	{ .compatible = "100ask,epd-240x360" },
	{ }
};
MODULE_DEVICE_TABLE(of, epd_of_match);
```

---

## 11. GPIO 获取要求

DC：

```c
epd->dc_gpio = devm_gpiod_get(&spi->dev, "dc", GPIOD_OUT_HIGH);
```

BUSY：

```c
epd->busy_gpio = devm_gpiod_get(&spi->dev, "busy", GPIOD_IN);
```

RESET 必须 optional：

```c
epd->reset_gpio = devm_gpiod_get_optional(&spi->dev, "reset", GPIOD_OUT_HIGH);
```

如果没有 `reset-gpios`，驱动不能 probe 失败。

---

## 12. SPI 设置要求

```c
spi->mode = SPI_MODE_0;
spi->bits_per_word = 8;

if (!spi->max_speed_hz)
	spi->max_speed_hz = 10000000;

ret = spi_setup(spi);
```

---

## 13. 基础 SPI 写命令逻辑

命令模式：

```c
gpiod_set_value_cansleep(epd->dc_gpio, 0);
spi_write(epd->spi, &cmd, 1);
```

数据模式：

```c
gpiod_set_value_cansleep(epd->dc_gpio, 1);
spi_write(epd->spi, data, len);
```

---

## 14. BUSY 检测逻辑

ESP32 版本逻辑为：

```c
while (0 == gpio_get_level(g_epd_t->pin_busy))
    delay;
```

Linux 内核中应改成：

```c
static int epd_wait_busy(struct epd_100ask *epd)
{
	unsigned long timeout;
	int val;

	timeout = jiffies + msecs_to_jiffies(30000);

	do {
		val = gpiod_get_value_cansleep(epd->busy_gpio);
		if (val < 0)
			return val;

		if (val == 1)
			return 0;

		msleep(10);
	} while (time_before(jiffies, timeout));

	dev_err(&epd->spi->dev, "busy timeout\n");
	return -ETIMEDOUT;
}
```

---

## 15. RESET 逻辑

RESET 可选。

如果有 reset GPIO：

```c
gpiod_set_value_cansleep(epd->reset_gpio, 1);
msleep(20);
gpiod_set_value_cansleep(epd->reset_gpio, 0);
msleep(20);
gpiod_set_value_cansleep(epd->reset_gpio, 1);
msleep(20);
```

如果没有 reset GPIO：

```c
dev_warn_once(&epd->spi->dev, "no reset-gpios, skip hardware reset\n");
msleep(50);
return 0;
```

---

## 16. 初始化命令移植

ESP32 初始化命令需要完整移植到 Linux：

```c
0x00: 0xFF, 0x01
0x01: 0x03, 0x10, 0x3F, 0x3F, 0x03
0x06: 0x37, 0x3D, 0x3D
0x60: 0x22
0x82: 0x07
0x30: 0x09
0xE3: 0x88
0x61: 0xF0, 0x01, 0x68
0x50: 0xB7
0x50: 0xD7
```

---

## 17. 显示刷新逻辑

### 17.1 清屏

清白屏：

```c
memset(fb, 0xFF, 10800);
```

清黑屏：

```c
memset(fb, 0x00, 10800);
```

清屏时需要同时写旧图和新图：

```c
cmd 0x10 -> 10800 bytes
cmd 0x13 -> 10800 bytes
refresh
```

### 17.2 显示图片

显示图片时至少需要：

```c
cmd 0x13 -> 10800 bytes
refresh
```

建议第一版也可以同时写：

```c
cmd 0x10 -> 10800 bytes
cmd 0x13 -> 10800 bytes
refresh
```

如果发现残影或刷新异常，优先改为同时写 `0x10` 和 `0x13`。

---

## 18. 刷新命令

刷新前先下载 LUT。

刷新命令：

```c
cmd 0x17
data 0xA5
wait busy
msleep(200)
```

---

## 19. LUT 要求

需要从 ESP32 版本完整复制以下 LUT：

```text
epd_240x360_lut_R20_GC[56]
epd_240x360_lut_R21_GC[42]
epd_240x360_lut_R22_GC[56]
epd_240x360_lut_R23_GC[42]
epd_240x360_lut_R24_GC[42]

epd_240x360_lut_R20_DU[56]
epd_240x360_lut_R21_DU[42]
epd_240x360_lut_R22_DU[56]
epd_240x360_lut_R23_DU[42]
epd_240x360_lut_R24_DU[42]

epd_240x360_lut_vcom[42]
epd_240x360_lut_ww[42]
epd_240x360_lut_bw[42]
epd_240x360_lut_wb[42]
epd_240x360_lut_bb[42]
```

LUT 写入命令：

```text
0x20 -> VCOM / R20
0x21 -> R21
0x22 -> R22 或 R23，按 lut_flag 切换
0x23 -> R23 或 R22，按 lut_flag 切换
0x24 -> R24
```

---

## 20. ioctl 要求

定义：

```c
#define EPD_IOC_MAGIC       'E'
#define EPD_IOC_CLEAR_WHITE _IO(EPD_IOC_MAGIC, 0x01)
#define EPD_IOC_CLEAR_BLACK _IO(EPD_IOC_MAGIC, 0x02)
#define EPD_IOC_SET_LUT_GC  _IO(EPD_IOC_MAGIC, 0x03)
#define EPD_IOC_SET_LUT_DU  _IO(EPD_IOC_MAGIC, 0x04)
#define EPD_IOC_SET_LUT_5S  _IO(EPD_IOC_MAGIC, 0x05)
```

功能：

```text
EPD_IOC_CLEAR_WHITE -> 清白屏并刷新
EPD_IOC_CLEAR_BLACK -> 清黑屏并刷新
EPD_IOC_SET_LUT_GC  -> 后续 write 使用 GC 全刷
EPD_IOC_SET_LUT_DU  -> 后续 write 使用 DU 局刷波形
EPD_IOC_SET_LUT_5S  -> 后续 write 使用 5S 波形
```

---

## 21. 用户空间测试

### 21.1 检查设备

```bash
ls -l /dev/epd_100ask
dmesg | grep -i epd
```

### 21.2 白屏测试

```bash
dd if=/dev/zero bs=10800 count=1 | tr '\000' '\377' > white.bin
cat white.bin > /dev/epd_100ask
```

### 21.3 黑屏测试

```bash
dd if=/dev/zero bs=10800 count=1 > black.bin
cat black.bin > /dev/epd_100ask
```

### 21.4 灰度/棋盘测试

```bash
python3 - <<'PY'
w, h = 240, 360
buf = bytearray()
for y in range(h):
    for x in range(0, w, 8):
        v = 0xAA if ((y // 16) % 2 == 0) else 0x55
        buf.append(v)
open("checker.bin", "wb").write(buf)
print(len(buf))
PY

cat checker.bin > /dev/epd_100ask
```

---

## 22. 编译验证

进入 TinaSDK：

```bash
make kernel_menuconfig
```

确认：

```text
Device Drivers
  Misc devices
    <*> 100ASK 240x360 SPI EPD driver
```

然后编译：

```bash
make kernel
make
```

烧录启动后检查：

```bash
dmesg | grep -i "100ASK EPD"
ls /dev/epd_100ask
```

---

## 23. 常见问题排查

### 23.1 没有 `/dev/epd_100ask`

检查：

```bash
dmesg | grep -i epd
dmesg | grep -i spi
```

重点看：

```text
compatible 是否匹配
SPI1 是否启用
GPIO 是否被其他功能占用
CONFIG_EPD_100ASK_240X360 是否开启
```

### 23.2 probe 失败，提示 reset GPIO

原因：

```text
RESET 当前接 3V3，但设备树写了 reset-gpios。
```

解决：

```text
删除 reset-gpios。
```

或者飞线 RESET 到 GPIO 后再启用。

### 23.3 busy timeout

检查：

```text
PB3 是否接到 ELINK_BUSY
busy-gpios 极性是否正确
屏幕是否供电正常
RESET 是否需要飞线控制
```

### 23.4 SPI 有波形但不显示

检查：

```text
SPI mode 是否为 mode 0
PD11/PD12 是否 pinmux 到 spi1
PD10 CS 是否正确
DC_CTRL PD22 是否正常翻转
RESET 是否需要 GPIO 复位
```

### 23.5 显示花屏或方向不对

检查：

```text
输入文件是否正好 10800 bytes
图像是否为 240x360 1bpp
bit 顺序是否与屏幕要求一致
初始化命令 0x61 是否正确：0xF0, 0x01, 0x68
```

---

## 24. 验收标准

完成后必须满足：

```text
1. 内核能编译通过
2. 设备树能编译通过
3. 启动后生成 /dev/epd_100ask
4. dmesg 显示 100ASK EPD 240x360 registered
5. 写入 white.bin 可以刷白
6. 写入 black.bin 可以刷黑
7. BUSY 等待不会无限死等，有 timeout
8. reset-gpios 不配置时驱动仍能 probe
9. 后续飞线 RESET 后，添加 reset-gpios 可正常硬复位
```

---

## 25. 需要 AI 自动修改的文件清单

请自动完成以下文件修改：

```text
新增：
kernel/linux-5.4/drivers/misc/epd_100ask/epd_100ask_240x360.c
kernel/linux-5.4/drivers/misc/epd_100ask/Kconfig
kernel/linux-5.4/drivers/misc/epd_100ask/Makefile

修改：
kernel/linux-5.4/drivers/misc/Kconfig
kernel/linux-5.4/drivers/misc/Makefile
device/config/chips/t113/configs/evb1_auto_nand/linux-5.4/board.dts
device/config/chips/t113/configs/evb1_auto_nand/linux-5.4/config-5.4
```

---

## 26. 重要提醒

当前屏幕 RESET 接 3V3，理论上可以尝试不复位直接初始化，但墨水屏很多控制器对复位时序比较敏感。

如果出现以下情况：

```text
第一次上电偶尔能刷
热重启不能刷
busy timeout
SPI 有波形但屏幕无反应
```

请优先飞线 `LCD_RESET` 到一个空闲 GPIO，例如 `PB4`，然后设备树添加：

```dts
reset-gpios = <&pio 1 4 GPIO_ACTIVE_HIGH>;
```
