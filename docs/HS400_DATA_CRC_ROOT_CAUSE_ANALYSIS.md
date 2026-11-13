# RK3568 HS400 ES @100MHz DATA_CRC 根因分析

日期：2026-11-13
固定证据：`docs/HS400_FIRST_WRITE_DIAG_REPORT.md`（commit `2a9d87c`）
证据目录：`build/out/dwcmshc-diag/hs400/`
本轮只做源码/文档审计，不修改内核、DTB、U-Boot，不执行硬件写入。

---

## 0. 结论摘要

1. **首次硬件错误是 `DATA_CRC`**，不是 CMD 错误、DATA_TIMEOUT、ADMA 或 Auto CMD 错误。
   `INT_STATUS=0x00208001` 解码为：`DATA_END(bit0) | ERROR(bit15) | DATA_CRC(bit21)`。
2. `DATA_END` 是正常完成/数据阶段结束位，和 `DATA_CRC` 可以同时置位；它不是“写入成功”的证据。Linux 驱动先处理 `DATA_CRC`，`DATA_END` 不会覆盖错误。
3. `Command Timeout Occurred=6` 不是首条 0030 日志中的错误。它只可能来自后续重试/恢复路径中的 CMD13、CMD12 或 CMD25 超时；源码位置已定位，但本次只有一次 one-shot 捕获，**无法证明 6 次具体分布**。
4. `adma_err=0x30` 不符合标准 SDHCI ADMA Error Status 的已定义位（bit2 LMIS + bits1:0 AES），且同一控制器在上一轮 **正常 HS200 DATA_END** 快照中同样出现 `0x30`。`INT_STATUS` 没有 ADMA_ERROR 位，因此不能把 0x30 与本次 DATA_CRC 建立因果。
5. 当前 Linux 寄存器配置：
   - `TXCLK=0x29000010`：tap=0x10、DLYENA、FROM_SW、TX source-select；
   - `RXCLK=0x28000000`：DLYENA、NO_INVERTER；
   - `STRBIN=0x09000004`：tap=0x4、DLYENA、FROM_SW；
   - `CMDOUT=0`：RK3568 revision 0 不配置；
   - `DLL=0x00050201`，`STAT0=0x00000171`（LOCKED=1、TIMEOUT=0）。

   这些值与 **Linux 7.2.12 主线 + Barebox 2026 当前实现** 的 RK3568 revision 0 默认值一致；
   Rockchip vendor BSP 和当前 U-Boot 的 RK3568 HS400 配置存在差异，**TX tap 分别是 8 vs Linux 的 0x10**。
6. **最值得优先验证的下一轮单参数是 HS400/HS400ES 的 TXCLK tap=0x8**，依据是 U-Boot commit `5c053f3adc`（“Fix HS400 mode write on RK3568”）以及 vendor BSP `hs400_tx_tap=8`。
   但该 U-Boot 修复是在 **200MHz HS400/ES** 上验证的；本次 Linux 故障是 **100MHz**，所以它是“明确方向”，不是已证明的 100MHz 修复。
7. 目前不能宣布根因。`DLL LOCKED=1` 只证明 DLL 锁定，不证明采样/发送相位窗口正确。

---

## 1. 固定证据与实验上下文

### 1.1 首次错误

```text
INT_STATUS = 0x00208001
  0x00000001  DATA_END
  0x00008000  ERROR
  0x00200000  DATA_CRC
```

首笔请求：

```text
opcode=25 (CMD25)
cmd_flags=0x000000b5
data_flags=0x100 (MMC_DATA_WRITE)
blocks=1024
blksz=512
bytes_xfered=0   (驱动在错误后置 0，不等于硬件绝对没有发出数据)
```

### 1.2 错误瞬间寄存器

```text
hctl=0x35
hctl2=0x000f
emmc_ctrl=0x0000010d
dll ctrl=0x00050201
rx=0x28000000
tx=0x29000010
strbin=0x09000004
cmdout=0x00000000
stat0=0x00000171
adma_err=0x00000030
auto_cmd=0x0000
```

### 1.3 `err_stats`

```text
Command Timeout Occurred: 6
Data CRC Errors Occurred: 6
其他: 0
```

### 1.4 环境

```text
HS400 Enhanced Strobe @100MHz
8-bit, 1.8V, driver type B
DLL LOCKED=1, TIMEOUT=0
dd 退出码 1，0 字节写出
测试区读回 hash = 原始备份 hash
```

---

## 2. DATA_CRC 完整驱动调用链

### 2.1 入口：0030 hook 在原始状态被清除之前

0030 生产补丁把诊断放在 `dwcmshc_cqe_irq_handler()` 入口：

```text
0030-patched /tmp/diagmod/sdhci-of-dwcmshc.c:730  dwcmshc_cqe_irq_handler()
0030-patched /tmp/diagmod/sdhci-of-dwcmshc.c:737  dwcmshc_diag_maybe()
```

调用顺序：

1. `sdhci_irq()` 先读 `SDHCI_INT_STATUS`：
   `drivers/mmc/host/sdhci.c:3572`
2. 调用 `host->ops->irq`：
   `drivers/mmc/host/sdhci.c:3581-3582`
3. 0030 在 `host->ops->irq` 入口读同一份原始 `INT_STATUS`，尚未被 `sdhci_irq` 清除；
4. 0030 返回后，`sdhci_cqe_irq()` 在 CQE off 时返回 false；
5. `sdhci_irq()` 写 `mask` 清状态：
   `drivers/mmc/host/sdhci.c:3587-3590`
