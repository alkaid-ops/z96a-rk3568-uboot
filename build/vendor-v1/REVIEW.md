# Z96A vendor U-Boot V1 — 构建与核对（未刷写）

状态：**仅构建，未写 eMMC**。所有产物在 `build/vendor-v1/`。

## 产物

| 文件 | 大小 | SHA-256 |
|---|---|---|
| `u-boot-patched.bin` | 1246064 | `3155f16007af31e3ff7a289be4ec31146f7bb9b630efdb72dccf4f3bcaea6340` |
| `vendor-idbloader.bin` | 827392 | `dc44ccffee1d1fa055a37e65949725f853b306f4cca04e479d902b50e8d2ba2a` |
| `vendor-p2-uboot.bin` | 4194304 | `243b5c4f8b086ee09e5c955feca82b25bc6305096f1fb04a766220304f1d30c4` |

来源（只读输入）：

```text
uboot-2017.bin        7b1438d3b4253604cb36e04a54bbd5a53024ecb3358f07a0d03224ca9491cfb7
factory-first16m.bin  f761cac65d410a629dc3b6610e4791c6b2a3aa86a6ce3d7fdae6c3f8a76c87e1
```

## 改了什么

1. **默认环境变量**（payload 偏移 `0xc0ed0`，长度 `0xebc`）

   ```text
   bootcmd:
     - sysboot mmc 0:9 any 0x00c00000 /extlinux/extlinux.conf
     + setenv devnum 0; usb start; if usb dev 0; then setenv devtype usb; run scan_dev_for_boot_part; fi; run bootcmd_mmc0; sysboot mmc 0:1 any 0x00c00000 /extlinux/extlinux.conf

   stdin/stdout/stderr:  serial,usbkbd / serial,vidconsole / serial,vidconsole
                     ->  serial
   partitions:         移除（762 字节，只服务 gpt/part 写分区，不参与启动）
   ```

2. **冷插电 preboot 字符串**（payload 偏移 `0xf6019`）

   ```text
   setenv preboot; charge   ->   setenv preboot;
   ```

   `setup_boot_mode`（`0xa03064`）在 boot mode 4 时会把 `preboot` 设成这个字符串。
   改掉后，冷插电不再进入 `charge`，直接走 `bootcmd`。

3. **board_late_init 显示调用**（`0xa03e14`）

   ```text
   0xa03f58: bl 0xa7c5d4   ->  nop      charge animation / video
   0xa03f5c: bl 0xa52710   ->  nop      logo / DRM / VOP2 / eDP
   ```

4. **FIT A / FIT B**（`0x800000`、`0xa00000`）

   - `/images/uboot/hash/value` 更新为新 payload 的 SHA-256
   - `/images/uboot/data-position 0x1000` 数据区替换为新 payload
   - ATF / OP-TEE / fdt 的 hash 和数据**未改**

## 为什么这样不会卡住 eMMC

启动命令用 `;` 分隔，不是 `&&`，所以每一步失败都继续：

```text
setenv devnum 0;
usb start;                    // 失败 -> 继续
if usb dev 0; then            // 无 U 盘 -> 跳过
    setenv devtype usb; run scan_dev_for_boot_part;
fi;
run bootcmd_mmc0;             // eMMC p1（bootable 扫描，失败则回退 part 1）
sysboot mmc 0:1 any 0x00c00000 /extlinux/extlinux.conf   // eMMC p1 直读
```

代码层面的有界性（静态证据）：

- USB host reset：`0xa920b0`，`0x1e`/`0x32` 次循环，带 `udelay`，超时打印 `ERROR: USB HC reset timed out!`
- DWC3 reset：`0xa862d0`，`5000` 次轮询 + `0x65` 次 `udelay(1000)`，超时打印 `Reset Timed Out`
- fastboot gadget 的 `while(true)` 只在 `0xa13dd4`（fastboot / recovery 模式），正常启动不可达
- charge 的无限循环 `0xa7cefc` 只由 `preboot=charge` 进入，已被补丁 2 移除

显示不会在启动链上初始化：

- `stdout=serial` → `console_init_r`（`0xa256b8`）只按环境变量名查找设备
  （`0xa254dc` → `0xa2db6c`），不再查找 `vidconsole`，因此不会 probe 视频设备
- `stdin=serial` → 不再查找 `usbkbd`，避免键盘驱动 probe USB
- `board_late_init` 的两条显示 `bl` 已改成 `nop`

eMMC 回退已被只读核对：

