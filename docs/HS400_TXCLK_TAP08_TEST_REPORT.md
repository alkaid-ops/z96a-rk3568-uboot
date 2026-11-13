# RK3568 HS400 ES TXCLK tap=0x8 单参数验证报告

日期：2026-11-13
平台：Sunniwell Z96A / RK3568
内核：Linux `7.2.12`，`CONFIG_MMC_SDHCI_OF_DWCMSHC=y`
测试条件：HS400 Enhanced Strobe @100MHz，8-bit，1.8V
本轮只改变 HS400/HS400ES TXCLK tap：`0x10 -> 0x8`。

---

## 0. 结论摘要

- 补丁 `0031` 只增加 RK3568 revision 0 的 HS400/ES TX tap 字段，并只在 `MMC_TIMING_MMC_HS400` 使用；HS200 分支保持 `priv->txclk_tapnum`（0x10）。
- 实际 HS400ES 寄存器确认：

```text
TXCLK  = 0x29000008
RXCLK  = 0x28000000
STRBIN = 0x09000004
CMDOUT = 0x00000000
```

- 第一次 512MiB 写入 **通过**：

```text
dd rc=0，5.42807 s，98.9 MB/s
读回 hash == pattern hash
err_stats 全部为 0
无 0030 诊断日志
无 I/O error / block retry
```

- 这是单次写入通过，**不能认定 HS400ES 已稳定**。本轮没有做第二次 HS400 写入、没有冷启动重复、没有 A/B 回退。
- 结果**初步支持**“TXCLK tap=0x8 影响 DATA_CRC”，但尚未达到根因证明或稳定性证明。
- HS200 已恢复，测试区恢复原 hash；当前内核仍为 0031 测试内核（HS200 分支未变）。

---

## 1. 修改前核验

### 1.1 RK3568 revision=0 的 HS400/ES 路径

Linux 7.2.12：

```text
drivers/mmc/host/sdhci-of-dwcmshc.c
  :328-336  rockchip_pltfm_data，revision 0 = RK3566/RK3568
  :752-758  dwcmshc_rk3568_set_clock
  :850-852  HS200/HS400 原共用 priv->txclk_tapnum
  :854-863  revision 1（RK3588 等）才使用 90° TX / CMDOUT
  :865-869  TXCLK = DLYENA | FROM_SW | bit29 | txclk_tapnum
  :871-874  STRBIN = DLYENA | tap4 | FROM_SW
```

Linux 的 HS400ES 使用 `MMC_TIMING_MMC_HS400` + `enhanced_strobe=true`：

```text
drivers/mmc/core/mmc.c:1453-1459
```

因此，一个只针对 `MMC_TIMING_MMC_HS400` 的分支会同时覆盖 HS400 和 HS400ES，不影响 HS200。

### 1.2 TXCLK 位宽与语义

```text
DWCMSHC_EMMC_DLL_TXCLK (0x808)
  bit[7:0]  TX tap value（本测试使用 0x8）
  bit24     FROM_SW
  bit27     DLYENA
  bit29     TX source-select
```

修改前：

```text
0x29000010 = bit29 | bit27 | bit24 | 0x10
```

修改后：

```text
0x29000008 = bit29 | bit27 | bit24 | 0x08
```

`DLYENA`、`FROM_SW`、source select 均不变；只改低 8 位 tap。

### 1.3 与 U-Boot `5c053f3adc` 的语义一致性

U-Boot 第三补丁：

```c
- .hs400_txclk_tapnum = DLL_TXCLK_TAPNUM_DEFAULT;  /* 0x10 */
+ .hs400_txclk_tapnum = 0x8;
```

U-Boot 写入同样为：

```text
TXCLK = DLYENA | FROM_SW | source select | tap 8
      = 0x29000008
```

Linux 0031 与 U-Boot 的 TX tap 语义一致。
没有照搬 U-Boot 的 RXCLK、CMDOUT 或 DLL stop 配置。