6. 然后按 CMD/DATA 掩码分发：
   `drivers/mmc/host/sdhci.c:3622-3626`

因此 0030 的 `int=status=0x00208001` 是**原始硬件锁存状态**，不是驱动处理后的结果。

本机 `cmdq_en=0`，CQE 未启用；`dwcmshc_cqe_irq_handler()` 中的 `sdhci_cqe_irq()` 返回 false，不会走 CQE 错误统计路径。

### 2.2 `sdhci_data_irq()` 处理 DATA_CRC

`drivers/mmc/host/sdhci.c:3406` 进入 `sdhci_data_irq()`。

对于 `intmask=DATA_END|DATA_CRC`：

1. `host->data` 非空；
2. 不是 DATA_TIMEOUT；
3. 不是 DATA_END_BIT；
4. 命中 DATA_CRC 分支：
   `drivers/mmc/host/sdhci.c:3477-3482`
   - `host->data->error = -EILSEQ`
   - `sdhci_err_stats_inc(host, DAT_CRC)`：`sdhci.c:3482`
5. 设置错误后，`sdhci_finish_data()`：
   `drivers/mmc/host/sdhci.c:3499-3500`

`DATA_END` 的 else 分支在 `sdhci.c:3531` 之后，只有 `host->data->error == 0` 才会进入；本次错误非 0，所以 **DATA_END 不会作为成功完成处理**。

### 2.3 `sdhci_finish_data()` 设置请求失败

```text
sdhci_finish_data()                  sdhci.c:1648
 -> __sdhci_finish_data()            sdhci.c:1605
    -> __sdhci_finish_data_common()  sdhci.c:1566
       - host->data = NULL            :1571
       - host->data_cmd = NULL        :1572
       - data->error -> 控制器 reset  :1578-1585
       - data->bytes_xfered = 0       :1598-1599
    - 多块写错误时发送 CMD12 stop     :1611-1641
```

`sdhci_reset_for(REQUEST_ERROR)`：

```text
sdhci.c:285-290  -> SDHCI_RESET_CMD + SDHCI_RESET_DATA
```

这一步是错误后的控制器 CMD/DATA 电路复位；0030 读到的是复位前状态。

### 2.4 MMC core 请求完成

`sci_finish_data()` 最终通过 `__sdhci_finish_mrq()` 把请求放入完成队列：

```text
__sdhci_finish_mrq()                 sdhci.c:1532
sdhci_set_mrq_done()                 sdhci.c:1511
sdhci_irq() 最后调用 mmc_request_done sdhci.c:3677-3685
mmc_request_done()                   core.c:139
```

`mmc_request_done()` 对 `data->error == -EILSEQ` 会触发 `mmc_retune_needed()`（`core.c:144-150`），然后调用 `mrq->done`；对块请求，`mrq->done` 是 `mmc_blk_mq_req_done()`。

### 2.5 block layer 错误与重试

```text
mmc_blk_mq_req_done()                block.c:2210
  mmc_blk_rq_error(&mqrq->brq)       block.c:1921-1926
    -> recovery_needed = true         block.c:2249-2258
       schedule_work(mm->recovery_work)
       -> mmc_blk_mq_recovery()       block.c:2159
          -> mmc_blk_mq_rw_recovery() block.c:1838
```

`mmc_blk_mq_rw_recovery()` 的关键动作：

```text
__mmc_send_status(card,&status,0)     block.c:1853  (CMD13)
if (err) __mmc_send_status(...)       block.c:1864  (第二次 CMD13)
mmc_blk_fix_state(card,req)           block.c:1876
  -> mmc_blk_send_stop()              block.c:1754-1763 (CMD12, mmc_wait_for_cmd retries=5)
  -> mmc_poll_for_busy()              block.c:1777 -> mmc_ops.c:552
       -> mmc_busy_cb()               mmc_ops.c:470
          -> mmc_send_status()        mmc_ops.c:480  (CMD13)
```

之后：

```text
if (mqrq->retries < 3)                block.c:1915
    mqrq->retries = 3                 block.c:1916
mmc_blk_mq_complete_rq()              block.c:2016
  nr_bytes == 0
  mqrq->retries++ < MMC_MAX_RETRIES   block.c:2028
     -> blk_mq_requeue_request()
  else -> blk_mq_end_request(IOERR)   block.c:2031-2035
```

`MMC_MAX_RETRIES=5`、`MMC_DATA_RETRIES=2`：

```text
drivers/mmc/core/block.c:1750-1751
```

### 2.6 `err_stats` 的递增位置

`err_stats` 是 `host->err_stats[MMC_ERR_MAX]`：

```text
include/linux/mmc/host.h:142-159
include/linux/mmc/host.h:742-745
drivers/mmc/host/sdhci.h:419-420
```

Data CRC：

```text
sdhci_data_irq() DATA_CRC 分支
  sdhci.c:3477-3482  -> sdhci_err_stats_inc(host, DAT_CRC)
```

Command Timeout：

```text
sdhci_cmd_irq()  CMD timeout
  sdhci.c:3330-3334  -> sdhci_err_stats_inc(host, CMD_TIMEOUT)

sdhci_data_irq()  no-data / busy data_cmd + DATA_TIMEOUT
  sdhci.c:3431-3435  -> sdhci_err_stats_inc(host, CMD_TIMEOUT)

sdhci_cqe_irq() CQE CMD timeout
  sdhci.c:4013-4019  -> sdhci_err_stats_inc(host, CMD_TIMEOUT)
```

