# RK3568 eMMC HS400 ES @100MHz 首次写入诊断报告

日期：2026-11-13  
平台：Sunniwell Z96A / RK3568  
内核：`7.2.12`，`CONFIG_MMC_SDHCI_OF_DWCMSHC=y`  
诊断补丁：`0030`（生产集成，commit `777916b`）  
本轮范围：只执行一次 HS400 ES @100MHz 写入；不做 TXCLK tap 扫描，不调整 PHY，不修改 U-Boot/分区表。

---

## 1. 实验前环境及设备状态

```text
uname: Linux z96a-devuan 7.2.12 #1 SMP PREEMPT Fri Nov 13 21:30:56 CST 2026 aarch64
root:  /dev/sda2（USB）
eMMC:  /dev/mmcblk0，HS200 @200MHz，8-bit，1.8V，driver type B
eMMC p2: 未挂载
```

实验前只读检查结果：

| 项目 | 值 |
|---|---|
| USB `/boot/Image` | `6e971b5d7dcf1183006f4035a4f2dfdbd4df0e6c559c37a5c9b87874aec491cf` |
| USB `/boot/rockchip/...dtb` | `d025984c947eb46eb9520c0ecd805ff74c8d3eab256573858c07e3a28c235c7a` |
| eMMC p1 DTB | `03b800af7e2fdad781f9075fe1bc64e5330c37e146be0625ad95428c2b7e31b7` |
| eMMC p1 Image | `c923e5de2498acceaf99f96ee4df496bbf9b4305b009fb3a13504a170cf45f9b` |
| p2 总扇区 | `60022751` |
| p2 起始扇区 | `1048576` |
| p2 未挂载 | `fuser -vm /dev/mmcblk0p2` 无输出 |
| 驱动参数 | `diag=N`，`diag_reset=0` |
| 外部模块 | 无 `sdhci_of_dwcmshc` 外部 `.ko` |

### 1.1 启动 DTB 路径修正

只读检查发现当前运行中的 DTB **不是** eMMC p1 的 `03b800af...`，而是 USB
`/boot/rockchip/...dtb` 的 `d025984c...` 加上 U-Boot 的 memory/initrd/bootargs 修正。

证据：

- `/sys/firmware/fdt` 的 base 与 USB DTB 除 U-Boot fixup 外一致；
- eMMC p1 DTB 缺少当前 CPU idle/OPP/VOP 等配置，属于旧版；
- 因此按用户确认，临时 HS400 DTB 放入 **USB** 实际启动路径，eMMC p1 保持不变。

本轮没有修改 eMMC p1 DTB/Image。

---

## 2. 测试区与备份/恢复

测试区沿用既有定义：p2 尾部 `skip=28795`、`count=512`（MiB）。

```text
p2_start_sector        = 1048576
test_start_rel_sector  = 58972160
test_length_sectors    = 1048576  (512MiB)
test_end_rel_exclusive = 60020736
test_start_abs_sector  = 60020736  (= p2_start + 58972160)
```

说明：p2 总大小不是整数 MiB，测试区之后还有 `2015` sectors（约 `0.984 MiB`）
未纳入写入范围；写入严格限制在 `58972160..60020735`（相对 p2）。

备份：

```text
file: /home/user/emmc-testarea-backup.bin
size: 536870912
sha256: 64fc0210f826f815e0e4cf6e6798c1cc80d45043df1dccd9c4bb5b30ff3b9718
```

恢复后：

```text
sha256: 64fc0210f826f815e0e4cf6e6798c1cc80d45043df1dccd9c4bb5b30ff3b9718
```

备份文件可读、大小与 512MiB 一致；USB 剩余空间充足（47G）。

---

## 3. 实际生效的 DTB 与 HS400 ES 状态

### 3.1 临时 DTB

基于实际启动的 USB DTB `d025984c...`，只修改了 `mmc@fe310000` 节点：

```diff
- max-frequency = <0xbebc200>;   /* 200MHz */
+ max-frequency = <0x5f5e100>;   /* 100MHz */
  non-removable;
+ mmc-hs400-1_8v;
+ mmc-hs400-enhanced-strobe;
```

未修改其他节点。DTB diff 仅上述 3 项（`hs400-es-100mhz.roundtrip.dts` 对 `usb-hs200.dts`）。

```text
temp DTB: /home/user/hs400-es-100mhz.dtb
sha256: 774343c45ffbf33c026266a1f5c520efe559d0ada260a63f1cbc1e1bb43ac036
```

### 3.2 重启后证据

`/proc/device-tree/mmc@fe310000`：

```text
max-frequency present     原始字节 05 f5 e1 00 = 100000000
mmc-hs400-1_8v present
mmc-hs400-enhanced-strobe present
bus-width present
non-removable present
```