---

## 2. 最小补丁 0031

文件：

```text
patches/git/z96a-v2/linux-add_sunniwell_z96a/0031-rk3568-hs400-txclk-tap-08.patch
sha256 9cc6f550784e5abf46dbfd83541a81fc07f38049eeb8c438dc29596bc4b933b5
```

完整 diff：

```diff
--- a/drivers/mmc/host/sdhci-of-dwcmshc.c
+++ b/drivers/mmc/host/sdhci-of-dwcmshc.c
@@
 	 * - Revision 0: Exclusively used by RK3566 and RK3568 SoCs.
 	 * - Revision 1: Implemented in all other Rockchip SoCs, including RK3576, RK3588, etc.
 	 */
+	u8 hs400_txclk_tapnum;
 	int revision;
 };

@@
 	if (host->mmc->ios.timing == MMC_TIMING_MMC_HS200 ||
 	    host->mmc->ios.timing == MMC_TIMING_MMC_HS400)
 		txclk_tapnum = priv->txclk_tapnum;

+	if (rockchip_pdata->revision == 0 &&
+	    host->mmc->ios.timing == MMC_TIMING_MMC_HS400)
+		txclk_tapnum = rockchip_pdata->hs400_txclk_tapnum;
+
 	if (rockchip_pdata->revision == 1 && host->mmc->ios.timing == MMC_TIMING_MMC_HS400) {
 		txclk_tapnum = DLL_TXCLK_TAPNUM_90_DEGREES;

@@
 		.init = dwcmshc_rk35xx_init,
 		.postinit = dwcmshc_rk35xx_postinit,
 	},
+	.hs400_txclk_tapnum = 0x8,
 	.revision = 0,
 };
```

只影响：

- `rockchip_pdata->revision == 0`；
- `MMC_TIMING_MMC_HS400`（包括 HS400ES）；
- 写入 `TXCLK` 的 tap 字段。

不影响：

- HS200：仍执行 `txclk_tapnum = priv->txclk_tapnum`（0x10）；
- revision 1（RK3588 等）：仍走原有 90° TX/CMDOUT 分支；
- RXCLK、STRBIN、CMDOUT、DLL CTRL、ATCTRL。

---

## 3. 应用与编译

构建方式：从 pinned commit `0211cf63...` 生成干净源码，按补丁目录顺序应用 0001–0031，保留 0030 诊断补丁。

应用日志：

```text
build/out/dwcmshc-diag/build-0031-patch.log
0031 应用：patching file drivers/mmc/host/sdhci-of-dwcmshc.c
无 fuzz/reject
```

编译：

```text
make -j28 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- \
  CC="ccache aarch64-linux-gnu-gcc-14" \
  KBUILD_BUILD_USER=foghorn KBUILD_BUILD_HOST=leghorn.com Image
```

结果：

```text
build rc=0
warning: 0
CONFIG_MMC_SDHCI_OF_DWCMSHC=y
# CONFIG_MODVERSIONS is not set
```

Image：

```text
build/out/dwcmshc-diag/Image-0031
sha256 971d3c292d4e25f0852f2c69099ee560dc2230d7d9ca62376c175037e976db30
```

0030 仍存在：

```text
drivers/mmc/host/sdhci-of-dwcmshc.c 内含
  "DWCMSHC diag: first WRITE error ..."
无 diag_test
```

### 3.1 静态检查 HS200 分支未变

0031 final diff 只有 3 个 hunk（见上）。HS200 的赋值语句保持：

```c
if (timing == MMC_TIMING_MMC_HS200 ||
    timing == MMC_TIMING_MMC_HS400)
        txclk_tapnum = priv->txclk_tapnum;
```

新分支只在 `revision == 0 && timing == MMC_TIMING_MMC_HS400` 时覆盖；HS200 不会进入。

### 3.2 静态检查 HS400/ES 只有 TXCLK tap 变化