本机 CQE 未启用（`cmdq_en=0`），所以第三条路径不适用。

**结论**：`Data CRC=6` 表示硬件/驱动处理了 6 次 DATA_CRC 事件；`Command Timeout=6` 表示另有 6 次超时事件。它们不是 6 次 dd 调用。

---

## 3. 为什么一次 dd 会产生多个底层错误计数

### 3.1 请求拆分

SDHCI 把单次 MMC 请求限制为 512KiB：

```text
sdhci_setup_host()  mmc->max_req_size = 524288
drivers/mmc/host/sdhci.c:4770
```

0030 捕获的 `blocks=1024, blksz=512` 正好是：

```text
1024 * 512 = 512KiB
```

dd 的 1MiB 输入被拆成两个连续的 512KiB MMC 请求。两条 `I/O error` 的绝对扇区：

```text
60020736 = p2_start(1048576) + test_start(58972160)
60021760 = 60020736 + 1024 sectors (+512KiB)
```

### 3.2 重试

`mmc_blk_mq_complete_rq()` 在 `nr_bytes==0` 时把请求重新排队：

```text
block.c:2016-2035
```

`mmc_blk_mq_rw_recovery()` 对数据错误会降低剩余重试次数：

```text
block.c:1910-1917
```

因此同一 512KiB 请求可以被多次重发；每次重发都可能再次触发 DATA_CRC，`err_stats` 逐次加 1。dd 层只看到最终 I/O error，不会看到每次硬件重试。

### 3.3 计数不是“重试次数”的精确除法

`Data CRC=6` + `Command Timeout=6` 可能对应：

- 两个 512KiB 请求、多次重发；或
- 一个请求的多次重发 + 恢复路径中的 CMD13/CMD12 超时；或
- 两者混合。

仅凭计数器不能唯一反推“每个请求重试了几次”。需要下一次 trace 才能精确分解。

---

## 4. `Command Timeout Occurred: 6` 的来源

### 4.1 已定位的递增点

| 路径 | 源码 | 触发条件 | 是否可能 |
|---|---|---|---|
| `sdhci_cmd_irq()` | `sdhci.c:3330-3334` | 任意 CMD 的 `SDHCI_INT_TIMEOUT` | 是；CMD13/CMD12/CMD25 都可能 |
| `sdhci_data_irq()` busy 分支 | `sdhci.c:3431-3435` | `host->data==NULL`，data_cmd 有 `MMC_RSP_BUSY`，`DATA_TIMEOUT` | 可能；CMD12 busy 结束路径 |
| `sdhci_cqe_irq()` | `sdhci.c:4013-4019` | CQE on + `SDHCI_INT_TIMEOUT` | 否；`cmdq_en=0` |

### 4.2 本次最可能的恢复调用

`mmc_blk_mq_rw_recovery()` 在每次数据错误后：

1. `__mmc_send_status()` CMD13（`block.c:1853`）；
2. 若第一次 status 失败，再发一次 CMD13（`block.c:1864`）；
3. 若状态不是 ready，`mmc_blk_fix_state()`：
   - `mmc_blk_send_stop()` CMD12，`mmc_wait_for_cmd(..., 5)`（`block.c:1754-1763`）；
   - `mmc_poll_for_busy()`，内部 `mmc_send_status()` CMD13（`mmc_ops.c:480`）。

这些 CMD 若遇到 `SDHCI_INT_TIMEOUT`，都会走 `sdhci_cmd_irq()` 的 `CMD_TIMEOUT` 计数。

### 4.3 能确定什么、不能确定什么

**能确定：**

- 首条 `INT_STATUS` 没有 `SDHCI_INT_TIMEOUT`（bit16），所以首错不是 CMD timeout。
- `Command Timeout=6` 必然发生在首条 DATA_CRC 之后，或来自后续重试/恢复。
- 源码中能够产生 `CMD_TIMEOUT` 的路径只有上表三条；CQE 路径被 `cmdq_en=0` 排除。
- 因此“由重试期间的 CMD13/CMD12/状态查询/恢复流程产生”是**代码上可能的**，并且与计数多次出现一致。

**不能确定：**

- 6 次具体是 CMD13、CMD12、CMD25 还是混合；
- 每次恢复实际发了几次命令；
- 是否有 CMD25 在后续重试中直接超时。

要回答这些，需要下一轮在 `sdhci_cmd_irq()` 或 MMC tracepoint 上记录 `SDHCI_GET_CMD(SDHCI_COMMAND)`、`intmask` 和请求序号；本轮不做。

---

## 5. DATA_CRC 与 DATA_END 同时出现

### 5.1 SDHCI 定义

- `SDHCI_INT_DATA_END` = bit0，数据阶段结束/Transfer Complete 类正常状态；
- `SDHCI_INT_DATA_CRC` = bit21，数据 CRC 错误；
- `SDHCI_INT_ERROR` = bit15，错误汇总。

源码定义：`drivers/mmc/host/sdhci.h:181-189`。

三者可以同时出现在同一个 `INT_STATUS` 快照中，规范没有互斥要求。SDHCI 规范把 Data End 定义为数据传输结束事件，把 Data CRC 定义为写 CRC status 不为 `010` 或读数据 CRC 错误；错误发生时数据阶段仍可能到达结束点。

### 5.2 Linux 驱动如何处理

`sdhci_data_irq()` 的判断顺序是：

