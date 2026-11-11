# Z96A U-Boot 原版构建复现与 B1 早期 LED

## 沙箱

```text
/home/hsy/project/rea-z97a/armbian-uboot
```

从归档的 Armbian 构建树复制，使用原版：

- Docker 镜像 `ghcr.io/armbian/docker-armbian-build:armbian-ubuntu-jammy-latest`
- gcc 11.4.0
- `rk3568_bl31_v1.46.elf`
- 定制 `rk3568_ddr_1184MHz_android5042_z96a.bin`
- binman 生成 FIT
- 板级补丁 0005–0008

构建命令：

```sh
SOURCE_DATE_EPOCH=1793132605 ./compile.sh uboot BOARD=z96a-v2-mainline \
  BRANCH=mainline DOWNLOAD_MIRROR=china EXPERT=yes \
  KERNEL_CONFIGURE=no RELEASE=bookworm
```

（`SOURCE_DATE_EPOCH` 通过沙箱 `lib/functions/host/docker.sh` 传入容器。）

## 基线复现结果

```text
idbloader.img  7d799106bd6942bd57e25c56d7a1c89cd354ab836bdeeed3a1ddcee86c09fe4f
u-boot image0   28bafb63aaaa82e9f940b151cb5ebed6f4551634d11ff3c680f346bc0fbffb0c
```

两者与 eMMC 上的部署版**逐字节一致**。

`u-boot.itb` 仅 FIT 头 `timestamp` 差 1 个字节（20:23:25 vs 20:23:29）。
将该字节规范为 20:23:29 后：

```text
u-boot.itb      efd764f4aaf53ec588884447b38c1a5b4b3d301cbf3081398330f9272e7a2895
```

与部署版完全一致。产物在 `build/out/armbian-baseline/`。

## B1：env-only 早期 LED

新增 Armbian 补丁：

```text
patch/u-boot/legacy/board_z96a-v2-mainline/0009-enable-early-power-led.patch
```

只改 `CONFIG_BOOTCOMMAND`，保持总长度不变：

```text
gpio s A6;gpio s E11;gpio s E9;gpio s C31;gpio s A27;usb start;bootflow scan -lb
```

- `A27` = GPIO0_D3 = 白色电源灯（active high）
- `gpio s` 与 `gpio set` 等价（cmd_gpio 只取首字符）
- 四条 USB 5V 轨命令全部保留
- `usb start`、`bootflow scan -lb`、`boot_targets` 不变
- 如果点灯失败，cmd_gpio 打印错误后继续执行 `usb start`

### 与部署版的差异（已逐字节核对）

`u-boot.itb` 大小仍为 1,220,096；`image0` 大小仍为 948,960；ATF、DTB、data-offset 全部不变。

差异总计 105 字节：

```text
0x1a4-0x1c3   32 字节   FIT 中 image0 的 SHA-256 值
0xb0142-0xb018d 76 字节  bootcmd 字符串
```

即：

- 没有任何指令字节变化；
- 没有 DTB 变化；
- 没有 ATF 变化；
- 没有 FIT 布局变化；
- 只有 bootcmd 文本和它的哈希。

产物：

```text
build/out/armbian-b1/u-boot.itb   b1d7abfd17b44203aaa9868531ab27277f8d3216edfc8aa4323e108de9aa5e9b
build/out/armbian-b1/idbloader.img 7d799106bd6942bd57e25c56d7a1c89cd354ab836bdeeed3a1ddcee86c09fe4f
```

8 个 FIT image 的声明哈希均与 payload 一致。

## 刷写状态

**本步骤没有对设备做任何写入。**

Linux 对 eMMC 未分区区域的 raw write 仍然报 I/O 错误，直接 `dd` 路径不可行。
可选路径只剩 BootROM 下载模式：

1. 按住 Recovery/音量加开机，或从 Linux 写 BROM flag 后暖重启；
2. USB-C 接到有 `rkdeveloptool` 的主机；
3. 先写回已知良好的基线 `efd764...`，验证读写与启动；
4. 再写 B1 `b1d7ab...`。

需要用户明确授权并提供物理按键/USB 连接条件后才能执行。
