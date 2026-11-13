# RK3568 HS400ES 200MHz TXCLK tap 扫描报告（第一轮）

日期：2026-11-14
平台：Sunniwell Z96A / RK3568
模式：HS400 Enhanced Strobe，200MHz，8-bit，1.8V
目的：找出“数据完整性通过、无 CRC/timeout/ADMA、写入吞吐量高”的 TXCLK tap 稳定窗口。

---

## 0. 结论摘要

第一轮扫描结果：

```text
稳定通过：0x04、0x06、0x08、0x0A、0x0C
首次写失败：0x0E、0x10（DATA_CRC）
```

性能：

```text
写入最高：0x08 = 129 MB/s
原始读取最高：0x0A = 227 MB/s
0x08 原始读取 = 225 MB/s
0x0C 原始读取 = 225 MB/s
```

因此第一轮稳定窗口为 **0x04–0x0C**，当前 `0x08` 仍是最稳的候选之一；0x0A/0x0C 性能非常接近。
**尚未进行第二轮细测/多轮稳定性验证，不能仅凭第一轮确定唯一最优 tap。**

---

## 1. TXCLK tap 字段与写入路径核验

Linux 7.2.12：

```text
drivers/mmc/host/sdhci-of-dwcmshc.c
  txclk_tapnum 为驱动内 u8
  DWCMSHC_EMMC_DLL_TXCLK (0x808)
    bit[7:0]  tap 值
    bit24     FROM_SW
    bit27     DLYENA
    bit29     TX source-select
```

写入：

```text
extra = DWCMSHC_EMMC_DLL_DLYENA |
        DLL_TXCLK_TAPNUM_FROM_SW |
        DLL_RXCLK_NO_INVERTER << 29 |
        txclk_tapnum;
sdhci_writel(host, extra, DWCMSHC_EMMC_DLL_TXCLK);
```

- 软件侧值是 8-bit；第一轮只测试 0x04–0x10，不扩展到 0x00–0x1F。
- 没有证据在本轮确认硬件完整 5-bit 范围；因此没有扫描 0x11 以上。
- RXCLK、STRBIN、CMDOUT、DLL_CTRL 均未改变。

---

## 2. 无重启扫描方法

为了不在每个 tap 都完整重启，使用一个 **仅测试用** 的扫描内核：

```text
build/out/dwcmshc-diag/scan/Image-scan-param
sha256 85d68a2157e816f2410a48d87e15486c64a680b44502049a3676f3aeeac4cbb9
```

测试补丁：

```text
scan/test-scan-param.patch
+ static int scan_hs400_tap = 0x8;
+ module_param_named(scan_hs400_tap, scan_hs400_tap, int, 0644);
...
+ txclk_tapnum = (u8)scan_hs400_tap;  /* revision 0 HS400/ES */
```

每个 tap 的流程：

1. 写 `/sys/module/sdhci_of_dwcmshc/parameters/scan_hs400_tap`；
2. sdhci 控制器 unbind/bind（完整重新初始化，不写数据）；
3. 确认实际 HS400ES 200MHz 和 TXCLK 寄存器；
4. 备份测试区 → 一次 512MiB 写 → 原始读速度 → SHA256；
5. 读取 err_stats / dmesg；
6. 设回 tap=0x8，unbind/bind，恢复测试区并核对 hash；
7. 下一个 tap。

这不是动态改寄存器连续扫值；每个 tap 都经过一次控制器重新初始化。
**限制：除首次安装扫描内核和最终恢复 HS200 外，tap 之间没有完整 reboot。** 因此本报告是“控制器重初始化级”的第一轮扫描，不是冷启动稳定性结论。

测试 DTB：

```text
hs400-es-200mhz.dtb
sha256 ad0858fcaad4c011fa3b7a4102a127efb6ca5f4b3ce3aa58ecba0b9ae68ab7ad
```

---

## 3. 第一轮结果

测试数据：

```text
ed92a2039b6b594748b58db561f76d54d0a3c99f21e3cb6d7fd36967fb813e85
```

测试区：p2 尾部 512MiB；每个 tap 前重新备份，恢复后 hash 均为：

```text
64fc0210f826f815e0e4cf6e6798c1cc80d45043df1dccd9c4bb5b30ff3b9718
```