```text
DATA_TIMEOUT
DATA_END_BIT
DATA_CRC / TUNING_ERROR
ADMA_ERROR
```

`DATA_CRC` 命中后立即设置 `host->data->error=-EILSEQ` 并 `sdhci_finish_data()`：

```text
sdhci.c:3477-3500
```

之后的 `DATA_END` else 分支不会执行；`DATA_END` 不会覆盖错误。
Linux 历史上也有补丁说明 TC（DATA_END）可能比 CRC 错误晚到，驱动在 `!host->data` 时会忽略迟到的 DATA_END：

- `[PATCH 09/23] mmc: sdhci: fix incorrect get data interrupt during no data transfer`
- `https://lists.infradead.org/pipermail/linux-arm-kernel/2016-May/428189.html`

### 5.3 本次快照的解释

- 0030 在 `sdhci_irq()` 清状态之前读取，看到原始锁存 `0x00208001`；
- `DATA_END` 是数据阶段结束的锁存位，不是“成功”；
- 由于 `DATA_CRC` 已置位，驱动后续把整笔请求判为 `-EILSEQ`，并把 `bytes_xfered=0`；
- 单次快照无法绝对排除“上一条传输残留”的可能，但同一 `sdhci_irq` 已先读到同一值，且请求是 CMD25 写、`bytes_xfered=0`；当前事件解释最合理。
  **不要把 DATA_END 当作成功，也不要把 DATA_END 当成额外错误类型。**

---

## 6. ADMA_ERROR `0x30` 审计

### 6.1 标准定义

SDHCI ADMA Error Status（offset `0x54`）：

```text
bits 1:0  AES  (ADMA Error State)
bit 2     LMIS (ADMA Length Mismatch)
bits 7:3  reserved
```

标准 AES 值：

```text
0 = ST_STOP
1 = ST_FDS
2 = reserved / never
3 = ST_TFR
```

参考：

- SD Host Controller Simplified Specification 4.20, ADMA Error Status Register；
- Microchip SAMA7G54 “SDMMC ADMA Error Status Register”：
  `https://onlinedocs.microchip.com/oxy/GUID-999BFD8D-CFE5-4F54-AF70-76475125B7FB-en-US-2/GUID-E3AEE8E3-3295-40E7-ACD5-3ECEB393EAF7.html`

### 6.2 解码 `0x30`

```text
0x30 = 0b0011_0000
  bits 1:0 = 00 -> AES = ST_STOP
  bit 2     = 0  -> no length mismatch
  bits 4,5  = 1  -> reserved in standard SDHCI
```

因此 `0x30` **不是标准 ADMA Error Status 的可解释错误组合**。它要么是 DWCMSHC 的 vendor-specific 状态位/内部状态，要么是残留值；不能仅凭它判断 ADMA 故障。

### 6.3 生命周期与残留证据

- `SDHCI_ADMA_ERROR` 是只读状态寄存器：`drivers/mmc/host/sdhci.h:304`；
- 0030 在 `dwcmshc_diag_dump()` 读取：`/tmp/diagmod/sdhci-of-dwcmshc.c:366`；
- 驱动没有在每次传输前显式写清该寄存器；`sdhci_prepare_data()` 不写它；
- 错误后 `sdhci_finish_data()` 会做 CMD/DATA reset（`sdhci.c:1578-1585`、`285-290`），但 0030 读在 reset 之前；
- **关键对照**：上一轮 HS200 正常写入的强制快照（`int=0x00000001`，正常 DATA_END）同样记录：

```text
adma_err=00000030
adma_addr=044ee208/00000000
```

来源：`build/out/dwcmshc-diag/INSTALL-NOTES.md:81`。

这说明 `0x30` 在同一控制器、正常 HS200 写入中也会出现，不是 HS400 DATA_CRC 专属，也不是本次 ADMA 事件的证据。

### 6.4 本次能否排除 ADMA 故障

- `INT_STATUS=0x00208001` 不含 `SDHCI_INT_ADMA_ERROR`（bit25=`0x02000000`）；
- `adma_addr=0x12836208` 是有效 DMA 地址；
- `adma_err=0x30` 的标准位没有有效 AES/LMIS；
- 正常 HS200 快照同样为 `0x30`。

**结论：当前证据不支持“ADMA 故障是本次 DATA_CRC 的原因”；但不能用 0x30 本身证明没有历史/残留 ADMA 状态。**
不建议本轮增加“清除 ADMA_ERROR”的写操作；如果要进一步排除，下一轮只增加**前后只读快照**（request 前后 + error 时），不要写清。

---

## 7. RK3568 HS400 ES PHY 配置审计

### 7.1 当前寄存器逐 bit

#### TXCLK `0x29000010` (`DWCMSHC_EMMC_DLL_TXCLK`)

```text
0x29000010 = 0x20000000 | 0x08000000 | 0x01000000 | 0x10
  bit29 = TX source-select (Linux 用同名 DLL_RXCLK_NO_INVERTER << 29 写入 TX 寄存器)
  bit27 = DLYENA
  bit24 = TX tap value from software
  bits[7:0] = 0x10 (tap 16)
```

源码：`drivers/mmc/host/sdhci-of-dwcmshc.c:865-869`；宏 `:106-107,113`。

#### RXCLK `0x28000000` (`DWCMSHC_EMMC_DLL_RXCLK`)

```text
0x28000000 = 0x20000000 | 0x08000000
  bit29 = NO_INVERTER (source select)
  bit27 = DLYENA
  bit31 = ORI_GATE not set
```

