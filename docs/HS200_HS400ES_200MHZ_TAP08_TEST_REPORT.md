# RK3568 HS200 / HS400ES 200MHz TXCLK tap=0x8 单次读写验证报告

日期：2026-11-13
平台：Sunniwell Z96A / RK3568
内核：Linux 7.2.12
测试条件：200MHz，8-bit，1.8V
每个模式只执行一次 512MiB 写入 + 一次读回校验。

---

## 0. 结论

| 测试 | 模式 | 频率 | TXCLK tap | 结果 |
|---|---|---:|---:|---|
| T1 | HS200 | 200MHz | 0x8 | 写入成功、读回 hash 一致、err_stats 全 0、无 I/O error |
| T2 | HS400ES | 200MHz | 0x8 | 写入成功、读回 hash 一致、err_stats 全 0、无 I/O error |

记录为：

- **HS200 200MHz / TXCLK tap=0x8：单次读写验证有效。**
- **HS400ES 200MHz / TXCLK tap=0x8：单次读写验证有效。**

上述仅代表功能验证通过，不代表长期稳定性已证明。

---

## 1. T1：HS200 200MHz / tap=0x8

### 1.1 实现差异

0031 只让 HS400/ES 使用 tap=0x8；T1 需要 HS200 也使用 0x8。因此使用一个 **仅测试用** 的附加补丁：

```diff
+	/* T1 test-only: HS200 also uses tap 8 on RK3568 revision 0 */
+	if (rockchip_pdata->revision == 0 &&
+	    host->mmc->ios.timing == MMC_TIMING_MMC_HS200)
+		txclk_tapnum = 0x8;
```

文件：

```text
build/out/dwcmshc-diag/0032-test-hs200-txclk-tap-08.patch
```

T1 Image（仅测试，不是生产补丁）：

```text
build/out/dwcmshc-diag/Image-t1-hs200-tap08
sha256 4f3c0ac08038e29f29bd1dacaf5de6596a2bb116f4cf0fefa785ef7ae07ab042
```

T1 DTB：

```text
usb-hs200.dtb
sha256 d025984c947eb46eb9520c0ecd805ff74c8d3eab256573858c07e3a28c235c7a
```

### 1.2 实际状态

启动时间：

```text
2026-11-13 15:35:29 UTC
```

iOS：

```text
clock:          200000000 Hz
actual clock:   200000000 Hz
bus width:      3 (8 bits)
timing spec:    9 (mmc HS200)
signal voltage: 1 (1.80 V)
driver type:    0 (driver type B)
```

寄存器：

```text
TXCLK  = 0x29000008
RXCLK  = 0x28000000
STRBIN = 0x09000004
CMDOUT = 0x00000000
DLL    = 0x00050201
STAT0  = 0x00000138  (LOCKED=1, TIMEOUT=0)
```

TXCLK 实际为 `0x29000008`，证明 HS200 测试路径确实使用 tap=0x8，而不是只看 HS400 寄存器。

### 1.3 测试区与数据

测试区：p2 尾部 `skip=28795`，`count=512`（512MiB），未挂载。

备份：

```text
t1-hs200-tap08/backup.bin
size 536870912
sha256 64fc0210f826f815e0e4cf6e6798c1cc80d45043df1dccd9c4bb5b30ff3b9718
```

测试数据：

```text
/home/user/hs400-pattern-tap08.bin
sha256 ed92a2039b6b594748b58db561f76d54d0a3c99f21e3cb6d7fd36967fb813e85
```

### 1.4 写入与读回

```text
dd rc=0
输入 512+0，输出 512+0
512MiB，5.15211 s，104 MB/s
```

读回：

```text
ed92a2039b6b594748b58db561f76d54d0a3c99f21e3cb6d7fd36967fb813e85
```

与 pattern 一致。

`err_stats`：

```text
Command Timeout: 0
Data CRC:        0
其他:            0
```

`dmesg`：

- 无 DWCMSHC diag；
- 无 I/O error / CRC / timeout；
- 无 block layer 重试。

测试区恢复：

```text
64fc0210f826f815e0e4cf6e6798c1cc80d45043df1dccd9c4bb5b30ff3b9718
```

T1 通过。

---

## 2. T2：HS400ES 200MHz / tap=0x8

### 2.1 Image / DTB

使用 0031 Image：

```text
build/out/dwcmshc-diag/Image-0031
sha256 971d3c292d4e25f0852f2c69099ee560dc2230d7d9ca62376c175037e976db30
```

使用临时 HS400ES 200MHz DTB（仅本次测试）：

```text
build/out/dwcmshc-diag/hs400-es-200mhz.dtb
sha256 ad0858fcaad4c011fa3b7a4102a127efb6ca5f4b3ce3aa58ecba0b9ae68ab7ad
```

DTB 只在 USB 基线的 `mmc@fe310000` 增加：

```text
mmc-hs400-1_8v;
mmc-hs400-enhanced-strobe;
```

`max-frequency` 仍为 200MHz。

### 2.2 实际状态

