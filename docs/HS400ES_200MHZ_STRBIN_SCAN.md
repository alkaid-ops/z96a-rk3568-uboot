# RK3568 HS400ES 200MHz STRBIN tap 扫描报告

日期：2026-11-14
平台：Sunniwell Z96A / RK3568
模式：HS400 Enhanced Strobe，200MHz，8-bit，1.8V
目的：在 TXCLK 固定 tap 0x8 的前提下，单变量验证 STRBIN tap 4 / 6 / 8。

---

## 0. 结论摘要

```text
STRBIN tap 4 : 通过
STRBIN tap 6 : 通过
STRBIN tap 8 : 通过
```

三轮均为：

```text
dd_rc=0
读回 SHA256 = 测试数据原文
Command Timeout = 0
Data CRC = 0
ADMA Error = 0
DLL LOCKED = 1，TIMEOUT = 0
```

因此在本轮单次 512MiB 写 + 512MiB 原始读条件下，tap 4 / 6 / 8 都在稳定窗口内，未出现失败边界。
本报告不据此认定 tap 4 之外的取值更优；性能差异在单次测量的噪声范围内。

---

## 1. 源码检查与实现方式

### 1.1 STRBIN 初始化位置

`drivers/mmc/host/sdhci-of-dwcmshc.c`，`dwcmshc_rk3568_set_clock()`：

```text
DWCMSHC_EMMC_DLL_STRBIN = 0x80c

默认高速路径：
  extra = DWCMSHC_EMMC_DLL_DLYENA |
          DLL_STRBIN_TAPNUM_DEFAULT(0x4) |
          DLL_STRBIN_TAPNUM_FROM_SW;
  // = 0x09000004
```

写入顺序在 DLL 初始化之后：

```text
1. reset DLL          (DLL_CTRL bit1)
2. 写 RXCLK           (0x804)
3. 写 DLL_CTRL        (0x800 = start point 0x5, inc 0x2, start)
4. 轮询 DLL_STATUS0   (0x840) 直到 LOCKED 且无 TIMEOUT
5. 写 ATCTRL
6. 写 TXCLK           (0x808)
7. 写 STRBIN          (0x80c)
```

因此每次 `unbind/bind` 都会完整重走该初始化流程，STRBIN 会被重新写入。

### 1.2 测试补丁

仅测试用，未进入生产补丁集：

```text
build/out/dwcmshc-diag/scan/test-scan-strbin.patch
sha256 b81f027fe70f689ccb513a11e4bd6ff94e410444215386c21295a039a2177cc5
```

关键改动：

```text
+ static int scan_hs400_strbin = 0x4;
+ module_param_named(scan_hs400_strbin, scan_hs400_strbin, int, 0644);
...
  extra = DWCMSHC_EMMC_DLL_DLYENA |
          DLL_STRBIN_TAPNUM_DEFAULT |
          DLL_STRBIN_TAPNUM_FROM_SW;
+ if (rockchip_pdata->revision == 0 &&
+     host->mmc->ios.timing == MMC_TIMING_MMC_HS400)
+     extra = DWCMSHC_EMMC_DLL_DLYENA |
+             DLL_STRBIN_TAPNUM_FROM_SW |
+             ((u8)scan_hs400_strbin & 0x1f);
  sdhci_writel(host, extra, DWCMSHC_EMMC_DLL_STRBIN);
```

生效范围：

- 仅 `rockchip_pdata->revision == 0`；
- 仅 HS400 / HS400ES（`MMC_TIMING_MMC_HS400`）；
- HS200 路径不变。

### 1.3 扫描内核

```text
build/out/dwcmshc-diag/scan/Image-scan-strbin
sha256 1f99fbd841963b3530a93dbae254dd45045d043e991dfb7ee2f3fca5d1ff7e58
```

测试参数：

```text
/sys/module/sdhci_of_dwcmshc/parameters/scan_hs400_tap    固定 8
/sys/module/sdhci_of_dwcmshc/parameters/scan_hs400_strbin  依次 4 / 6 / 8
```

### 1.4 测试 DTB