源码：`sdhci-of-dwcmshc.c:827-830`（revision 0 分支）。

#### STRBIN `0x09000004` (`DWCMSHC_EMMC_DLL_STRBIN`)

```text
0x09000004 = 0x08000000 | 0x01000000 | 0x4
  bit27 = DLYENA
  bit24 = FROM_SW
  bits[7:0] = 0x4 (STRBIN tap 4)
```

源码：`:871-874`；宏 `:108-109`。

#### CMDOUT `0x00000000` (`DECMSHC_EMMC_DLL_CMDOUT`)

RK3568 `rockchip_pdata->revision == 0` 分支不写 CMDOUT：

```text
sdhci-of-dwcmshc.c:854-863  (revision 1 才写)
```

#### DLL CTRL `0x00050201`

```text
0x00050201 = 0x5 << 16 | 0x2 << 8 | 0x1
  start point = 5
  increment = 2
  START = 1
```

源码：`:833-836`。

#### STAT0 `0x00000171`

```text
bit8 = DWCMSHC_EMMC_DLL_LOCKED = 1
bit9 = DWCMSHC_EMMC_DLL_TIMEOUT = 0
低 8 位 = 0x71（vendor/DLL 内部状态/锁定值，不作为相位窗口证明）
```

宏：`sdhci-of-dwcmshc.c:97-99`；锁检查 `:837-839`。

> `LOCKED=1` 只证明 DLL 锁定，不证明 TX/RX 相位窗口正确。

### 7.2 与 Linux 7.2.12 自身配置对比

当前 Linux 代码的 RK3568 revision 0：

- `DLL_TXCLK_TAPNUM_DEFAULT=0x10`，HS200 和 HS400 使用同一个 `priv->txclk_tapnum`（默认 0x10）；
- revision 0 设置 RX NO_INVERTER；
- HS400 不写 CMDOUT；
- STRBIN 固定 tap 4；
- `HOST_CTRL3` bit4 设置（内部 clock gate 关闭）。

源码：

```text
sdhci-of-dwcmshc.c:328-336   revision 定义
sdhci-of-dwcmshc.c:752-758   默认 tx tap
sdhci-of-dwcmshc.c:828-830   RX rev0
sdhci-of-dwcmshc.c:850-852   HS200/HS400 共用 tap
sdhci-of-dwcmshc.c:854-863   rev1 才写 TX 90° / CMDOUT
sdhci-of-dwcmshc.c:865-874   TX/STRBIN
sdhci-of-dwcmshc.c:2125-2138 rk3568 pdata revision=0
```

因此当前 `TXCLK=0x29000010`、`RXCLK=0x28000000`、`STRBIN=0x09000004`、`CMDOUT=0` **正是 Linux 主线 + Barebox 2026 的 RK3568 rev0 默认配置**，不是异常漂移。

### 7.3 与 Barebox 2026 交叉核对

Barebox 2026-05 的 `rockchip-dwcmshc` 系列明确说明：

- Revision 0 (RK3566/RK3568)：
  - `DLL_RXCLK` source-select 使用 `DLL_RXCLK_NO_INVERTER`；
  - HS400 使用默认 `0x10` TX clock tap；
  - 不配置 `DECMSHC_EMMC_DLL_CMDOUT`。
- 后续修复：
  - `DLL_STRBIN_TAPNUM_DEFAULT` 从 0x8 改为 0x4（与 Linux 对齐）；
  - `DWCMSHC_HOST_CTRL3` 关闭内部 clock gate（bit4）。

参考：

- `[PATCH v2 08/10] mci: sdhci: rockchip: distinguish IP revision 0 ...`
  `https://www.mail-archive.com/barebox@lists.infradead.org/msg58719.html`
- `[PATCH] drivers: mci: rockchip: fix HS400 DLL strobe and clock gate config`
  `http://lists.infradead.org/pipermail/barebox/2026-May/056570.html`
- `[PATCH v2 07/10] mci: sdhci: rockchip: set TX-path source-select bit ...`
  `http://lists.infradead.org/pipermail/barebox/2026-May/056418.html`

**当前 Linux 的 TX/RX/STRBIN/CMDOUT 与 Barebox 的 revision 0 结论一致；Barebox 也没有建议把 RK3588 的 90° TX/CMDOUT 参数套到 RK3568。**

### 7.4 与 Rockchip vendor BSP / 当前 U-Boot 交叉核对

当前 U-Boot v2026.01（本仓库 `u-boot-mainline-src`，`v2026.01-63-g10942055fd`）的 RK3568 `rk3568_data`：

```text
drivers/mmc/rockchip_sdhci.c:690-700
  hs200_txclk_tapnum = 0x10
  hs400_txclk_tapnum = 0x8
  hs400_cmdout_tapnum = 0x8
  hs400_strbin_tapnum = 0x4
  ddr50_strbin_delay_num = 0x10
```

当前 U-Boot HS400/ES 写入：

- `TXCLK`: `0x29000008`（tap 8）；
- `RXCLK`: `0xA8000000`（DLYENA + ORI_GATE + NO_INVERTER）；
- `CMDOUT`: `0x59000008`（SRC_CLK_NEG + BOTH_CLK_EDGE + DLYENA + tap 8 + FROM_SW）；
- `STRBIN`: `0x09000004`（tap 4）。

源码：

```text
u-boot-mainline-src/drivers/mmc/rockchip_sdhci.c:327-330   stop DLL on disable
:361-366   RXCLK
:368-371   HS400/HS400ES tap selection
:373-380   CMDOUT
:382-387   TXCLK
:389-393   STRBIN
```