`dmesg`：

```text
[    7.045076] mmc0: new HS400 Enhanced strobe MMC card at address 0001
```

`/sys/kernel/debug/mmc0/ios`：

```text
clock:          100000000 Hz
actual clock:   100000000 Hz
bus width:      3 (8 bits)
timing spec:    10 (mmc HS400 enhanced strobe)
signal voltage: 1 (1.80 V)
driver type:    0 (driver type B)
```

寄存器证据：

- `hctl2=0x000f`：UHS mode `0x7` + 1.8V；
- `emmc_ctrl=0x0000010d`：`CARD_IS_EMMC`（bit0）与 `ENHANCED_STROBE`（bit8）置位；
- `hctl=0x35`：8-bit + high-speed。

为创建 debugfs `mmc0` 节点，在启用 `diag` 前对控制器做过一次
unbind/bind；此过程没有发起数据写，重绑后仍为 HS400 ES @100MHz。
写测试前 `diag=1`、`diag_reset=0`、`err_stats` 全 0、one-shot 未触发。

---

## 4. 唯一一次受控写入

命令：

```sh
dd if=/home/user/hs400-pattern.bin of=/dev/mmcblk0p2 \
  bs=1M count=512 seek=28795 oflag=direct conv=fsync
```

测试数据：

```text
file: /home/user/hs400-pattern.bin
size: 536870912
sha256: f8e74374186b18f61ee876d9243ef9db9efe08b2f53c42ff29423cb2fe7f33e2
```

写入结果：

```text
dd_rc=1
dd: 写入 '/dev/mmcblk0p2' 时出错: 输入/输出错误
输入了 1+0 块记录
输出了 0+0 块记录
0 字节已复制，0.0861201 s，0.0 kB/s
```

只执行了这一次写入；失败后没有重试写入。

---

## 5. 首次错误完整原始日志

```text
[  115.260515] sdhci-dwcmshc fe310000.mmc: DWCMSHC diag: first WRITE error int=00208001 status=00208001 ier=03ff000b int_en=03ff000b sig=03ff000b
[  115.260551] sdhci-dwcmshc fe310000.mmc: DWCMSHC diag: opcode=25 cmd_flags=000000b5 data_flags=100 blocks=1024 blksz=512 xfer=0
[  115.260562] sdhci-dwcmshc fe310000.mmc: DWCMSHC diag: present=03f700f0 hctl=35 hctl2=000f clk=0007 reset=00
[  115.260571] sdhci-dwcmshc fe310000.mmc: DWCMSHC diag: adma_err=00000030 adma_addr=12836208/00000000 auto_cmd=0000 xfer=002b cmd=193a
[  115.260581] sdhci-dwcmshc fe310000.mmc: DWCMSHC diag: emmc_ctrl=0000010d host3=000f0010 atctrl=001f0000
[  115.260589] sdhci-dwcmshc fe310000.mmc: DWCMSHC diag: dll ctrl=00050201 rx=28000000 tx=29000010 strbin=09000004 cmdout=00000000 stat0=00000171
[  115.332683] I/O error, dev mmcblk0, sector 60020736 op 0x1:(WRITE) flags 0xc800 phys_seg 128 prio class 2
[  115.332877] I/O error, dev mmcblk0, sector 60021760 op 0x1:(WRITE) flags 0x8800 phys_seg 128 prio class 2
```

`dmesg` 前后 diff 只增加了上述 8 行（另有 elogind 会话日志），没有其他 MMC/block 错误。

---

## 6. `err_stats` 前后对比

写入前：

```text
所有计数器 = 0
```

写入后：

```text
Command Timeout Occurred:       6
Command CRC Errors Occurred:    0
Data Timeout Occurred:          0
Data CRC Errors Occurred:       6
Auto-Cmd Error Occurred:        0
ADMA Error Occurred:            0
Tuning Error Occurred:          0
CMDQ RED/GCE/ICCE:              0
Request Timedout:               0
CMDQ Request Timedout:          0
ICE Config Errors:              0
Controller Timedout errors:     0
Unexpected IRQ errors:          0
```

完整文件：`hs400/err_stats-after-hs400.txt`。

---

## 7. DLL / TXCLK / RXCLK / STRBIN 快照

```text
dll ctrl = 0x00050201
dll rx   = 0x28000000
dll tx   = 0x29000010
dll strbin = 0x09000004
dll cmdout = 0x00000000
dll stat0  = 0x00000171
```

解读：

- `stat0` bit8 `DWCMSHC_EMMC_DLL_LOCKED` = 1，bit9 `DLL_TIMEOUT` = 0；
  首次 DATA_CRC 时 DLL 报告锁定，未见 DLL lock timeout。
