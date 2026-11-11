# Z96A U-Boot A 方案（P0-2 A）构建/刷写报告

日期：2026-11-11
基底：Armbian 2026.01 U-Boot（`kemp233/u-boot-1` @ `8e474aa...`）
补丁：`0005..0012`（0011 为 P0 充电电流，0012 为 `CONFIG_SYSRESET_PSCI=y`）

## 产物

| 文件 | 大小 | SHA-256 |
|---|---|---|
| `idbloader.img` | 204800 | `367d32ac9b362b0c0be435d81bded6685122461b36d50dc3d9a0edea3ae2f261` |
| `u-boot.itb` | 1220608 | `c24e18048faf2fe9382868a26e63e67b8f9d6039cfc11bc2d6a589a571bd1f1c` |

对照：

```text
P0  idbloader.img  202752  7d799106...   （未刷，现场原 SPL 为 3a89818e...）
P0  u-boot.itb     1220096 2e252a33...
A   idbloader.img  204800  367d32ac...   （+2 KiB，SPL 含 PSCI FW 代码）
A   u-boot.itb     1220608 c24e1804...   （+512 B，proper 含 psci-sysreset）
```

## 改动

```text
1. board.c：SC8886 充电电流 0x14 -> 0x02（P0，已在 A 中保留）
2. defconfig：CONFIG_SYSRESET_PSCI=y
```

A 的 `u-boot` 镜像：

```text
data-size 949008 -> 949504   (+496 B)
hash      b7e5e62a... -> aa97e987...
```

FIT 中 ATF/OP-TEE/fdt 未变；仅 `u-boot` 镜像变化。

## SPL 变化（A 的代价）

```text
idbloader 202752 -> 204800  (+2 KiB)
SPL 中包含 SPL_ARM_PSCI_FW（由 SYSRESET_PSCI 的 Kconfig select 引入）
SPL 运行在 ATF 之前，这段 PSCI 代码在 SPL 阶段没有实际用途
```

这是 A 方案明确接受的代价。

## 刷写

刷写前备份：

```text
/boot/z96a-uboot/pre-A-idb-0x8000-0xD2000.bin   204800 B  cbfacae3...
  其中前 202752 B = 现场原 SPL 3a89818e...
/boot/z96a-uboot/pre-A-uboot-0x800000.bin       1220608 B 574403c3...
  其中前 1220096 B = P0 u-boot.itb 2e252a33...
本地副本 build/backup-a/
```

50 MHz：

```text
echo 50000000 > /sys/kernel/debug/mmc0/clock
```

写入：

```text
idbloader  seek=64    count=400
u-boot.itb seek=16384 count=2384
```

回读：

```text
idbloader  367d32ac...  （与产物一致）
u-boot.itb c24e1804...  （与产物一致）
```

## 启动验证

```text
boot_id  6556c0b7... -> ca29d51d...
uptime   0 min
root     /dev/sda2        ← U 盘启动正常
eMMC idbloader  367d32ac...
eMMC u-boot.itb c24e1804...
```

## 未验证

```text
U-Boot 命令行 reset 没有实机触发：无串口，不能在正常启动链里安全触发。
静态确认：CONFIG_SYSRESET_PSCI=y、u-boot.itb 含 "psci-sysreset"、
          psci_bind 会为该 PSCI 节点绑定驱动。
Linux reboot 已再次验证：boot_id 变化，链路正常（Linux 不经过 U-Boot reset）。
eMMC 无 U 盘回退仍未做物理拔盘测试。
```

## 回滚

```bash
mount -t debugfs none /sys/kernel/debug
echo 50000000 > /sys/kernel/debug/mmc0/clock

sudo dd if=/boot/z96a-uboot/pre-A-idb-0x8000-0xD2000.bin of=/dev/mmcblk0 \
     bs=512 seek=64 count=400 oflag=direct conv=fsync
sudo dd if=/boot/z96a-uboot/pre-A-uboot-0x800000.bin of=/dev/mmcblk0 \
     bs=512 seek=16384 count=2384 oflag=direct conv=fsync

# 校验前段
sudo dd if=/dev/mmcblk0 bs=512 skip=64 count=396 2>/dev/null | sha256sum
# 期望 3a89818e...
sudo dd if=/dev/mmcblk0 bs=512 skip=16384 count=2383 2>/dev/null | sha256sum
# 期望 2e252a33...
sync && sudo reboot
```