Rockchip vendor BSP（旧 5.10 风格镜像）`rk3568_drvdata`：

```text
hs200_tx_tap = 16
hs400_tx_tap = 8
hs400_cmd_tap = 8 (但 RK3568 无 RK_DLL_CMD_OUT flag，实际不写 CMDOUT)
hs400_strbin_tap = 4
flags = RK_PLATFROM | RK_RXCLK_NO_INVERTER
```

参考：`https://raw.githubusercontent.com/li-chiga/RK3588-machine-learning/main/kernel/drivers/mmc/host/sdhci-of-dwcmshc.c`
可信度：中（GitHub vendor 镜像/旧内核，不是本板 BSP 原始发布）。

### 7.5 RK3568 TRM

公开 RK3568 TRM Part2 PDF 已找到入口：

`https://dl.radxa.com/rock3/docs/hw/datasheet/Rockchip%20RK3568%20TRM%20Part2%20V1.1-20210301.pdf`

但本轮下载未完整完成，且公开 TRM 对 DWCMSHC 内部 DLL 的 vendor-specific 位定义并不完整/不易检索。
**TRM 独立核验可信度：低**；当前结论主要依赖 Linux 主线、Barebox 和 U-Boot/BSP 源码交叉核对。

### 7.6 Enhanced Strobe 的实际影响

- `DWCMSHC_EMMC_CONTROL[8]`（`DWCMSHC_ENHANCED_STROBE`）由 `dwcmshc_hs400_enhanced_strobe()` 设置：`sdhci-of-dwcmshc.c:583-599`、`:2453`。
- HS400ES 的 core 路径是设置 `MMC_TIMING_MMC_HS400` 后调用 `host->ops->hs400_enhanced_strobe`：`drivers/mmc/core/mmc.c:1453-1459`。
- 因此本次 `emmc_ctrl=0x10d` 的 bit8 是 ES 路径已启用的证据。
- ES 让控制器使用 eMMC 的 data strobe 采样；它不能替代 host TX 数据时序、CMD/DAT 信号完整性和写 CRC status 的采样。
- `STRBIN`/DS 路径正确不代表 TX 发送相位正确；`TXCLK` tap 仍可能是写方向 DATA_CRC 的关键变量。

---

## 8. 根因候选排序

按证据强度（不是最终概率）：

| 排名 | 候选 | 证据 | 强度 | 备注 |
|---|---|---|---|---|
| 1 | **HS400/ES TXCLK tap 不是 8** | 当前 Linux=0x10；U-Boot commit `5c053f3adc` 把 RK3568 HS400 tap 从 0x10 改为 0x8；vendor BSP=8 | 高（方向） | U-Boot 在 200MHz HS400/ES 验证；Linux 100MHz 需重新验证 |
| 2 | 板级 DAT/CLK 信号完整性 / 驱动强度 | DATA_CRC 是写 CRC status 错误；无眼图/驱动强度数据 | 中 | 需要示波器/误码率，不是 tap 扫描 |
| 3 | CMDOUT 未配置 | Linux=0；当前 U-Boot HS400/ES=0x59000008 | 中低 | 首个 CMD25 命令阶段没有 CMD 错误；CMDOUT 主要影响命令输出，不能直接解释 DATA_CRC |
| 4 | RXCLK 缺 ORI_GATE / RX 路径差异 | Linux=0x28000000；U-Boot=0xA8000000 | 中低 | 可能影响 CRC status/读回采样；需单变量验证 |
| 5 | DLL 锁定但相位窗口不对 | `STAT0` bit8=1、bit9=0；但 tap/相位未校准 | 中 | “LOCKED”不等于窗口正确 |
| 6 | STRBIN Enhanced Strobe 采样窗口 | Linux/U-Boot/BSP 都是 tap4 | 低 | 当前值已被三方一致采用；不宜先扫 |
| 7 | eMMC HS400ES 能力/兼容性 | 卡进入 HS400ES；HS200 正常 | 低 | 不能完全排除，但不是首要 |
| 8 | DLL disable 生命周期（U-Boot patch 的另一半） | Linux `set_clock` 在 clock=0 不显式 stop DLL | 低 | 更影响模式切换，不直接解释第一笔写 |

**为什么不能只归因于 RXCLK tap：**
- HS400 写方向的 `DATA_CRC` 是 host 检查 eMMC 返回的 Write CRC Status（值不为 `010`）或 CRC status 采样错误；
- 写数据由 host TX 发送，TXCLK/CMDOUT/板级 SI 会直接影响 eMMC 看到的 CRC；
- 读方向的 DS/STRBIN 主要影响 host 采样 eMMC 数据；
- 因此必须同时考虑 TX 发送和 RX/STRBIN 采样，不能把 DATA_CRC 直接等同于 RX tap 错误。

---

## 9. 下一轮最小风险实验方案

### 阶段 A：零参数基线 / 只读快照

**建议：先加只读字段，不改变 PHY 参数。**

在现有 0030 基础上增加以下只读字段（一次 HS400 写即可收集）：

```text
DWCMSHC_EMMC_DLL_STATUS1 (0x844)
SDHCI_ADMA_ERROR (0x54)  请求前/错误时
DWCMSHC_EMMC_DLL_STATUS0 低 8 位
SDHCI_INT_STATUS 原始值
```

目的：

