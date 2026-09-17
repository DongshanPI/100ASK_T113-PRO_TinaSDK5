# 100ASK T113s3-PRO TinaSDK5 扩展支持包

本仓库为 **100ASK T113s3-PRO V1.3** 提供 Tina5 SDK 板级扩展文件，包含
Buildroot、设备树、内核和无线模块相关配置。

> 本仓库不是完整 Tina5 SDK。请先取得完整 SDK 源码包，再将本仓库内容覆盖
> 到 SDK 根目录。

本文根据百问网《[开发环境搭建](https://dshanpi.100ask.net/docs/T113s3-Pro/part6/part6-2/DevelopmentEnvironmentSetup)》
重新整理，命令中的目录名可以按自己的实际路径调整。

## 1. 选择正确分支

T113s3-PRO 有不同的 Wi-Fi 硬件版本，补丁分支不能混用：

| 开发板版本 | Wi-Fi 模组 | 本仓库分支 |
| --- | --- | --- |
| T113s3-PRO WiFi6 增强版 | AIC8800D80 | `master` |
| T113s3-PRO XR829 版 | XR829 | `tina5v1p2-xr829` |
| 240×360 墨水屏扩展 | 基于 XR829 配置 | `einkos-240x360` |

不确定开发板版本时，请先核对板载 Wi-Fi 模组丝印，再选择分支。

## 2. 准备编译环境

官方教程以 64 位 Ubuntu 18.04 为例。先启用 i386 多架构并安装依赖：

```bash
sudo dpkg --add-architecture i386
sudo apt-get update
sudo apt-get install -y \
  build-essential subversion git \
  libncurses5-dev zlib1g-dev gawk flex quilt libssl-dev \
  xsltproc libxml-parser-perl mercurial bzr ecj cvs unzip \
  lib32z1 lib32z1-dev lib32stdc++6 libstdc++6 \
  libc6:i386 libstdc++6:i386 lib32ncurses5 bison rsync tree
```

较新的 Ubuntu 发行版可能不再提供部分兼容包；如果依赖名称或工具链运行出现
问题，优先使用 Ubuntu 18.04 虚拟机或容器环境。

## 3. 获取并校验 Tina5 SDK

从百问网《[源码工具文档手册](https://dshanpi.100ask.net/docs/T113s3-Pro/SupportingResources/)》
下载 Tina5 SDK 分卷压缩包。完整下载后，目录中应包含：

```text
Tina_SDK/
├── md5.txt
├── 100ASK_T113s3-PRO_TinaSDK5.tar.gz.00
├── 100ASK_T113s3-PRO_TinaSDK5.tar.gz.01
└── 100ASK_T113s3-PRO_TinaSDK5.tar.gz.02
```

校验三个分卷：

```bash
cd ~/Tina_SDK
md5sum 100ASK_T113s3-PRO_TinaSDK5.tar.gz.0*
```

预期 MD5：

```text
35577ee74334ee8bd9c5fcca844795b3  100ASK_T113s3-PRO_TinaSDK5.tar.gz.00
3420eef596165883cde2e14a41a12358  100ASK_T113s3-PRO_TinaSDK5.tar.gz.01
ad730f3b76e3943652b56c8c5335f52c  100ASK_T113s3-PRO_TinaSDK5.tar.gz.02
```

任一校验值不一致，都应重新下载对应分卷，不要继续解压。

合并并解压：

```bash
cd ~/Tina_SDK
cat 100ASK_T113s3-PRO_TinaSDK5.tar.gz.0* | tar -xzvf -
mv 100ASK_T113s3-PRO_TinaSDK5 ~/
```

后续命令假设完整 SDK 位于：

```text
~/100ASK_T113s3-PRO_TinaSDK5
```

注意两个相近的名称：

- 完整 SDK：`100ASK_T113s3-PRO_TinaSDK5`
- 本扩展仓库：`100ASK_T113-PRO_TinaSDK5`

## 4. 应用板级扩展

### 4.1 WiFi6 增强版（AIC8800D80）

WiFi6 增强版使用默认的 `master` 分支：

```bash
cd ~
git clone --depth 1 --branch master \
  https://github.com/DongshanPI/100ASK_T113-PRO_TinaSDK5.git

rsync -a \
  --exclude=.git \
  --exclude=README.md \
  ~/100ASK_T113-PRO_TinaSDK5/ \
  ~/100ASK_T113s3-PRO_TinaSDK5/
```

### 4.2 XR829 版

XR829 版必须显式选择 `tina5v1p2-xr829`：

```bash
cd ~
git clone --depth 1 --branch tina5v1p2-xr829 \
  https://github.com/DongshanPI/100ASK_T113-PRO_TinaSDK5.git \
  100ASK_T113-PRO_TinaSDK5-xr829

rsync -a \
  --exclude=.git \
  --exclude=README.md \
  ~/100ASK_T113-PRO_TinaSDK5-xr829/ \
  ~/100ASK_T113s3-PRO_TinaSDK5/
```

`rsync` 会覆盖同名文件，但不会删除 SDK 中的其他内容。应用补丁前，建议先备份
自己已经修改过的板级配置和驱动。

## 5. 初始化 SDK 环境

进入完整 SDK 根目录并加载环境：

```bash
cd ~/100ASK_T113s3-PRO_TinaSDK5
source build/envsetup.sh
```

每次打开新终端都需要重新执行 `source build/envsetup.sh`，否则 `croot`、
`ckernel` 等快捷命令和构建环境变量不会生效。

## 6. 选择板级方案

执行：

```bash
./build.sh config
```

菜单序号可能随 SDK 内容变化，请按名称选择，不要死记数字：

| 配置项 | 选择值 |
| --- | --- |
| Platform | `linux` |
| Linux device | `buildroot` |
| IC | `t113` |
| Board | `evb1_auto_nand` |
| Flash | `default` |

配置完成后会在 SDK 根目录生成 `.buildconfig`。可以用下面的命令快速核对：

```bash
grep -E 'LICHEE_(PLATFORM|LINUX_DEV|IC|BOARD|FLASH)=' .buildconfig
```

其中 Buildroot defconfig 应为：

```text
sun8iw20p1_t113_nand_defconfig
```

## 7. 编译与打包

完整编译：

```bash
./build.sh
```

看到 `build OK` 或相应的成功提示后再打包：

```bash
./build.sh pack
```

固件通常生成在：

```text
out/t113/evb1_auto_nand/buildroot/t113_linux_evb1_auto_nand_uart0.img
```

不同 SDK 小版本也可能把最终镜像复制到 `out/` 顶层，可以用以下命令定位：

```bash
find out -maxdepth 5 -type f -name 't113_linux_evb1_auto_nand*.img' -print
```

## 8. 常用构建命令

```bash
# 完整编译
./build.sh

# 单独编译引导程序
./build.sh bootloader

# 单独编译内核
./build.sh kernel

# 单独编译 Buildroot 根文件系统
./build.sh buildroot_rootfs

# 内核配置及保存
./build.sh menuconfig
./build.sh saveconfig

# Buildroot 配置及保存
./build.sh buildroot_menuconfig
./build.sh buildroot_saveconfig

# 清理构建产物
./build.sh clean

# 普通打包
./build.sh pack

# 打包调试版本
./build.sh pack_debug

# 打包安全启动版本
./build.sh pack_secure
```

## 9. 更新扩展仓库

已经 clone 过补丁仓库时，可以更新后重新覆盖：

```bash
cd ~/100ASK_T113-PRO_TinaSDK5
git pull --ff-only

rsync -a \
  --exclude=.git \
  --exclude=README.md \
  ./ ~/100ASK_T113s3-PRO_TinaSDK5/
```

请保持当前 Git 分支与开发板 Wi-Fi 版本一致。

## 10. 常见问题

### 找不到 `build.sh` 或快捷命令

确认当前目录是完整 SDK 根目录，而不是本扩展仓库；然后重新加载环境：

```bash
cd ~/100ASK_T113s3-PRO_TinaSDK5
source build/envsetup.sh
```

### 编译到了错误的方案

重新执行 `./build.sh config`，按名称选择 `linux`、`buildroot`、`t113`、
`evb1_auto_nand` 和 `default`。不要仅依赖菜单序号。

### 依赖包安装失败

确认使用 64 位 Ubuntu，并已启用 i386 多架构。如果新版本 Ubuntu 缺少
`libncurses5` 等旧兼容包，建议切换到官方教程使用的 Ubuntu 18.04 环境。

### 补丁覆盖后无线网络不可用

首先检查分支是否选错：AIC8800D80 使用 `master`，XR829 使用
`tina5v1p2-xr829`。两个版本的无线固件和配置不能混用。

## 11. 烧录与更多资料

- [T113s3-PRO 开发环境搭建原文](https://dshanpi.100ask.net/docs/T113s3-Pro/part6/part6-2/DevelopmentEnvironmentSetup)
- [源码和工具下载说明](https://dshanpi.100ask.net/docs/T113s3-Pro/SupportingResources/)
- [快速开始与固件烧录](https://dshanpi.100ask.net/docs/T113s3-Pro/part1/03-1_FlashSystem/)
- [T113s3-PRO 版本差异说明](https://dshanpi.100ask.net/docs/T113s3-Pro/intro/)

## License

本仓库中的各组件沿用其原始项目许可证。Allwinner、Tina Linux、XR829、
AIC8800D80 及其他第三方组件的版权归各自权利人所有。