`RXCLK` 行、`STRBIN` 行、`CMDOUT=0` 路径、`DLL CTRL`、`ATCTRL` 均未在 0031 中出现修改。

### 3.2 实际 HS200 交叉验证

恢复 HS200 后直接读寄存器：

```text
TXCLK  = 0x29000010
RXCLK  = 0x28000000
STRBIN = 0x09000004
CMDOUT = 0x00000000
```

证明 0031 没有改变 HS200 的 TX tap。

---

## 4. 板端实验

### 4.1 部署

- 新 Image：`971d3c29...` 安装到 USB `/boot/Image`；
- 旧 0030 Image 备份：`/boot/Image.pre-0031-20261113`，hash `6e971b5d...`；
- HS400ES DTB：沿用上一轮 `774343c4...`，安装到 USB `/boot/rockchip/...dtb`；
- eMMC p1 未修改。

### 4.2 HS400ES 状态确认

`/proc/device-tree/mmc@fe310000`：

```text
max-frequency = 100000000 (bytes 05 f5 e1 00)
mmc-hs400-1_8v present
mmc-hs400-enhanced-strobe present
```

`dmesg`：

```text
mmc0: new HS400 Enhanced strobe MMC card at address 0001
```

`ios`：

```text
clock:          100000000 Hz
actual clock:   100000000 Hz
bus width:      3 (8 bits)
timing spec:    10 (mmc HS400 enhanced strobe)
signal voltage: 1 (1.80 V)
driver type:    0 (driver type B)
```

### 4.3 实际寄存器（写入前/写入后一致）

```text
0xfe310800 DLL_CTRL   = 0x00050201
0xfe310804 RXCLK      = 0x28000000
0xfe310808 TXCLK      = 0x29000008
0xfe31080c STRBIN     = 0x09000004
0xfe310810 CMDOUT     = 0x00000000
0xfe310840 DLL_STATUS0= 0x00000171
0xfe310844 DLL_STATUS1= 0x001C0139
hctl2                 = 0x000F (HS400 + 1.8V)
```

与要求一致：

```text
TXCLK  = 0x29000008
RXCLK  = 0x28000000
STRBIN = 0x09000004
CMDOUT = 0x00000000
```

写入后同样值（`STAT0` rebind 后为 `0x170`，LOCKED=1/TIMEOUT=0；不影响 TX/RX/STRBIN/CMDOUT）。

---

## 5. 唯一一次 512MiB 写入

测试区：p2 尾部 `skip=28795`、`count=512`（512MiB），未挂载。
备份：

```text
/home/user/emmc-testarea-backup-tap08.bin
size 536870912
sha256 64fc0210f826f815e0e4cf6e6798c1cc80d45043df1dccd9c4bb5b30ff3b9718
```

测试数据：

```text
/home/user/hs400-pattern-tap08.bin
size 536870912
sha256 ed92a2039b6b594748b58db561f76d54d0a3c99f21e3cb6d7fd36967fb813e85
```

说明：上一轮的随机 pattern 文件已清理，未保留；本轮使用同尺寸、同 dd 方法的新固定 pattern，因此 pattern hash 与上一轮 `f8e7...` 不同。测试区、长度和写入方式保持一致。

命令：

```sh
dd if=/home/user/hs400-pattern-tap08.bin of=/dev/mmcblk0p2 \
  bs=1M count=512 seek=28795 oflag=direct conv=fsync
```

结果：

```text
dd_rc=0
输入了 512+0 块记录
输出了 512+0 块记录
536870912 字节 (537 MB, 512 MiB) 已复制，5.42807 s，98.9 MB/s
```

读回：

```text
ed92a2039b6b594748b58db561f76d54d0a3c99f21e3cb6d7fd36967fb813e85
```

与 pattern hash 完全一致。

---

## 6. `err_stats` 与 dmesg

写入前：

```text
全部 0
```

写入后：