- 确认 `STATUS1` 是否给出更多 DLL 相位/错误信息；
- 确认 `ADMA_ERROR=0x30` 在请求前后是否变化，排除/确认残留；
- 不写清 ADMA_ERROR，不改变 DLL/PHY。

判定：

- 若 `ADMA_ERROR` 在正常请求前后保持不变，且 `STATUS1` 无异常，则 ADMA 仍不是候选；
- 若 `STATUS1` 出现异常，再单独分析，不改 PHY。

### 阶段 B1：单参数——HS400/ES TXCLK tap 8

**选择理由：**

- 当前 Linux `TXCLK=0x29000010`（tap 16）；
- U-Boot commit `5c053f3adc` 把 RK3568 HS400 tap 改为 8，并在 HS400/ES 200MHz 写入测试中修复 ERROR；
- vendor BSP RK3568 `hs400_tx_tap=8`；
- 该差异直接作用于写方向 TX 数据时序，和 DATA_CRC 最相关。

**实验变量：**

只改变 HS400/HS400ES 的 TX tap 0x10 -> 0x8；HS200 保持 0x10；RX/STRBIN/CMDOUT/DLL 不变。

实现方式优先级：

1. 临时驱动数据字段 `hs400_txclk_tapnum=0x8`，仅 `MMC_TIMING_MMC_HS400` 使用；不改公共 DT 语义；
2. 若必须零代码，使用现有 `rockchip,txclk-tapnum` DT 属性设为 8，但它是 HS200/HS400 共用的，可能污染 HS200；不推荐作为首选。

**测试：**

- 保持 HS400ES **100MHz** 第一轮（与当前故障条件一致）；
- 每次冷启动只做一次 512MiB 写入；
- 第一次出现 DATA_CRC/任何错误立即停止该参数；
- 若首轮通过，再 3 次冷启动 × 1 次 512MiB 写入；
- 每轮结束恢复 HS200 DTB；测试区备份/恢复 hash。

**判定：**

- 首错消失；
- `Data CRC=0`、`Command Timeout=0`；
- dd rc=0，readback hash == pattern；
- dmesg 无 I/O error/block retry；
- 3 次冷启动都满足；
- 然后做 A/B 回退：恢复 tap 0x10，确认 DATA_CRC 再次出现；若回退也通过，则不能归因于 tap 8。

**注意：** U-Boot 的验证是 200MHz；100MHz 下 tap 8 的相位不同。
如果 100MHz tap8 失败，不要立即扫描；下一轮可把 **200MHz tap8** 作为一个独立的“时钟+同一 tap”对照实验，以判断 U-Boot 方向是否只在 200MHz 成立。但不能把 100MHz 失败直接解释成 tap8 无效。

### 阶段 B2（仅当 B1 失败）：CMDOUT 0x8

当前 U-Boot HS400/ES 写 CMDOUT=`0x59000008`，Linux 为 0。
但首个 CMD25 命令阶段没有 CMD 错误，CMDOUT 主要影响命令输出；对 DATA_CRC 的证据弱于 TX tap。
只改 CMDOUT，保持 TX tap 0x10；一轮一个变量；失败即停。

### 阶段 B3（仅当 B1/B2 失败）：RXCLK ORI_GATE bit31

当前 U-Boot `RXCLK=0xA8000000`（含 ORI_GATE），Linux `0x28000000`（无）。
该位可能影响 CRC status/读路径采样；只改 RXCLK bit31，保持 TX/STRBIN/CMDOUT。
证据弱于 TX tap，放在第三。

### 阶段 B4：DLL stop-on-disable

U-Boot patch 3 还在 `config_dll(enable=false)` 时写 0 停止 DLL；Linux `set_clock(clock=0)` 目前只关 clock。
这与“第一笔 HS400 写”的 DATA_CRC 关系弱，优先用于模式切换/时钟切换稳定性测试，不作为首轮 DATA_CRC 实验。

### 阶段 C：结果分类

- 首次错误消失 + Data CRC/CMD Timeout 归零 + hash 一致 + 3 次冷启动 + A/B 回退 => 强证据该参数影响 DATA_CRC；
- 仅一次成功 => 不算稳定；
- 错误类型改变（DATA_CRC -> DATA_TIMEOUT/CMD_CRC 等）=> 记录新首错，不宣布成功；
- 任何 block layer 隐藏重试（err_stats 非零、dmesg I/O error）=> 不算成功。

---

## 10. 已证实 / 推断 / 待验证

### 已证实

- 首错是 DATA_CRC，DATA_END 同时置位但只是数据阶段结束；
- 首错不含 DATA_TIMEOUT、DATA_END_BIT、ADMA、AUTO_CMD 或 CMD 错误位；
- `Data CRC=6` 来自 `sdhci_data_irq()` 的 DAT_CRC 计数；
- `Command Timeout=6` 只可能来自后续 CMD13/CMD12/CMD25 或 busy 恢复路径；首条日志无 CMD timeout；
- `ADMA_ERROR=0x30` 不是标准 ADMA 错误状态；同一控制器正常 HS200 快照也为 0x30；INT_STATUS 无 ADMA 位；
- 当前 Linux TX/RX/STRBIN/CMDOUT 配置与 Linux 主线/Barebox rev0 默认一致；
- U-Boot 当前版本已包含 RK3568 HS400 tap=8 和 stop-DLL 修复；Linux 仍是 0x10；
- HS200 已恢复，测试区 hash 原样，eMMC p1 未改动。

### 推断