- `tx=0x29000010`：软件 tapnum 字段为 `0x10`（默认 16），没有做 TXCLK tap 扫描。
- `rx=0x28000000`：DLYENA 与 RXCLK source select 置位。
- `strbin=0x09000004`：DLYENA/FROM_SW 置位，STRBIN delay 仍为默认 `0x4`。
- `cmdout=0`：未配置 CMDOUT tap。
- `atctrl=0x001f0000`：clock stop / pre-change / post-change delay 位存在。
- `dll ctrl=0x00050201`：DLL start point `0x5`、increment `0x2`、START 置位。

这些是错误瞬间的控制器/DLL 快照，不是 root cause 证明。

---

## 8. 错误类型与证据链

### 8.1 已证实事实

1. **首次错误是 `DATA_CRC`**。

   `int=0x00208001`：

   | 位 | 值 | 含义 |
   |---|---:|---|
   | bit0 | `0x00000001` | `SDHCI_INT_DATA_END`（正常完成位，不是错误类） |
   | bit15 | `0x00008000` | `SDHCI_INT_ERROR`（错误汇总位） |
   | bit21 | `0x00200000` | `SDHCI_INT_DATA_CRC`（数据 CRC 错误） |

   首条 `INT_STATUS` 中没有：
   - `SDHCI_INT_DATA_TIMEOUT`（bit20）
   - `SDHCI_INT_DATA_END_BIT`（bit22）
   - `SDHCI_INT_ADMA_ERROR`（bit25）
   - `SDHCI_INT_AUTO_CMD_ERR`（bit24）
   - `SDHCI_INT_CMD_*` 错误位

2. 首笔请求是 CMD25 多块写：

   ```text
   opcode=25, cmd_flags=0x000000b5
   data_flags=0x100 (MMC_DATA_WRITE)
   blocks=1024, blksz=512  -> 512KiB MMC request
   bytes_xfered=0
   ```

3. 控制器已进入 HS400 ES：

   ```text
   hctl=0x35
   hctl2=0x000f       (UHS mode 7 + 1.8V)
   emmc_ctrl=0x10d    (CARD_IS_EMMC + ENHANCED_STROBE)
   ```

4. DLL 锁定、无 timeout：

   ```text
   dll stat0 = 0x171  (LOCKED bit8=1, TIMEOUT bit9=0)
   ```

5. `ADMA_ERROR` 寄存器非零不能单独说明 ADMA 故障：

   ```text
   adma_err=0x30
   int=0x00208001 不含 SDHCI_INT_ADMA_ERROR
   ```

   因此首笔错误不是 ADMA 事件；`adma_err` 只能作为控制器残留/状态值看待。

6. 写入没有成功落地：

   ```text
   dd 输出 0 字节
   readback sha256 = 64fc...（等于写入前备份，不等于 pattern 的 f8e7...）
   ```

### 8.2 证据链

- 0030 在 `dwcmshc_cqe_irq_handler()` 入口捕获，早于 `sdhci_cqe_irq()` 和核心清状态；
- 首条日志的 `int`/`status` 都是 `0x00208001`；
- 写请求是 CMD25、`MMC_DATA_WRITE`、`bytes_xfered=0`；
- 因此“HS400 ES @100MHz 首次写入的第一手硬件错误 = DATA_CRC”是有直接寄存器证据的事实。

### 8.3 推断（未证明为根因）

- DATA_CRC 指向 DAT 数据采样窗口、TX/RX 时序或信号完整性；
- DLL 已锁定且无 timeout，说明错误发生时并非 DLL 未锁；
- TXCLK tapnum 仍为默认 `0x10`，未做任何扫描，不能判断 tap 是否处于最佳窗口；
- 需要后续受控实验分别验证 TXCLK/RXCLK/STRBIN、驱动强度和信号完整性，不能直接归因于某一个 PHY 参数。

### 8.4 待验证

- `err_stats` 中 `Command Timeout=6` 的来源：首条 0030 日志没有 CMD timeout 位，
  该计数可能来自后续重试/状态恢复路径，当前没有第二次 0030 捕获，无法仅凭计数确认具体命令。
- `adma_err=0x30` 的实际状态含义需要结合 ADMA 状态机/描述符，但首条 INT_STATUS 没有 ADMA 错误位。
- 没有读取/解码 R1，也没有读取 EXT_CSD 错误状态；本轮不使用这些值下结论。

---

## 9. 是否发生 block layer 重试

是。证据：

