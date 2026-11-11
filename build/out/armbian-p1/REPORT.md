# Z96A U-Boot P1 构建/刷写报告

日期：2026-11-11
基底：Armbian 2026.01 U-Boot（`kemp233/u-boot-1` @ `8e474aa...`）
补丁：`0005..0014`（0013 = LED DTS，0014 = LED 代码 + 原厂充电器初值）

## 产物

| 文件 | 大小 | SHA-256 |
|---|---|---|
| `idbloader.img` | 204800 | `2e6d0ce2c866dbc126a7b1effbb2e8d7e290f19b16c0ba56c13e2fa0c86356a5` |
| `u-boot.itb` | 1221120 | `3dd682033885ed88c3c95fa02d152747bf577fadfd678372f59304dcc7f215ea` |

对照：

```text
A  idbloader  367d32ac...  204800  ← 现场仍用这个，P1 idbloader 未刷
A  u-boot.itb c24e1804...  1220608 ← P1 前的 u-boot
P1 u-boot.itb 3dd68203...  1221120 ← 当前现场
```

## P1 改动

```text
1. LED DTS（0013）
   - gpio-leds: z96a-led-power (D3), z96a-led-charging (D5), z96a-led-full (D6)
   - 与 Linux/Android 前面板灯一致

2. board.c（0014）
   - 加入 z96a_led_set / z96a_leds_update / z96a_leds_immediate
   - rk_board_late_init 里先点白灯，再按 SC8886 状态更新红/绿
   - 原厂充电器初值：
       0x00 清 watchdog（bits[15:13]）
       0x0c = 0x1200
       0x0e = 0x1e00（AC 在位）/ 0x0a00（无 AC）
     AC 作为 adapter 的代理；没有复刻原厂 USB PHY type 检测
```

P1 的 `u-boot` 镜像：

```text
data-size 949504 -> 949880
hash      aa97e987... -> ead50b11...
```

FIT 中 ATF/OP-TEE 未变；`fdt-1` 加入 LED 节点。

## 重要：P1 idbloader 没有刷

P1 的 `idbloader.img` 变成 `2e6d0ce2...`，原因是 LED 代码/DTS 也会进入 SPL 构建。  
但 P1 是 proper/DTS 改动，当前 A 的 SPL `367d32ac...` 完全可用，P1 的 u-boot 在它上面启动成功。

所以现场保持：

```text
eMMC idbloader = A 367d32ac...   （未改）
eMMC u-boot.itb = P1 3dd68203...
```

不要为了“构建产物一致”去刷 P1 idbloader；没有功能收益。

## 刷写

刷写前备份：

```text
/boot/z96a-uboot/pre-P1-uboot-0x800000.bin  1221120 B  b1a54d2a...
  前 1220608 B = A u-boot c24e1804...
本地副本 build/backup-p1/
```

50 MHz：

```text
echo 50000000 > /sys/kernel/debug/mmc0/clock
```

写入：

```text
u-boot.itb seek=16384 count=2385
```

回读：

```text
3dd682033885ed88c3c95fa02d152747bf577fadfd678372f59304dcc7f215ea
```

idbloader 未写，回读仍为：

```text
367d32ac9b362b0c0be435d81bded6685122461b36d50dc3d9a0edea3ae2f261
```

## 启动验证

```text
boot_id  ca29d51d... -> ae6ca3c4...
uptime   0 min
root     /dev/sda2          U 盘启动正常
eMMC idbloader  367d32ac
eMMC u-boot.itb 3dd68203
```

Linux 起来后读充电器：

```text
0x0c = 0x1500
0x0e = 0x3b00
0x21 = 0x0084   （正在充电）
```

这是 Linux 驱动接管后的值，会覆盖 U-Boot 早期的 `0x1200/0x1e00`；属预期。  
U-Boot 阶段的 `0x0c/0x0e` 无法从 Linux 侧直接证明，只能静态确认代码。

## LED 预期

```text
AC 在位 + 正在充电：白 + 红，绿灭
AC 在位 + 不在充电：白 + 绿，红灭
无 AC：白灭，红/绿灭
```

`led-power` 是 D3，`preboot=gpio set A27` 也会碰 D3；LED 驱动先 claim 后，`gpio set A27` 可能报错但不会阻止启动。  
需要你目视确认红/绿是否按预期亮。

## 回滚 P1

```bash
mount -t debugfs none /sys/kernel/debug
echo 50000000 > /sys/kernel/debug/mmc0/clock

sudo dd if=/boot/z96a-uboot/pre-P1-uboot-0x800000.bin of=/dev/mmcblk0 \
     bs=512 seek=16384 count=2385 oflag=direct conv=fsync

sudo dd if=/dev/mmcblk0 bs=512 skip=16384 count=2384 2>/dev/null | sha256sum
# 期望 c24e1804...（A 的 u-boot.itb）
sync && sudo reboot
```

idbloader 不动，仍是 A 的 `367d32ac`。

## 未验证

```text
LED 红/绿实际亮灭（需要目视）
U-Boot 阶段 0x0c/0x0e 实际写入（需要串口，当前无）
U-Boot 命令行 reset 实机触发（同前）
eMMC 无 U 盘回退物理测试（同前）
```
