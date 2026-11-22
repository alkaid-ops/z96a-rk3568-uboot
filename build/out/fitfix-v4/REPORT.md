# Z96A fitfix-v4 产物报告

日期：2026-11-23（产物实际构建于 2026-11-16 17:50）
基线：U-Boot 2026.01 @ `8e474aa605a86de64316ba6da8dc089402976380`
补丁：`build/edp-logo/phase2/0018-z96a-nov16-bootfb-and-gating.patch`（**单独应用，取代 0016+0017**）
状态：**已在 eMMC 上运行并经回读校验**（2523 扇区完整回读 = 产物哈希）

## 1. 与 fitfix-v3 的差异（逐组件）

| 组件 | fitfix-v3 | fitfix-v4 | 结果 |
|---|---|---|---|
| `atf-1` … `atf-6` | — | — | **逐字节相同** |
| `u-boot.dtb` | `29b2f31a…` | `29b2f31a…` | **完全相同** |
| `u-boot`（nodtb） | 1,015,688 B | 1,018,848 B | **+3,160 B** |
| 版本串 | `U-Boot 2026.01` | `2026.01-00063-g10942055fdd1-dirty` | 板上为更新的分支 |
| `rk3568_vop .flags` | `0` | `0` | 一致（两者交接都不关输出） |

功能性新增（字符串级证据）：

- **`z96a-bootfb`**：把 U-Boot framebuffer 以 `/chosen` 的 `simple-framebuffer` +
  `reserved-memory`（`no-map`）描述写进传给 Linux 的 DTB，并配套 `skip_board_fixup`；
  目的是让 Linux 早期拿到可用的 framebuffer（与 LOGO/启动动画衔接）。
- 分支上的启动门控：U 盘存在时优先从 U 盘启动（而非充电模式）、按 `ON_SOURCE`
  验证后才进入充电模式、早期 LED 反馈。

`bootcmd` 与 `boot_targets` 与 v3 相同：

```
bootcmd=gpio s A6;gpio s E1..;z96a_logo;usb start;bootflow scan -lb
boot_targets=usb mmc1 mmc0 nvme scsi pxe dhcp spi
```

## 2. 产物

| 文件 | 大小 | SHA-256 | 是否入 Git |
|---|---|---|---|
| `u-boot.itb` | 1,291,776（2523 扇区） | `4a2d6982d6846300de224d6178736b3a29d7a07512dc62eff12457bcde923cde` | 否（Release 资产） |
| `u-boot-nodtb.bin` | 1,018,848 | `0d820a65e27c28e5aaf7bbb8b3484ec1b46264ef41252a741c492822690ca956` | 否 |
| `u-boot.dtb` | 76,832 | `29b2f31abe72ca23d6264241f17ce492dfb58183e9c193ef3d794c17b2031ab0` | 否 |
| `z96a-uboot.its` | 2,648 | `fb581f702f1200d31d384ae7bc4370b95606285ce8d3c46574b1dfa14c5d2f16` | 是 |

`idbloader` 不变：`35c368f3ad8d84acd756e4bb45bcbff56912b9b4a8892ac605ff35809bf683ac`
LOGO 资产不变：`z96a-logo.bin.gz` = `ec7e6366…`

## 3. eMMC 校验

```
# 完整 2523 扇区（不是 2516，v3 的尺寸只够 v3）
dd if=/dev/mmcblk0 bs=512 skip=16384 count=2523 | sha256sum
4a2d6982d6846300de224d6178736b3a29d7a07512dc62eff12457bcde923cde   ← 与产物一致
```

写入范围 `16384..18906`，仍小于 p1 起始 `32768`，不碰 GPT / idbloader / 分区表。

> 注意：fitfix-v3 的 `u-boot.itb` 恰为 2516 扇区，v4 为 2523 扇区。
> 用 `count=2516` 回读 v4 会得到截断前缀 `94d850a3…`，看起来"不一致"，实际是少读了 7 个扇区。

## 4. 复现构建

```sh
git clone https://github.com/kemp233/u-boot-1.git u-boot
cd u-boot && git checkout 8e474aa605a86de64316ba6da8dc089402976380
git apply /path/to/build/edp-logo/phase2/0018-z96a-nov16-bootfb-and-gating.patch

make z96a-rk3568-laptop-v2_defconfig ARCH=arm CROSS_COMPILE=aarch64-linux-gnu-
make -j$(nproc) ARCH=arm CROSS_COMPILE=aarch64-linux-gnu- u-boot-nodtb.bin u-boot.dtb
mkimage -f z96a-uboot.its u-boot.itb
```

已验证：

- 补丁应用后，源码树与原始构建树**逐文件一致**（`diff -ruN` 为空）
- 用 Debian 交叉工具链 `aarch64-linux-gnu-gcc 14.2.0-19` 重编，产物**大小、版本串、
  控制 DTB 哈希均一致**；nodtb 二进制只有构建时间/路径差异（字符串集仅差 2 行：
  编译器名、构建时间）
- 因此 `u-boot.itb` 的哈希与构建目录/时间有关，**发布以本 Release 的产物为准**，
  复现验证用 `SHA256SUMS` 中的 nodtb/dtb 哈希