```text
build/out/dwcmshc-diag/hs400-es-200mhz.dtb
sha256 ad0858fcaad4c011fa3b7a4102a127efb6ca5f4b3ce3aa58ecba0b9ae68ab7ad
```

仅增加 HS400/HS400ES 属性，`max-frequency = 200MHz`。

---

## 2. 执行方式与 reboot 次数

每个 tap：

```text
1. 写 scan_hs400_tap = 8
2. 写 scan_hs400_strbin = 目标值
3. unbind / bind sdhci-dwcmshc 控制器（完整重新初始化）
4. 读 devmem 0x80c 校验实际值（不符立即停止）
5. 读 ios 确认 200MHz / 8-bit / 1.8V / HS400ES
6. 读 devmem 0x840 确认 DLL LOCKED 且无 TIMEOUT
7. 备份 p2 尾部 512MiB
8. diag=1 / diag_reset=0 / 清零 err_stats
9. 一次 512MiB 写（oflag=direct conv=fsync）
10. 原始读速度测量 + 读回 SHA256
11. 读 err_stats、dmesg、实际寄存器
12. 恢复 scan_hs400_strbin=4、tap=8，unbind/bind
13. 用备份恢复 p2 尾部并核对原始 hash
```

本轮完整 reboot 共 **2 次**：

```text
1. 安装 Image-scan-strbin + 200MHz HS400ES DTB 后 reboot 一次
2. 最终恢复 Image-0031 + HS200 DTB 后 reboot 一次
```

tap 之间 **没有完整 reboot**，只做控制器 unbind/bind。
因此本报告是“控制器重初始化级”扫描，不是冷启动稳定性结论。

---

## 3. 每轮实际寄存器

共同值（三轮一致）：

```text
0xfe310800 (DLL_CTRL)    0x00050201   start point 0x5, inc 0x2, start
0xfe310804 (RXCLK)       0x28000000
0xfe310808 (TXCLK)       0x29000008   tap 8
0xfe310810 (CMDOUT)      0x00000000
0xfe310840 (DLL_STATUS0) 0x00000138   LOCKED=1, TIMEOUT=0
```

| Tap | 实际 `0x80c` | 校验 | DLL STATUS0 | DLL |
|---|---|---|---|---|
| 4 | `0x09000004` | 与期望一致 | `0x00000138` | LOCKED=1 TIMEOUT=0 |
| 6 | `0x09000006` | 与期望一致 | `0x00000138` | LOCKED=1 TIMEOUT=0 |
| 8 | `0x09000008` | 与期望一致 | `0x00000139`（运行时）/ `0x00000138`（寄存器快照） | LOCKED=1 TIMEOUT=0 |

说明：

- 每轮都直接读取 `devmem 0xfe31080c`，不依赖 sysfs 参数；
- `DLL_LOCKED = BIT(8)`，`DLL_TIMEOUT = BIT(9)`；
- tap 8 校验时 `0x840` 曾读到 `0x139`（bit0 额外置位），但 LOCKED=1、TIMEOUT=0，寄存器快照为 `0x138`，不影响结论；
- 未修改 TXCLK、RXCLK、CMDOUT、DLL 参数。

---

## 4. 测试结果

测试数据：

```text
ed92a2039b6b594748b58db561f76d54d0a3c99f21e3cb6d7fd36967fb813e85
```

测试区：eMMC p2 尾部 512MiB（`skip=28795`, `count=512`, 1MiB 块）。
每轮备份与恢复后原始 hash：

```text
64fc0210f826f815e0e4cf6e6798c1cc80d45043df1dccd9c4bb5b30ff3b9718
```

| Tap | 实际 STRBIN | 写入 MB/s | 原始读取 MB/s | 读+SHA MB/s | 读回 SHA256 | CRC | Timeout | ADMA | 结果 |
|---|---|---:|---:|---:|---|---:|---:|---:|---|
| 4 | `0x09000004` | 125 | 227 | 103 | `ed92...` 通过 | 0 | 0 | 0 | 通过 |
| 6 | `0x09000006` | 121 | 225 | 99.4 | `ed92...` 通过 | 0 | 0 | 0 | 通过 |
| 8 | `0x09000008` | 126 | 223 | 101 | `ed92...` 通过 | 0 | 0 | 0 | 通过 |