| Tap | 写入 MB/s | 原始读取 MB/s | 读+SHA MB/s | CRC | Timeout | Hash | 结果 |
|---|---:|---:|---:|---:|---:|---|---|
| 0x04 | 127 | 223 | 102 | 0 | 0 | 通过 | 通过 |
| 0x06 | 126 | 220 | 103 | 0 | 0 | 通过 | 通过 |
| **0x08** | **129** | **225** | **101** | **0** | **0** | **通过** | **通过** |
| 0x0A | 126 | **227** | 102 | 0 | 0 | 通过 | 通过 |
| 0x0C | 126 | 225 | 104 | 0 | 0 | 通过 | 通过 |
| 0x0E | 0 | — | — | **6** | 0 | 原始 hash | **失败：DATA_CRC** |
| 0x10 | 0 | — | — | **6** | 0 | 原始 hash | **失败：DATA_CRC** |

说明：

- 0x04 使用固定 tap=0x04 的独立 Image；
- 0x06/08/0A/0C/0E/10 使用 scan-param 内核 + 控制器重初始化；
- 0x0E 的首条 0030 日志：

```text
first WRITE error int=00208001
tx=2900000e
Data CRC Errors=6
Command Timeout=0
```

- 0x10：

```text
TXCLK=0x29000010
dd rc=1，0 字节
Data CRC Errors=6
Command Timeout=0
```

0x10 的 0030 one-shot 已被 0x0E 消耗，未再次捕获独立 0030 日志；错误类型由 err_stats 和 dd 失败证据确认。
0x0E/0x10 写入失败后，测试区仍恢复为原始 hash。

---

## 4. 性能观察

- 写入：0x08 最高（129 MB/s）；0x04/0x06/0x0A/0x0C 在 126–127 MB/s。
- 原始读取：0x0A 最高（227 MB/s）；0x08 225，0x0C 225，0x04 223，0x06 220。
- 读+SHA 受 SHA256 CPU 开销影响，约 101–104 MB/s，不能直接作为存储读取上限。
- 0x08 与 0x0A/0x0C 差距很小，第一轮单次测量不足以确定唯一最优。

---

## 5. 结论与下一步

### 第一轮结论

- **稳定窗口：0x04–0x0C**。
- 0x0E 起出现 DATA_CRC；0x10 失败。
- 0x08 写入最好，0x0A 原始读取最好；差距在测量噪声范围内。
- 尚未做第二轮细测、第三轮稳定性，也未做 100MHz 独立验证。

### 建议第二轮

只围绕稳定窗口中心：

```text
0x08、0x0A、0x0C
```

每个 tap 至少重复 3 次相同的 512MiB 写/读，记录中位数，并尽量用完整 reboot 或明确区分“控制器重初始化”与“真正冷启动”。
不要同时扫描 0x0E/0x10；它们是失败边界。

### 100MHz

100MHz 作为独立验证对象，不应与 200MHz 结果混在同一表里；后续可只验证第二轮选出的 tap。

### 当前生产状态

- 0031（HS400/ES tap=0x8）未自动提交。
- 扫描补丁 `scan_hs400_tap` 仅测试用，不在生产补丁集。
- 最终设备已恢复 HS200 原始配置（0031 Image + HS200 DTB，TXCLK=0x10）。

---

## 6. 最终恢复状态

```text
root:           /dev/sda2（USB）
eMMC:           HS200 @200MHz
eMMC p2:        未挂载
eMMC p2 hash:   64fc0210...
eMMC p1 DTB:    03b8...（未修改）
eMMC p1 Image:  c923...（未修改）
USB DTB:        d025...（HS200）
USB Image:      971d...（0031 测试内核）
diag:           N
diag_reset:     0
TXCLK:          0x29000010
RXCLK:          0x28000000
STRBIN:         0x09000004
CMDOUT:         0x00000000
外部模块:       无
debugfs:        已卸载
```

---

## 7. 产物

```text
build/out/dwcmshc-diag/scan/
  Image-scan-param
  test-scan-param.patch
  Image-tap-04 ... Image-tap-10
  scan-tap04/
  scan-param/tap06/ tap08/ tap0a/ tap0c/ tap0e/ tap10/
```

本报告：`docs/HS400ES_200MHZ_TXCLK_TAP_SCAN_ROUND1.md`