```text
mmcblk0p1  vfat armbi_boot  UUID 060F-2C80
  /extlinux/extlinux.conf  ->  root=UUID=c953e502-035b-4727-a2f3-634cf778e167
  /Image 36633088 B, /uInitrd 17534396 B, /dtb/rockchip/rk3568-sunniwell-z96a-typec.dtb 存在
mmcblk0p2  ext4 armbi_root  UUID c953e502-035b-4727-a2f3-634cf778e167   ← 与 extlinux 一致
```

## 未改动

```text
GPT / 分区表          未改
p1 文件系统内容       未改
p3 区域               未改
ATF / OP-TEE / fdt    未改（FIT 内 hash 与数据逐字节保持）
0x380000 起的 DDR 区  未写（沿用设备现有内容）
RK809 / PMIC 寄存器   未改（V1 不动 0xf4）
```

## 核对命令（离线，可直接跑）

```bash
cd /home/hsy/project/rea-z97a/build/vendor-v1

# 1. 哈希
sha256sum -c SHA256SUMS

# 2. 环境变量差异
cat env-diff.txt

# 3. 反汇编确认两条显示调用已变成 nop
cat disasm-patched.txt

# 4. 全量离线校验（环境、补丁点、FIT hash、FIT payload、非目标字节未变）
python3 verify_vendor_uboot.py

# 5. FIT 结构
dumpimage -l /tmp/opencode/verify-fit-A.bin
dumpimage -l /tmp/opencode/verify-fit-B.bin
```

`verify_vendor_uboot.py` 会检查：

- payload 只有 env / charge 字符串 / 两处 NOP 共存 135 段差异
- FIT 只改了 uboot hash 与 uboot 数据区
- ATF hash 仍为 `b5946ac6…`
- FIT A/B 头部一致
- idbloader 与 factory 字节一致

## 刷写计划（未执行，需单独授权）

刷写前必须满足：

1. `/boot`（U 盘）与 eMMC p1 都能放备份（约 5 MB）
2. eMMC 写入前把 `mmc0` 降到 50 MHz：

```bash
sudo mount -t debugfs none /sys/kernel/debug
echo 50000000 > /sys/kernel/debug/mmc0/clock
cat /sys/kernel/debug/mmc0/clock
```

备份：

```bash
sudo mkdir -p /boot/z96a-uboot
sudo dd if=/dev/mmcblk0 of=/boot/z96a-uboot/pre-vendor-idb-0x8000-0xd2000.bin \
    bs=512 skip=64 count=1616
sudo dd if=/dev/mmcblk0 of=/boot/z96a-uboot/pre-vendor-p2-0x800000-0xc00000.bin \
    bs=512 skip=16384 count=8192
```

写入：

```bash
sudo dd if=/home/hsy/…/build/vendor-v1/vendor-idbloader.bin of=/dev/mmcblk0 \
    bs=512 seek=64 count=1616 oflag=direct conv=fsync
sudo dd if=/home/hsy/…/build/vendor-v1/vendor-p2-uboot.bin of=/dev/mmcblk0 \
    bs=512 seek=16384 count=8192 oflag=direct conv=fsync
```

回读比对：

```bash
sudo dd if=/dev/mmcblk0 bs=512 skip=64 count=1616 2>/dev/null | sha256sum
# 期望 dc44ccffee1d1fa055a37e65949725f853b306f4cca04e479d902b50e8d2ba2a
sudo dd if=/dev/mmcblk0 bs=512 skip=16384 count=8192 2>/dev/null | sha256sum
# 期望 243b5c4f8b086ee09e5c955feca82b25bc6305096f1fb04a766220304f1d30c4
```

回滚：

```bash
sudo dd if=/boot/z96a-uboot/pre-vendor-idb-0x8000-0xd2000.bin of=/dev/mmcblk0 \
    bs=512 seek=64 count=1616 oflag=direct conv=fsync
sudo dd if=/boot/z96a-uboot/pre-vendor-p2-0x800000-0xc00000.bin of=/dev/mmcblk0 \
    bs=512 seek=16384 count=8192 oflag=direct conv=fsync
```

## 仍然未知（必须实机验证）

- 原厂 U-Boot 的 USB host 是否真的枚举当前 U 盘（VBUS / Type-C 方向）
- 原厂 SPL 是否校验 FIT 的 `uboot` hash（我们已同步更新，但未在硬件上观测）
- 原厂 U-Boot 读取 eMMC p1 的 36 MB `Image` 是否会触发读错误
- `sysboot`（2017 版）对 U 盘 extlinux 里 `fdtdir` 的处理（U 盘 conf 同时给了显式 `fdt`）
- 冷插电无 U 盘时不再充电 → 电池低电场景的行为
