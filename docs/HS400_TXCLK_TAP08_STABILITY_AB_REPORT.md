# RK3568 HS400ES TXCLK tap=0x8 稳定性及 A/B 因果验证报告

日期：2026-11-13
平台：Sunniwell Z96A / RK3568
内核基线：Linux 7.2.12，基础 commit `0211cf630ee32283297935c56a12a510bd4584d6`
测试条件：HS400 Enhanced Strobe @100MHz，8-bit，1.8V
本轮只改变 HS400/ES TXCLK tap；0030 诊断保留；0031 未自动提交。

---

## 0. 结论摘要

A1（tap=0x8 冷启动 3 次）：**3/3 通过**。
B（tap=0x10 A/B 回退）：**首次写入复现 DATA_CRC**，dd 失败。
A2（恢复 tap=0x8）：**1/1 通过**。

按任务判定表：

> **A1 全通过 + B 出现 DATA_CRC + A2 通过**
> => 在“当前 RK3568 板卡 + 当前 eMMC + 100MHz HS400ES”范围内，**强证据支持 TXCLK tap 是 DATA_CRC 的关键影响因素**。

仍应限定范围：不能外推到其他 SoC、其他 eMMC、其他频率或其他 PHY 参数。

---

## 1. 测试数据与测试区

测试区：

```text
eMMC p2 尾部
skip=28795 MiB
count=512 MiB
p2 未挂载
```

测试数据：

```text
/home/user/hs400-pattern-tap08.bin
size 536870912
sha256 ed92a2039b6b594748b58db561f76d54d0a3c99f21e3cb6d7fd36967fb813e85
```

每轮写入前重新备份测试区：

| 轮次 | backup 文件 | backup SHA256 |
|---|---|---|
| A1-R1 | `a1-r1/backup.bin` | `64fc0210...` |
| A1-R2 | `a1-r2/backup.bin` | `64fc0210...` |
| A1-R3 | `a1-r3/backup.bin` | `64fc0210...` |
| B | `b-tap10/backup.bin` | `64fc0210...` |
| A2 | `a2-tap08/backup.bin` | `64fc0210...` |

每轮恢复后 p2 hash 均为：

```text
64fc0210f826f815e0e4cf6e6798c1cc80d45043df1dccd9c4bb5b30ff3b9718
```

---

## 2. A1：tap=0x8 冷启动重复 3 次

每一轮均为完整 `reboot`（完整内核重新启动，不是仅 reload 驱动）。
为创建 debugfs `mmc0` 节点，每轮 boot 后做一次 sdhci unbind/bind（无数据写），然后清零 `err_stats` 并执行唯一一次 512MiB 写入。

所有轮次相同条件：

```text
USB root
eMMC p2 未挂载
HS400ES 100MHz / 8-bit / 1.8V
DTB sha256 774343c45ffbf33c026266a1f5c520efe559d0ada260a63f1cbc1e1bb43ac036
测试数据 ed92...
diag=1, diag_reset=0
```

### A1-R1

```text
启动时间       2026-11-13 15:07:55 UTC
Image          971d3c292d4e25f0852f2c69099ee560dc2230d7d9ca62376c175037e976db30
TXCLK          0x29000008
RXCLK          0x28000000
STRBIN         0x09000004
CMDOUT         0x00000000
DLL_CTRL       0x00050201
DLL_STATUS0    0x00000171  (LOCKED=1, TIMEOUT=0)
err_stats前    全 0
err_stats后    全 0
dd rc          0
写入耗时/速率  5.42807 s / 98.9 MB/s
读回 hash      ed92a2039b6b594748b58db561f76d54d0a3c99f21e3cb6d7fd36967fb813e85
0030 日志      无
I/O error      无
block retry    无（err_stats 全 0，dmesg 无 I/O error）
恢复 p2 hash   64fc0210...
```

### A1-R2

```text
启动时间       2026-11-13 15:11:06 UTC
Image          971d3c29...
TXCLK          0x29000008
RXCLK          0x28000000
STRBIN         0x09000004
CMDOUT         0x00000000
DLL_CTRL       0x00050201
DLL_STATUS0    0x00000170  (LOCKED=1, TIMEOUT=0)
err_stats前    全 0
err_stats后    全 0
dd rc          0
写入耗时/速率  8.64866 s / 62.1 MB/s
读回 hash      ed92...
0030 日志      无
I/O error      无
block retry    无
恢复 p2 hash   64fc0210...
```

### A1-R3