- 写入为单次 512MiB `dd`，`oflag=direct conv=fsync`；
- “原始读取”为直接读盘测速，不含 SHA256 开销；
- “读+SHA”受 CPU 哈希开销影响，约 99–103 MB/s，不能作为存储读取上限；
- 三轮 `dd_rc` 均为 0；
- 三轮 `err_stats` 的 Command Timeout / Data CRC / ADMA Error 全部为 0；
- 未发生首次错误，因此没有触发“立即停止该 tap”分支。

性能观察：

```text
写入：tap 8 = 126，tap 4 = 125，tap 6 = 121 MB/s
原始读：tap 4 = 227，tap 6 = 225，tap 8 = 223 MB/s
```

差异很小，单次测量不足以判定唯一最优 tap。

---

## 5. 恢复验证（每轮）

每轮结束：

```text
scan_hs400_strbin 恢复 4
scan_hs400_tap     保持 8
unbind/bind
```

恢复后实际寄存器：

```text
0xfe31080c = 0x09000004
0xfe310840 = 0x00000138（LOCKED=1, TIMEOUT=0）
```

恢复后 p2 尾部 hash 三轮均为：

```text
64fc0210f826f815e0e4cf6e6798c1cc80d45043df1dccd9c4bb5b30ff3b9718
```

---

## 6. 最终恢复状态（HS200 原始配置）

最终重新安装：

```text
Image-0031     971d3c292d4e25f0852f2c69099ee560dc2230d7d9ca62376c175037e976db30
usb-hs200.dtb  d025984c947eb46eb9520c0ecd805ff74c8d3eab256573858c07e3a28c235c7a
```

最终设备状态：

```text
root:              /dev/sda2（USB）
内核:              Linux 7.2.12 #1
eMMC:              HS200 @200MHz
eMMC p2:           未挂载
eMMC p2 hash:      64fc0210...（原始）
eMMC p1 DTB:       03b800af...（未修改）
eMMC p1 Image:     c923e5de...（未修改）
USB Image:         971d3c29...（0031）
USB DTB:           d025984c...（HS200）
max-frequency:     0x00c2eb0b（200MHz）
TXCLK:             0x29000010
RXCLK:             0x28000000
STRBIN:            0x09000004
CMDOUT:            0x00000000
DLL_CTRL:          0x00050201
DLL_STATUS0:       0x00000138（LOCKED=1, TIMEOUT=0）
diag:              N
diag_reset:        0
外部模块:          无
debugfs:           已卸载
```

未修改：

- eMMC p1；
- 分区表；
- U-Boot；
- 生产 DTB；
- TXCLK / RXCLK / CMDOUT / DLL 参数；
- eMMC p2 测试区外的任何数据。

---

## 7. 产物

```text
build/out/dwcmshc-diag/scan/
  Image-scan-strbin
  test-scan-strbin.patch
  scan-strbin/tap04/
  scan-strbin/tap06/
  scan-strbin/tap08/
```

每个 tap 目录包含：

```text
start.txt  verify.txt  ios.txt  regs.txt  regs_after.txt
backup-dd.txt  backup.bin
dd.log  read-raw.log  read-speed-hash.log  read-hash.txt
err_stats_pre.txt  err_stats_after.txt
dmesg_before.txt  dmesg_after.txt  errors.txt  diaglog.txt
restore-dd.txt  restore-hash.txt  restore_regs.txt
```

本报告：`docs/HS400ES_200MHZ_STRBIN_SCAN.md`

---

## 8. 边界说明

- 本任务只做测试，测试参数与扫描内核不进入生产驱动。
- 0031（HS400/ES TXCLK tap=0x8）仍未自动提交生产补丁集。
- 本轮 tap 4/6/8 全部通过，没有发现 STRBIN 失败边界；若要确定最优值，需要多轮重复或完整冷启动验证。
- 本轮未扫描 tap 4/6/8 之外的值。