```text
Command Timeout: 0
Command CRC:     0
Data Timeout:    0
Data CRC:        0
Auto-Cmd:        0
ADMA:            0
Tuning:          0
CMDQ/RED/GCE/ICCE: 0
Request Timedout: 0
CTRL Timedout:   0
Unexpected IRQ:  0
```

`dmesg` 无：

- `DWCMSHC diag`（0030 one-shot 未触发）；
- `I/O error`；
- `CRC` 错误；
- block layer 重试/恢复日志。

只有正常的 HS400ES 启动/rebind 记录。

---

## 7. 与 tap=0x10 基线的对照

| 项目 | tap=0x10（上一轮） | tap=0x8（本轮） |
|---|---|---|
| HS400ES 100MHz | 是 | 是 |
| TXCLK | `0x29000010` | `0x29000008` |
| RXCLK | `0x28000000` | `0x28000000` |
| STRBIN | `0x09000004` | `0x09000004` |
| CMDOUT | 0 | 0 |
| DLL LOCKED | 1 | 1 |
| 首次写 | `DATA_CRC`，dd rc=1，0 字节 | 无错误，dd rc=0，98.9 MB/s |
| err_stats | Data CRC=6, CMD Timeout=6 | 全部 0 |
| 读回 hash | 原始备份 hash（未写入） | pattern hash（写入成功） |

**初步支持：**TXCLK tap 是本次 DATA_CRC 的一个影响因素。
**尚未证明：**

- 只做了一次 100MHz 写入；
- 没有冷启动重复；
- 没有 A/B 回退（恢复 0x10 确认错误重现）；
- U-Boot 的 tap=8 修复是在 200MHz 验证；100MHz 的 tap 相位不同；
- 单次成功不能说明长期稳定。

---

## 8. HS200 恢复与数据完整性

HS400 测试后恢复 HS200 DTB：

```text
/boot/rockchip/rk3568-sunniwell-z96a-typec.dtb
sha256 d025984c947eb46eb9520c0ecd805ff74c8d3eab256573858c07e3a28c235c7a
```

重启后：

```text
mmc0: new HS200 MMC card at address 0001
clock 200MHz，8-bit，1.8V，timing spec 9 (HS200)
```

HS200 寄存器：

```text
TXCLK  = 0x29000010
RXCLK  = 0x28000000
STRBIN = 0x09000004
CMDOUT = 0x00000000
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
eMMC p2 hash:   64fc...
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

---

## 9. 当前判定

- **情况 B（第一次写入通过）成立**，但不是稳定结论。
- 0031 patch 目前只是测试补丁，尚未提交到镜像仓库；后续冷启动重复验证通过后再决定是否合入。
- 不继续第二次 HS400 写入，不改变 RX/STRBIN/CMDOUT，不扫描其他 tap。
- 下一步（等待确认后）：
  1. 保持 0031；冷启动 3 次，每次一次 512MiB 写入；
  2. 每次检查 `err_stats` 全 0、hash 一致、无 block retry；
  3. 做 A/B 回退：恢复 tap=0x10，确认 DATA_CRC 是否重现；
  4. 只有重复 + A/B 都成立，才把 TXCLK tap 作为已证实影响因素。

---

## 10. 产物

```text
build/out/dwcmshc-diag/
  0031-rk3568-hs400-txclk-tap-08.patch
  Image-0031
  System.map-0031
  config-0031
  build-0031-patch.log
  build-0031-image.log
  tap08/
    dmesg-before-tap08-write.txt
    dmesg-after-tap08-write.txt
    dmesg-after-tap08-full.txt
    hs400-tap08-dd.log
    hs400-tap08-read.log
    tap08-ios-after.txt
    tap08-err_stats-after.txt
    tap08-regs-after.txt
    tap08-hs200-regs.txt
    tap08-final-ios-hs200.txt
    tap08-final-err_stats-hs200.txt
    tap08-final-dmesg.txt
    tap08-final-state-corrected.txt
```

本报告：`docs/HS400_TXCLK_TAP08_TEST_REPORT.md`