```text
启动时间       2026-11-13 15:14:26 UTC
Image          971d3c29...
TXCLK          0x29000008
RXCLK          0x28000000
STRBIN         0x09000004
CMDOUT         0x00000000
DLL_CTRL       0x00050201
DLL_STATUS0    0x00000171  (LOCKED=1, TIMEOUT=0)
err_stats前    全 0
err_stats后    全 0
dd rc          0
写入耗时/速率  7.91132 s / 67.9 MB/s
读回 hash      ed92...
0030 日志      无
I/O error      无
block retry    无
恢复 p2 hash   64fc0210...
```

**A1 结果：3/3 通过；未出现 DATA_CRC、I/O error、block retry，hash 全部一致。**

---

## 3. B：tap=0x10 A/B 回退

使用 0030 生产 Image（不含 0031）：

```text
/boot/Image
sha256 6e971b5d7dcf1183006f4035a4f2dfdbd4df0e6c559c37a5c9b87874aec491cf
```

其余条件与 A1 相同：

```text
同一 7.2.12 基线
同一 DTB 774343c4...
同一测试数据 ed92...
同一测试区/512MiB
100MHz / 8-bit / 1.8V
diag=1, diag_reset=0
```

实际寄存器：

```text
TXCLK          0x29000010
RXCLK          0x28000000
STRBIN         0x09000004
CMDOUT         0x00000000
DLL_CTRL       0x00050201
DLL_STATUS0    0x00000171
```

启动时间：

```text
2026-11-13 15:17:35 UTC
```

写入结果：

```text
dd_rc=1
输入了 1+0 块记录
输出了 0+0 块记录
0 字节已复制，0.127573 s，0.0 kB/s
```

首次硬件错误：

```text
[53.130106] sdhci-dwcmshc fe310000.mmc: DWCMSHC diag: first WRITE error int=00208001 status=00208001 ier=03ff000b int_en=03ff000b sig=03ff000b
[53.130151] ... opcode=25 cmd_flags=000000b5 data_flags=100 blocks=1024 blksz=512 xfer=0
[53.130162] ... present=03f700f0 hctl=35 hctl2=000f clk=0007 reset=00
[53.130172] ... adma_err=00000030 adma_addr=0061b208/00000000 auto_cmd=0000 xfer=002b cmd=193a
[53.130181] ... emmc_ctrl=0000010d host3=000f0010 atctrl=001f0000
[53.130189] ... dll ctrl=00050201 rx=28000000 tx=29000010 strbin=09000004 cmdout=00000000 stat0=00000171
[53.203393] I/O error, dev mmcblk0, sector 60020736 op 0x1:(WRITE) ...
[53.203487] I/O error, dev mmcblk0, sector 60021760 op 0x1:(WRITE) ...
```

`err_stats`：

```text
Data CRC Errors Occurred: 6
Command Timeout Occurred: 0
其他: 0
```

读回：

```text
64fc0210f826f815e0e4cf6e6798c1cc80d45043df1dccd9c4bb5b30ff3b9718
```

即测试区未被成功写入，等于原始备份 hash。

**B 结果：首次 DATA_CRC 复现，dd 失败；未执行第二次写入。**

恢复：

```text
HS200 DTB d025984c...
p2 恢复 hash 64fc0210...
```

---

## 4. A2：恢复 tap=0x8 后再次验证

使用 0031 Image：

```text
/boot/Image
sha256 971d3c29...
```

条件与 A1/B 相同；启动时间：

```text
2026-11-13 15:21:10 UTC
```

寄存器：

```text
TXCLK          0x29000008
RXCLK          0x28000000
STRBIN         0x09000004
CMDOUT         0x00000000
DLL_CTRL       0x00050201
DLL_STATUS0    0x00000171
```

结果：

```text
dd rc          0
写入耗时/速率  5.37598 s / 99.9 MB/s
读回 hash      ed92a2039b6b594748b58db561f76d54d0a3c99f21e3cb6d7fd36967fb813e85
err_stats前    全 0
err_stats后    全 0
0030 日志      无
I/O error      无
block retry    无
恢复 p2 hash   64fc0210...
```

**A2 结果：恢复 tap=0x8 后再次通过。**

---

## 5. A/B/A 汇总

| 轮次 | tap | Image | TXCLK | dd rc | 速率 | 读回 hash | err_stats | 结论 |
|---|---:|---|---:|---:|---:|---|---|---|
| A1-R1 | 0x8 | 0031 | `0x29000008` | 0 | 98.9 MB/s | pattern | 全 0 | 通过 |
| A1-R2 | 0x8 | 0031 | `0x29000008` | 0 | 62.1 MB/s | pattern | 全 0 | 通过 |
| A1-R3 | 0x8 | 0031 | `0x29000008` | 0 | 67.9 MB/s | pattern | 全 0 | 通过 |
| B | 0x10 | 0030 | `0x29000010` | 1 | 0.0 | 原备份 | Data CRC=6 | DATA_CRC 复现 |
| A2 | 0x8 | 0031 | `0x29000008` | 0 | 99.9 MB/s | pattern | 全 0 | 通过 |

