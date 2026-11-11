# P0 刷写结果（2026-11-11）

设备：`z96a-devuan` / 192.168.31.84
写入目标：eMMC `/dev/mmcblk0`，仅 `uboot` 区（8 MiB 起，1220096 字节）

## 刷写前

```text
当前 eMMC u-boot 区  4d409961e7145b445e140f763c8bbbfa21bdea9897e62e034e4599634599eb55  (B2)
备份                 /boot/z96a-uboot/emmc-uboot-B2-before-p0.bin   (hash 同上)
备份副本             build/backup-p0/emmc-uboot-B2-before-p0.bin     (hash 同上)
eMMC 时钟            50 MHz（debugfs /sys/kernel/debug/mmc0/clock）
```

## 刷写

```text
dd if=/home/user/u-boot.itb of=/dev/mmcblk0 bs=512 seek=16384 count=2383 \
   oflag=direct conv=fsync
→ 1220096 字节，0.238 s
```

回读：

```text
dd if=/dev/mmcblk0 bs=512 skip=16384 count=2383 | sha256sum
→ 2e252a339c97306610a1a69741811a854e0d72fabfb5f367956dddc4ef8c49d8   (与产物一致)
```

## idbloader 未写（有意）

现场 SPL 与构建产物 `idbloader.img`（`7d799106...`）只差 70 字节：

```text
0x0000e8..0x00000107  32 字节   idblock 容器哈希 1
0x00000600..0x0000061f  32 字节   idblock 容器哈希 2
0x002c054..0x002c05b    6 字节   SPL 构建时间戳  "16"->"20", "51"->"23", "48"->"25"
```

即现场 SPL 是同一份代码、不同构建时间。P0 不涉及 SPL，写它只会替换哈希和时间戳，
因此**保留现场 SPL**，把风险降到最低。

## 刷写后验证

```text
boot_id   2b2b2f0d-dc9c-4f0b-a895-9f622829a805  ->  6556c0b7-be86-46c1-81c1-5b20f345a7d4
uptime    0 min
root      /dev/sda2          ← U 盘启动正常
eMMC u-boot 区  2e252a339c97306610a1a69741811a854e0d72fabfb5f367956dddc4ef8c49d8
```

结论：

```text
U 盘启动       通过
Linux reboot   通过（真重启，boot_id 变化，自动回连）
eMMC 镜像      与 P0 产物一致
```

充电寄存器（Linux 起来后由内核重新配置）：

```text
0x02 = 0x03c0   0x20 = 0x8400 (AC_STAT=1)   0x21 = 0x0084
```

U-Boot 阶段的 1024 mA 属于早期状态，Linux 接管后按内核策略写回，属预期。

## 回滚

```bash
mount -t debugfs none /sys/kernel/debug
echo 50000000 > /sys/kernel/debug/mmc0/clock
sudo dd if=/boot/z96a-uboot/emmc-uboot-B2-before-p0.bin of=/dev/mmcblk0 \
     bs=512 seek=16384 count=2383 oflag=direct conv=fsync
sudo dd if=/dev/mmcblk0 bs=512 skip=16384 count=2383 2>/dev/null | sha256sum
# 期望 4d409961e7145b445e140f763c8bbbfa21bdea9897e62e034e4599634599eb55
sync && sudo reboot
```

## 待验证

```text
eMMC 回退：需要物理拔掉 U 盘后重启，确认从 eMMC p1 启动
（不做这一步，无法证明无 U 盘时的回退路径）
```