启动时间：

```text
2026-11-13 15:39:03 UTC
```

iOS：

```text
clock:          200000000 Hz
actual clock:   200000000 Hz
bus width:      3 (8 bits)
timing spec:    10 (mmc HS400 enhanced strobe)
signal voltage: 1 (1.80 V)
driver type:    0 (driver type B)
```

寄存器：

```text
TXCLK  = 0x29000008
RXCLK  = 0x28000000
STRBIN = 0x09000004
CMDOUT = 0x00000000
DLL    = 0x00050201
STAT0  = 0x00000138  (写入后 0x137，LOCKED=1, TIMEOUT=0)
```

### 2.3 测试区与数据

备份：

```text
t2-hs400es-200/backup.bin
size 536870912
sha256 64fc0210f826f815e0e4cf6e6798c1cc80d45043df1dccd9c4bb5b30ff3b9718
```

测试数据：

```text
ed92a2039b6b594748b58db561f76d54d0a3c99f21e3cb6d7fd36967fb813e85
```

### 2.4 写入与读回

```text
dd rc=0
输入 512+0，输出 512+0
512MiB，4.16668 s，129 MB/s
```

读回：

```text
ed92a2039b6b594748b58db561f76d54d0a3c99f21e3cb6d7fd36967fb813e85
```

与 pattern 一致。

`err_stats`：

```text
Command Timeout: 0
Data CRC:        0
其他:            0
```

`dmesg`：

- 无 DWCMSHC diag；
- 无 I/O error / CRC / timeout；
- 无 block layer 重试。

测试区恢复：

```text
64fc0210f826f815e0e4cf6e6798c1cc80d45043df1dccd9c4bb5b30ff3b9718
```

T2 通过。

---

## 3. 对照汇总

| 项目 | T1 HS200 | T2 HS400ES |
|---|---|---|
| 频率 | 200MHz | 200MHz |
| 时序 | HS200 | HS400 enhanced strobe |
| bus/voltage | 8-bit / 1.8V | 8-bit / 1.8V |
| TXCLK | `0x29000008` | `0x29000008` |
| RXCLK | `0x28000000` | `0x28000000` |
| STRBIN | `0x09000004` | `0x09000004` |
| CMDOUT | 0 | 0 |
| DLL | `0x00050201` | `0x00050201` |
| write | 104 MB/s | 129 MB/s |
| read | 64.5 MB/s | 106 MB/s |
| hash | pattern | pattern |
| err_stats | 全 0 | 全 0 |
| I/O error/retry | 无 | 无 |
| 测试区恢复 | `64fc...` | `64fc...` |
| 结果 | 单次有效 | 单次有效 |

---

## 4. 最终恢复

已恢复：

```text
Image      971d3c29...（0031 生产验证内核，HS200 仍为 0x10）
USB DTB    d025984c...（HS200 原始 USB DTB）
eMMC p1    03b8... / c923...（未修改）
eMMC p2    64fc0210...（原始测试区）
```

最终 HS200 状态：

```text
clock 200MHz / 8-bit / 1.8V
timing spec 9 (HS200)
TXCLK  = 0x29000010
RXCLK  = 0x28000000
STRBIN = 0x09000004
CMDOUT = 0x00000000
diag = N
diag_reset = 0
err_stats = 全 0
外部模块 = 无
debugfs = 已卸载
```

没有修改 U-Boot、生产 DTB、eMMC p1 或分区表。

---

## 5. 0031 与 T1 测试补丁的关系

- T2 使用 0031：`hs400_txclk_tapnum=0x8` 应用于 HS400/HS400ES；HS200 仍为 0x10。
- T1 额外使用 **仅测试用** 的 HS200 override，不是 0031 生产逻辑。
- T1 的测试补丁/Image 不应进入生产补丁集。
- 0031 仍保持原先范围：只影响 RK3568 revision 0 的 HS400/HS400ES；未自动提交。

---

## 6. 产物

```text
build/out/dwcmshc-diag/
  0032-test-hs200-txclk-tap-08.patch
  Image-t1-hs200-tap08
  hs400-es-200mhz.dtb / .dts / .roundtrip.dts
  tap08/
    t1-hs200-tap08/
      start.txt, dtb.txt, ios.txt, regs.txt, preflight.txt,
      backup.bin, backup-dd.txt, err_stats_pre.txt, err_stats_after.txt,
      dmesg_before.txt, dmesg_after.txt, dd.log, read.log,
      read-hash.txt, regs_after.txt, diaglog.txt, errors.txt, restore.txt
    t2-hs400es-200/
      同上
```

本报告：`docs/HS200_HS400ES_200MHZ_TAP08_TEST_REPORT.md`

---

## 7. 结论限定

- T1/T2 各自只执行一次写入和一次读回；
- “通过”仅表示该模式、该频率、该单次操作有效；
- 不代表长期稳定性、温度/电压裕量、其他板卡/其他 eMMC 或其他频率；
- 不扫描其他 tap，不改变其他 PHY 参数。
