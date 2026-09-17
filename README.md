# 100ASK T113-PRO 240×360 E-Ink OS

这是面向 **100ASK T113-PRO** 的 TinaSDK 5.0 源码覆盖层，在
`tina5v1p2-xr829` 基线上加入 3.52 英寸、240×360 单色墨水屏驱动和
E-Ink OS 桌面应用。

本仓库不是完整 TinaSDK。仓库目录与 SDK 根目录保持一致，clone 后将文件
覆盖到 TinaSDK 根目录即可编译出包含墨水屏支持的系统镜像。

## 已实现功能

- Linux 5.4 SPI 墨水屏驱动，设备节点为 `/dev/epd_100ask`
- 240×360、1 bpp 帧缓冲，单帧大小 10800 字节
- GC 全刷、DU 局刷和 5S 刷新波形
- 白屏、黑屏、刷新模式和驱动状态 ioctl
- BUSY 超时保护、重复帧跳过和刷新次数统计
- 基于 LVGL 8 的中英文 E-Ink OS 桌面
- 系统概览、日历、TXT 阅读器、网络状态、资源监控、设备诊断、设置和关于页面
- Wi-Fi 扫描、周期刷新和定期全刷
- OpenWrt 软件包、开机自启动服务和墨水屏测试工具

## 适用环境

| 项目 | 配置 |
| --- | --- |
| 开发板 | 100ASK T113-PRO |
| SDK | TinaSDK 5.0 v1.2 |
| 基线分支 | `tina5v1p2-xr829` |
| 芯片配置 | `t113` |
| 板级配置 | `evb1_auto_nand` |
| 内核 | Linux 5.4.61 |
| 根文件系统 | OpenWrt |
| 显示屏 | 3.52 英寸 240×360 单色 EPD |

## 硬件连接

本配置用于插在开发板 9 针 SPI TFT 接口上的 100ASK SPI-to-EPD 转接板。

| EPD 信号 | T113 引脚 | 功能 |
| --- | --- | --- |
| CS | PD10 | SPI1_CS0 |
| SCK | PD11 | SPI1_CLK |
| MOSI | PD12 | SPI1_MOSI |
| RESET | PD13 | 转接板复用原 MISO 引脚 |
| DC | PB5 | 命令/数据选择 |
| BUSY | PD22 | 低电平忙，高电平空闲 |

这些引脚与 LCD0 和 PWM7 存在复用冲突，因此本覆盖层会关闭 LCD0 和 PWM7。
SPI 仅发送数据，不使用 MISO。

## 获取源码

```bash
git clone --depth 1 --branch einkos-240x360 \
  git@github.com:DongshanPI/100ASK_T113-PRO_TinaSDK5.git
```

如果当前网络不能访问 GitHub SSH 的 22 端口，可改用 HTTPS clone，或者配置
SSH 通过 `ssh.github.com:443` 连接。

## 应用到 TinaSDK

假设完整 SDK 位于 `/path/to/T113-tina5v1.2-sdk`：

```bash
cd /path/to/T113-tina5v1.2-sdk

# 建议先确认 SDK 中没有需要保留的未提交修改
.repo/repo/repo status

rsync -a \
  --exclude=.git \
  --exclude=README.md \
  /path/to/100ASK_T113-PRO_TinaSDK5/ \
  /path/to/T113-tina5v1.2-sdk/
```

`rsync` 会覆盖同名源码和配置，但不会删除 SDK 中的其他文件。根目录
`README.md` 只用于 GitHub 展示，因此示例命令不会用它覆盖 SDK 自带文档。

## 配置与编译

首次使用 SDK 时执行配置，并选择：

- Chip：`t113`
- Board：`evb1_auto_nand`
- Kernel：`linux-5.4`
- Linux device：`openwrt`
- Flash：`default`

```bash
cd /path/to/T113-tina5v1.2-sdk
./build.sh config
./build.sh
./build.sh pack
```

如果 SDK 已经完成上述配置，可以直接增量构建：

```bash
./build.sh kernel
./build.sh
./build.sh pack
```

最终固件位于：

```text
out/t113_linux_evb1_auto_nand_uart0.img
```

也可以单独交叉编译用户态程序：

```bash
make -C eink-3.52/lvgl_epd_demo clean all
make -C eink-3.52/epd_test clean all
```

## 上板验证

系统启动后先确认驱动已经探测：

```bash
dmesg | grep -i epd
ls -l /dev/epd_100ask
epd-test info
```

正常日志应包含：

```text
100ASK EPD 240x360 registered as /dev/epd_100ask
```

运行测试图案：

```bash
epd-test white
epd-test black
epd-test checker
epd-test border
epd-test sequence

# 使用 DU 局刷
epd-test --partial checker
```

E-Ink OS 默认由 `/etc/init.d/eink-dashboard` 在开机时启动，也可以手动控制：

```bash
/etc/init.d/eink-dashboard stop
/etc/init.d/eink-dashboard start
logread | grep -i eink
```

默认配置文件为 `/etc/eink-dashboard.conf`：

```ini
language=zh_CN
refresh_interval_sec=60
full_refresh_every=10
key_previous=115
key_next=114
key_next_alt=119
key_confirm=28
key_confirm_alt=373
```

## 操作方式

- 上一个/下一个按键：移动选择、切换应用或翻页
- 确认键短按：打开应用、执行当前操作
- 确认键长按：返回桌面；在桌面长按可切换中英文
- 桌面默认每 60 秒更新一次状态，每 10 次局刷执行一次全刷

## 主要目录

| 路径 | 内容 |
| --- | --- |
| `kernel/linux-5.4/drivers/misc/epd_100ask/` | 240×360 内核驱动 |
| `kernel/linux-5.4/include/uapi/linux/epd_100ask.h` | 用户态 ioctl 接口 |
| `eink-3.52/lvgl_epd_demo/` | E-Ink OS 桌面源码 |
| `eink-3.52/epd_test/` | 墨水屏测试程序 |
| `openwrt/package/thirdparty/gui/eink-dashboard/` | OpenWrt 软件包和启动服务 |
| `device/config/chips/t113/configs/evb1_auto_nand/` | 内核配置和设备树 |

更详细的引脚分析、驱动接口和排障流程请阅读：

- [移植与上板记录](eink-3.52/PORTING_240X360_T113.md)
- [Linux 5.4 驱动设计与接口说明](eink-3.52/100ask_epd_t113_linux54_driver_requirements.md)

## 常见问题

### 没有 `/dev/epd_100ask`

确认内核配置包含 `CONFIG_EPD_100ASK_240X360=y`，并检查设备树中的 SPI1 和
GPIO 配置。查看完整启动日志：

```bash
dmesg | grep -Ei 'epd|spi|gpio'
```

### 驱动探测等待约 30 秒后失败

优先检查 BUSY 信号。PD22 在空闲状态应为高电平；随后检查 PD13 的高-低-高
复位波形、PB5 的 DC 电平以及 PD11/PD12 的 SPI 时钟和数据波形。

### 屏幕内容方向或黑白颜色不正确

驱动要求 240×360、1 bpp、MSB first，白色位为 1、黑色位为 0。每次
`write()` 必须正好写入 10800 字节。

## 许可证

内核驱动采用 GPL-2.0-only。E-Ink OS 应用及第三方字体的许可证和声明见
[THIRD_PARTY_NOTICES](openwrt/package/thirdparty/gui/eink-dashboard/files/THIRD_PARTY_NOTICES)
和 [OFL-1.1](openwrt/package/thirdparty/gui/eink-dashboard/files/OFL-1.1.txt)。