- DATA_CRC 指向 HS400ES 写方向 TX 数据/CRC status 采样时序或板级信号完整性；
- U-Boot 的 TX tap=8 是最明确的单参数方向，但只在 200MHz 验证；
- Command Timeout 很可能来自恢复路径的 CMD13/CMD12，但具体分布未证明；
- `adma_err=0x30` 更像 vendor/残留状态，不是本次首错原因。

### 待验证

- TX tap=8 在 Linux 100MHz HS400ES 是否改变首错；
- CMDOUT/RX ORI_GATE 是否是独立影响因素；
- `DLL_STATUS1` 是否提供更多相位信息；
- `ADMA_ERROR` 在请求前后的生命周期；
- 板级 DAT/CLK/DS 信号完整性/驱动强度；
- 100MHz 与 200MHz 的 tap 相位关系。

---

## 11. 引用源码/文档

### Linux 7.2.12（本仓库 `sources/linux`）

- `drivers/mmc/host/sdhci.c`
  - `sdhci_irq`: 3556
  - `sdhci_cmd_irq`: 3298
  - `sdhci_data_irq`: 3406
  - `sdhci_finish_data`: 1648
  - `__sdhci_finish_data_common`: 1566
  - `__sdhci_finish_mrq`: 1532
  - `sdhci_err_stats_inc`: 3334 / 3435 / 3482 / 4019
  - `mmc->max_req_size=524288`: 4770
- `drivers/mmc/host/sdhci.h`
  - INT bits: 181-189
  - ADMA_ERROR offset: 304
  - ERR_STATS macro: 419-420
- `include/linux/mmc/host.h`
  - err enum: 142-159
  - err inc: 742-745
- `drivers/mmc/core/block.c`
  - `MMC_MAX_RETRIES`: 1750-1751
  - `mmc_blk_send_stop`: 1754-1763
  - `mmc_blk_fix_state`: 1766-1781
  - `mmc_blk_mq_rw_recovery`: 1838-1919
  - `mmc_blk_mq_complete_rq`: 2016-2035
  - `mmc_blk_mq_req_done`: 2210-2268
- `drivers/mmc/core/mmc_ops.c`
  - `__mmc_send_status`: 69-91
  - `mmc_busy_cb`: 470-508
  - `mmc_poll_for_busy`: 552-564
- `drivers/mmc/core/mmc.c`
  - HS400ES sets `MMC_TIMING_MMC_HS400`: 1453-1459
- `drivers/mmc/host/sdhci-of-dwcmshc.c`
  - revision 0/1: 328-336
  - RX rev0: 828-830
  - HS200/HS400 tap: 850-852
  - rev1 90°/CMDOUT: 854-863
  - TX/STRBIN: 865-874
  - ES op: 583-599
  - rk3568 pdata: 2125-2138

### 0030

- `/tmp/diagmod/sdhci-of-dwcmshc.c`
  - `dwcmshc_diag_dump`: 330
  - `dwcmshc_diag_maybe`: 399
  - `dwcmshc_cqe_irq_handler`: 730
  - hook: 737
- `build/out/dwcmshc-diag/0030-z96a-sdhci-dwcmshc-diag.patch`

### U-Boot 当前源码

- `u-boot-mainline-src/drivers/mmc/rockchip_sdhci.c`
  - stop DLL: 327-330
  - RX: 361-366
  - HS400/ES tap: 368-371
  - CMDOUT: 373-380
  - TX: 382-387
  - STRBIN: 389-393
  - rk3568 data: 690-700
  - HS400 UHS: 445-448
  - ES bit: 462-467
- 版本：`v2026.01-63-g10942055fd`
- 包含 commit `5c053f3adc`（HS400 tap 8 + stop DLL）
- 后续扩展 `d64144a467`（CMDOUT/STRBIN 数据字段）

### SDHCI / 规范

- SD Host Controller Simplified Specification 4.20 (ADMA Error Status, Data CRC/End)
  - `https://www.ercankoclar.com/wp-content/uploads/2017/11/Simplified_SD_Host_Controller_Spec.pdf`
- Microchip SAMA7G54 ADMA Error Status Register:
  - `https://onlinedocs.microchip.com/oxy/GUID-999BFD8D-CFE5-4F54-AF70-76475125B7FB-en-US-2/GUID-E3AEE8E3-3295-40E7-ACD5-3ECEB393EAF7.html`
- Linux 2016 DATA_END/CRC 迟到补丁:
  - `https://lists.infradead.org/pipermail/linux-arm-kernel/2016-May/428189.html`
- Barebox revision 0 / STRBIN:
  - `https://www.mail-archive.com/barebox@lists.infradead.org/msg58719.html`
  - `http://lists.infradead.org/pipermail/barebox/2026-May/056570.html`
  - `http://lists.infradead.org/pipermail/barebox/2026-May/056418.html`
- U-Boot 2024 series:
  - cover: `https://lists.u-boot-project.org/pipermail/u-boot/2024-February/545267.html`
  - patch 1/3: `.../545269.html`
  - patch 2/3: `.../545268.html`
  - patch 3/3: `.../545270.html`
- Vendor BSP mirror:
  - `https://raw.githubusercontent.com/li-chiga/RK3588-machine-learning/main/kernel/drivers/mmc/host/sdhci-of-dwcmshc.c`
- RK3568 TRM 入口（本轮未完整核验）：
  - `https://dl.radxa.com/rock3/docs/hw/datasheet/Rockchip%20RK3568%20TRM%20Part2%20V1.1-20210301.pdf`