- 操作员只执行了一次 `dd`；
- `err_stats` 从全 0 变为 `Data CRC=6`、`Command Timeout=6`；
- 首笔错误 `blocks=1024, blksz=512` = 512KiB，说明 512MiB 的 dd 写被 MMC 层拆成多个 512KiB 请求；
- 两条 `I/O error` 的绝对扇区为：
  - `60020736` = p2 start `1048576` + test start `58972160`；
  - `60021760` = 前一个 + `1024` sectors = +512KiB；
  对应测试区起始的两个连续 512KiB 请求。

因此不是“单次请求只报一次错误”。`dd` 最终退出码 1，0 字节写出；
没有出现“重试后成功”的情况。

`MMC_MAX_RETRIES=5` / `MMC_DATA_RETRIES=2` 位于
`drivers/mmc/core/block.c`；数据错误会走重试/恢复路径。具体每个请求的尝试次数
和 `Command Timeout` 的精确来源仍需更细 trace，当前只确认发生了多次底层重试。

---

## 10. HS200 恢复与数据完整性

HS400 错误后立即恢复 USB HS200 DTB：

```text
/boot/rockchip/rk3568-sunniwell-z96a-typec.dtb
sha256 = d025984c947eb46eb9520c0ecd805ff74c8d3eab256573858c07e3a28c235c7a
```

重启后：

```text
mmc0: new HS200 MMC card at address 0001
clock:          200000000 Hz
actual clock:   200000000 Hz
bus width:      3 (8 bits)
timing spec:    9 (mmc HS200)
signal voltage: 1 (1.80 V)
driver type:    0 (driver type B)
```

测试区从备份恢复：

```text
restore sha256 = 64fc0210f826f815e0e4cf6e6798c1cc80d45043df1dccd9c4bb5b30ff3b9718
```

最终状态：

```text
root:           /dev/sda2（USB）
eMMC:           HS200 @200MHz
eMMC p2:        未挂载
eMMC p2 hash:   64fc...（原始备份 hash）
eMMC p1 DTB:    03b8...（未修改）
eMMC p1 Image:  c923...（未修改）
USB DTB:        d025...（HS200 已恢复）
USB Image:      6e97...（0030 生产内核）
diag:           N
diag_reset:     0
err_stats:      全 0
外部模块:       无
HS400:          未启用
debugfs:        已卸载
```

---

## 11. 已证实 / 推断 / 待验证

### 已证实

- HS400 Enhanced Strobe @100MHz、8-bit、1.8V 已实际生效；
- 唯一一次写入使用 CMD25、`MMC_DATA_WRITE`，首笔 512KiB 请求 `bytes_xfered=0`；
- 首条硬件错误为 `DATA_CRC`（`int=0x00208001`）；
- 首条状态不含 DATA_TIMEOUT、DATA_END_BIT、ADMA_ERROR、AUTO_CMD_ERR 或 CMD 错误位；
- DLL 在错误瞬间锁定、无 timeout；
- 写入未成功，测试区读回 hash 与原始备份一致；
- 发生了多次底层重试，`err_stats` 最终 `Data CRC=6`、`Command Timeout=6`；
- HS200 已恢复，测试区已恢复，eMMC p1 未改动。

### 推断

- DATA_CRC 指向 DAT 采样窗口/TX-RX 时序/信号完整性方向；
- DLL 锁定排除了“错误瞬间 DLL 未锁”这一简单解释；
- 两个 I/O error 扇区对应测试区起始的两个 512KiB 请求，说明 dd 的 1MiB 输入被拆成两笔 MMC 请求；
- 后续 Command Timeout 计数可能来自重试/状态恢复路径，而不是首笔 DATA_CRC 本身。

### 待验证

- `Command Timeout=6` 的精确来源（CMD13 状态读取、恢复命令还是重试中的 CMD25）；
- `adma_err=0x30` 的实际状态含义；
- TXCLK/RXCLK/STRBIN tap、驱动强度、信号完整性；
- 是否在第二次 HS400 写入中仍出现相同首错（本轮按规则未重试）。

---

## 产物

```text
build/out/dwcmshc-diag/hs400/
  usb-hs200.dtb / usb-hs200.dts
  hs400-es-100mhz.dtb / .dts / .roundtrip.dts
  running.dtb / running.dts
  running-hs400.fdt
  dmesg-hs400-boot.txt
  dmesg-before-hs400-write.txt
  dmesg-after-hs400-write.txt
  hs400-dd.log
  hs400-read.log
  ios-after-hs400.txt
  err_stats-after-hs400.txt
  final-ios-hs200.txt
  final-err_stats-hs200.txt
  final-dmesg.txt
  final-state.txt
```

本报告：`docs/HS400_FIRST_WRITE_DIAG_REPORT.md`