所有轮次：

- 同一测试数据 `ed92...`；
- 同一测试区 `skip=28795, count=512`；
- 同一 DTB；
- 100MHz / 8-bit / 1.8V；
- 写入前备份 hash `64fc...`；
- 写入后恢复 hash `64fc...`；
- 每轮只执行一次测试写入。

---

## 6. 根因证据等级

### 已证实

- A1 三轮 tap=0x8 冷启动首次写入全部通过；
- A2 在 tap=0x10 对照之后再次通过；
- B tap=0x10 首错为 DATA_CRC，`INT_STATUS=0x00208001`；
- B 读回仍是原始数据，没有成功写入；
- tap=0x8 时 TXCLK 实际为 `0x29000008`；tap=0x10 时为 `0x29000010`；
- 其他寄存器（RX/STRBIN/CMDOUT/DLL_CTRL/STAT0）在两配置中一致；
- 每轮恢复 p2 hash 均为 `64fc...`。

### 证据等级

**强证据**（限定于当前板卡/eMMC/100MHz HS400ES）：

- TXCLK tap=0x8 是本次 DATA_CRC 的关键影响因素；
- A/B/A 方向一致：
  - 0x8 → 4 次通过；
  - 0x10 → 首次 DATA_CRC；
  - 0x8 → 再通过。

### 不能外推

- 其他 RK3568 板卡/eMMC 可能不同；
- 200MHz HS400ES 未在本轮验证；
- U-Boot 的 tap=8 修复是在 200MHz 验证；本结果只覆盖 100MHz；
- 没有做信号完整性/驱动强度/温度/电压裕量测试；
- 不能据此认为所有 HS400/ES DATA_CRC 都由 TXCLK tap 解释。

---

## 7. 0031 是否建议正式提交

**建议：可以正式提交 0031，但应保持限定的提交说明和回滚路径。**

理由：

- 0031 只影响 RK3568 revision 0 的 HS400/ES；
- HS200 仍为 0x10，本轮 HS200 恢复后实际读回 `0x29000010`；
- RXCLK/STRBIN/CMDOUT/DLL 未改变；
- A1 3/3 + A2 1/1 通过，B 复现 DATA_CRC；
- 代码改动小（3 个 hunk），可回滚。

提交前建议在 commit message 中明确：

```text
- only RK3568 revision 0 HS400/HS400ES TX tap
- HS200 remains 0x10
- validated at 100MHz HS400ES on this board/eMMC
- not validated at 200MHz or other boards/eMMC
```

本轮按指令**没有自动提交 0031**；它仍在 patch 目录，等待确认。

---

## 8. 最终状态

```text
root:           /dev/sda2（USB）
eMMC:           HS200 @200MHz
eMMC p2:        未挂载
eMMC p2 hash:   64fc0210...
eMMC p1 DTB:    03b8...（未修改）
eMMC p1 Image:  c923...（未修改）
USB DTB:        d025...（HS200 已恢复）
USB Image:      971d...（0031 测试内核）
diag:           N
diag_reset:     0
err_stats:      全 0
外部模块:       无
debugfs:        已卸载
HS400:          未启用
```

HS200 实际寄存器：

```text
TXCLK  = 0x29000010
RXCLK  = 0x28000000
STRBIN = 0x09000004
CMDOUT = 0x00000000
```

证明 0031 没有改变 HS200 分支。

---

## 9. 产物

```text
build/out/dwcmshc-diag/tap08/
  a1-r1/  a1-r2/  a1-r3/
  b-tap10/
  a2-tap08/
    start.txt
    dtb.txt
    ios.txt
    regs.txt
    preflight.txt
    backup.bin / backup-dd.txt
    err_stats_pre.txt / err_stats_after.txt
    dmesg_before.txt / dmesg_after.txt
    dd.log
    read-hash.txt / read.log
    regs_after.txt
    diaglog.txt / errors.txt
    restore.txt
```

本报告：`docs/HS400_TXCLK_TAP08_STABILITY_AB_REPORT.md`

---

## 10. 限制说明

- 每轮“冷启动”为完整 `reboot`，不是物理断电上电；没有条件做远程电源循环。
- 为创建 debugfs 节点，每轮 boot 后做一次 unbind/bind；该操作没有数据写，之后才清零 err_stats 并执行唯一测试写入。
- 每轮使用同一随机 pattern `ed92...`；上一轮基线 pattern `f8e7...` 未保留，但本轮所有 A/B/A 轮次完全使用同一 pattern，A/B 对照有效。
