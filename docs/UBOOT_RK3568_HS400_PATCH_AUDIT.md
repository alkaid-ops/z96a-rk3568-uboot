# Jonas Karlman RK35xx eMMC 修复系列审计

系列：`[PATCH v2 0/3] rockchip: rk35xx: Fix writing to eMMC`
作者：Jonas Karlman `<jonas@kwiboo.se>`
日期：2024-02-04
审计对象：本仓库当前 U-Boot `v2026.01-63-g10942055fd` 与 Linux `7.2.12`
本轮只审计，不修改 U-Boot/Linux，不编译、不刷写、不执行硬件测试。

---

## 0. 结论摘要

1. 三补丁的原始 diff 已从 u-boot 邮件列表归档取得；patchwork.ozlabs.org 当前被 Anubis 防护，无法直接抓取页面。
2. 第三个补丁 `[PATCH v2 3/3] mmc: rockchip_sdhci: Fix HS400 mode write on RK3568` 的实际修改只有两类：
   - `rk3568_sdhci_config_dll(enable=false)` 时写 `0` 到 `DWCMSHC_EMMC_DLL_CTRL`，停止 DLL；
   - RK3568 `hs400_txclk_tapnum` 从 `DLL_TXCLK_TAPNUM_DEFAULT`（0x10）改为 `0x8`。
     RK3588 的 `hs400_txclk_tapnum` 由宏 `0x9` 改为字面量 `0x9`，行为不变。
3. 该补丁**同时影响 HS400 和 HS400ES**，因为 U-Boot 的 `selected_mode == MMC_HS_400 || selected_mode == MMC_HS_400_ES` 分支共用 `hs400_txclk_tapnum`；它**不影响 HS200**（HS200 仍是 0x10）。
4. 当前项目 U-Boot 已包含该提交 `5c053f3adc`，并且后续 `d64144a467` 把它扩展成 driver data 的 `hs400_cmdout_tapnum` / `hs400_strbin_tapnum` 字段；当前 RK3568 仍是 `hs400_txclk_tapnum=0x8`，没有被回退。
5. 当前 U-Boot HS400/ES 的实际寄存器组合：
   - `TXCLK=0x29000008`（tap 8）；
   - `RXCLK=0xA8000000`（DLYENA + ORI_GATE + NO_INVERTER）；
   - `STRBIN=0x09000004`（tap 4）；
   - `CMDOUT=0x59000008`（90° + both edge + tap 8）。
6. 当前 Linux 7.2.12 的 RK3568 revision 0：
   - `TXCLK=0x29000010`（tap 16）；
   - `RXCLK=0x28000000`（无 ORI_GATE）；
   - `STRBIN=0x09000004`；
   - `CMDOUT=0`。
7. 本次 Linux HS400ES @100MHz 首错 `DATA_CRC`，且 `TXCLK=0x29000010` 正好是 U-Boot 修复前的值。
   **U-Boot 补丁给出了明确的单参数方向：HS400/ES TXCLK tap=0x8。**
   但 U-Boot 的验证是 HS400/ES **200MHz**；Linux 当前是 100MHz，tap 相位不同，不能直接当作已证明的 100MHz 修复。
8. 第三个补丁**没有**修改 RXCLK、STRBIN、CMDOUT；当前 U-Boot 的 `RXCLK`/`CMDOUT` 差异来自补丁之前的代码和后续 `d64144a467`。因此不能把 U-Boot 的所有差异都归因于这个补丁。
9. 对 Linux 的可移植方向：新增独立的 `hs400_txclk_tapnum=0x8`，只在 `MMC_TIMING_MMC_HS400`（Linux 的 HS400 与 HS400ES 共用该 timing）使用；HS200 保持 0x10。不要套用 RK3588 的 0x9/0xA 或 90° CMDOUT。

---

## 1. 三补丁原始 diff

来源（邮件列表归档）：

- cover 0/3：`https://lists.u-boot-project.org/pipermail/u-boot/2024-February/545267.html`
- 1/3：`https://lists.u-boot-project.org/pipermail/u-boot/2024-February/545269.html`
- 2/3：`https://lists.u-boot-project.org/pipermail/u-boot/2024-February/545268.html`
- 3/3：`https://lists.u-boot-project.org/pipermail/u-boot/2024-February/545270.html`

用户给出的 patchwork URL 对应邮件编号 `...-2`、`...-3`、`...-4`，即 1/3、2/3、3/3。
patchwork 页面被 Anubis 拦截，以下 diff 来自 u-boot 邮件列表原始归档。

### 1.1 `[PATCH v2 1/3] rockchip: rk35xx: Remove use of eMMC DDR52 mode`

主题：写入 eMMC 在 DDR52 模式下失败；从 U-Boot board DTS 移除 `mmc-ddr-1_8v`，让 U-Boot 优先 HS200。

```diff
diff --git a/arch/arm/dts/rk3566-quartz64-a-u-boot.dtsi b/arch/arm/dts/rk3566-quartz64-a-u-boot.dtsi
index 11976fd3a6e0..930d660868bb 100644
--- a/arch/arm/dts/rk3566-quartz64-a-u-boot.dtsi
+++ b/arch/arm/dts/rk3566-quartz64-a-u-boot.dtsi
@@ -8,7 +8,6 @@

 &sdhci {
 	cap-mmc-highspeed;
-	mmc-ddr-1_8v;
 	pinctrl-names = "default";
 	pinctrl-0 = <&emmc_bus8 &emmc_clk &emmc_cmd &emmc_datastrobe>;
 };
diff --git a/arch/arm/dts/rk3566-quartz64-b-u-boot.dtsi b/arch/arm/dts/rk3566-quartz64-b-u-boot.dtsi
index 8de9d1535efb..c235b4357f7d 100644
--- a/arch/arm/dts/rk3566-quartz64-b-u-boot.dtsi
+++ b/arch/arm/dts/rk3566-quartz64-b-u-boot.dtsi
@@ -4,7 +4,6 @@

 &sdhci {
 	cap-mmc-highspeed;
-	mmc-ddr-1_8v;
 	pinctrl-names = "default";
 	pinctrl-0 = <&emmc_bus8 &emmc_clk &emmc_cmd &emmc_datastrobe>;
 };
diff --git a/arch/arm/dts/rk3566-radxa-cm3-io-u-boot.dtsi b/arch/arm/dts/rk3566-radxa-cm3-io-u-boot.dtsi
index 158f652cb3b1..e0e501deccfe 100644
--- a/arch/arm/dts/rk3566-radxa-cm3-io-u-boot.dtsi
+++ b/arch/arm/dts/rk3566-radxa-cm3-io-u-boot.dtsi
@@ -7,5 +7,4 @@

 &sdhci {
 	cap-mmc-highspeed;
-	mmc-ddr-1_8v;
 };
diff --git a/arch/arm/dts/rk3566-soquartz-u-boot.dtsi b/arch/arm/dts/rk3566-soquartz-u-boot.dtsi
index f65f4067f3e9..5e46a2422d60 100644
--- a/arch/arm/dts/rk3566-soquartz-u-boot.dtsi
+++ b/arch/arm/dts/rk3566-soquartz-u-boot.dtsi
@@ -4,7 +4,6 @@

 &sdhci {
 	cap-mmc-highspeed;
-	mmc-ddr-1_8v;
 	pinctrl-names = "default";
 	pinctrl-0 = <&emmc_bus8 &emmc_clk &emmc_cmd &emmc_datastrobe>;
 };
diff --git a/arch/arm/dts/rk3568-lubancat-2-u-boot.dtsi b/arch/arm/dts/rk3568-lubancat-2-u-boot.dtsi
index a44ac35bdacd..1597473017ed 100644
--- a/arch/arm/dts/rk3568-lubancat-2-u-boot.dtsi
+++ b/arch/arm/dts/rk3568-lubancat-2-u-boot.dtsi
@@ -8,7 +8,6 @@

 &sdhci {
 	cap-mmc-highspeed;
-	mmc-ddr-1_8v;
 	mmc-hs400-1_8v;
 	mmc-hs400-enhanced-strobe;
 	pinctrl-0 = <&emmc_bus8 &emmc_clk &emmc_cmd &emmc_datastrobe>;
diff --git a/arch/arm/dts/rk3568-nanopi-r5s-u-boot.dtsi b/arch/arm/dts/rk3568-nanopi-r5s-u-boot.dtsi
index 62f572c4cf9f..64c43374c042 100644
--- a/arch/arm/dts/rk3568-nanopi-r5s-u-boot.dtsi
+++ b/arch/arm/dts/rk3568-nanopi-r5s-u-boot.dtsi
@@ -14,7 +14,6 @@

 &sdhci {
 	cap-mmc-highspeed;
-	mmc-ddr-1_8v;
 	mmc-hs200-1_8v;
 	mmc-hs400-1_8v;
 	mmc-hs400-enhanced-strobe;
diff --git a/arch/arm/dts/rk3568-odroid-m1-u-boot.dtsi b/arch/arm/dts/rk3568-odroid-m1-u-boot.dtsi
index ecba91aa30f5..1fc71faa9e07 100644
--- a/arch/arm/dts/rk3568-odroid-m1-u-boot.dtsi
+++ b/arch/arm/dts/rk3568-odroid-m1-u-boot.dtsi
@@ -8,7 +8,6 @@

 &sdhci {
 	cap-mmc-highspeed;
-	mmc-ddr-1_8v;
 	mmc-hs200-1_8v;
 	mmc-hs400-1_8v;
 	mmc-hs400-enhanced-strobe;
diff --git a/arch/arm/dts/rk3568-radxa-e25-u-boot.dtsi b/arch/arm/dts/rk3568-radxa-e25-u-boot.dtsi
index caf524443079..74755a44eaee 100644
--- a/arch/arm/dts/rk3568-radxa-e25-u-boot.dtsi
+++ b/arch/arm/dts/rk3568-radxa-e25-u-boot.dtsi
@@ -16,7 +16,6 @@

 &sdhci {
 	cap-mmc-highspeed;
-	mmc-ddr-1_8v;
 	mmc-hs200-1_8v;
 	mmc-hs400-1_8v;
 	mmc-hs400-enhanced-strobe;
diff --git a/arch/arm/dts/rk3568-rock-3a-u-boot.dtsi b/arch/arm/dts/rk3568-rock-3a-u-boot.dtsi
index 46ebb77283f1..5b823fcca5fb 100644
--- a/arch/arm/dts/rk3568-rock-3a-u-boot.dtsi
+++ b/arch/arm/dts/rk3568-rock-3a-u-boot.dtsi
@@ -20,7 +20,6 @@

 &sdhci {
 	cap-mmc-highspeed;
-	mmc-ddr-1_8v;
 	mmc-hs200-1_8v;
 	mmc-hs400-1_8v;
 	mmc-hs400-enhanced-strobe;
diff --git a/arch/arm/dts/rk3588-rock-5b-u-boot.dtsi b/arch/arm/dts/rk3588-rock-5b-u-boot.dtsi
index e99e60185ebe..9ee9dd051e32 100644
--- a/arch/arm/dts/rk3588-rock-5b-u-boot.dtsi
+++ b/arch/arm/dts/rk3588-rock-5b-u-boot.dtsi
@@ -31,7 +31,6 @@

 &sdhci {
 	cap-mmc-highspeed;
-	mmc-ddr-1_8v;
 	mmc-hs200-1_8v;
 };
diff --git a/arch/arm/dts/rk3588-turing-rk1-u-boot.dtsi b/arch/arm/dts/rk3588-turing-rk1-u-boot.dtsi
index 471508a9ed74..ca2a684f3541 100644
--- a/arch/arm/dts/rk3588-turing-rk1-u-boot.dtsi
+++ b/arch/arm/dts/rk3588-turing-rk1-u-boot.dtsi
@@ -8,7 +8,6 @@

 &sdhci {
 	cap-mmc-highspeed;
-	mmc-ddr-1_8v;
 	mmc-hs200-1_8v;
 };
diff --git a/arch/arm/dts/rk3588s-rock-5a-u-boot.dtsi b/arch/arm/dts/rk3588s-rock-5a-u-boot.dtsi
index 9a6a353088df..efba0c359ba5 100644
--- a/arch/arm/dts/rk3588s-rock-5a-u-boot.dtsi
+++ b/arch/arm/dts/rk3588s-rock-5a-u-boot.dtsi
@@ -7,6 +7,5 @@

 &sdhci {
 	cap-mmc-highspeed;
-	mmc-ddr-1_8v;
 	mmc-hs200-1_8v;
 };
```

### 1.2 `[PATCH v2 2/3] rockchip: rk35xx: Enable eMMC HS200 mode by default`

```diff
diff --git a/arch/arm/mach-rockchip/Kconfig b/arch/arm/mach-rockchip/Kconfig
index 6ff0aa6911e2..946ef5d7023d 100644
--- a/arch/arm/mach-rockchip/Kconfig
+++ b/arch/arm/mach-rockchip/Kconfig
@@ -292,6 +292,8 @@ config ROCKCHIP_RK3568
 	imply OF_LIBFDT_OVERLAY
 	imply ROCKCHIP_OTP
 	imply MISC_INIT_R
+	imply MMC_HS200_SUPPORT if MMC_SDHCI_ROCKCHIP
+	imply SPL_MMC_HS200_SUPPORT if SPL_MMC && MMC_HS200_SUPPORT
 	help
 	  The Rockchip RK3568 is a ARM-based SoC with quad-core Cortex-A55,
@@ -317,6 +319,8 @@ config ROCKCHIP_RK3588
 	imply OF_LIBFDT_OVERLAY
 	imply ROCKCHIP_OTP
 	imply MISC_INIT_R
+	imply MMC_HS200_SUPPORT if MMC_SDHCI_ROCKCHIP
+	imply SPL_MMC_HS200_SUPPORT if SPL_MMC && MMC_HS200_SUPPORT
 	imply CLK_SCMI
 	imply SCMI_FIRMWARE
 	help
diff --git a/configs/nanopi-r5c-rk3568_defconfig b/configs/nanopi-r5c-rk3568_defconfig
index 833cff0e457d..f5a472d03d78 100644
--- a/configs/nanopi-r5c-rk3568_defconfig
+++ b/configs/nanopi-r5c-rk3568_defconfig
@@ -58,8 +58,6 @@ CONFIG_ROCKCHIP_GPIO=y
 CONFIG_SYS_I2C_ROCKCHIP=y
 CONFIG_MISC=y
 CONFIG_SUPPORT_EMMC_RPMB=y
-CONFIG_MMC_HS200_SUPPORT=y
-CONFIG_SPL_MMC_HS200_SUPPORT=y
 CONFIG_MMC_DW=y
 CONFIG_MMC_DW_ROCKCHIP=y
 CONFIG_MMC_SDHCI=y
diff --git a/configs/nanopi-r5s-rk3568_defconfig b/configs/nanopi-r5s-rk3568_defconfig
index 2736d382a352..99692d341f44 100644
--- a/configs/nanopi-r5s-rk3568_defconfig
+++ b/configs/nanopi-r5s-rk3568_defconfig
@@ -58,8 +58,6 @@ CONFIG_ROCKCHIP_GPIO=y
 CONFIG_SYS_I2C_ROCKCHIP=y
 CONFIG_MISC=y
 CONFIG_SUPPORT_EMMC_RPMB=y
-CONFIG_MMC_HS200_SUPPORT=y
-CONFIG_SPL_MMC_HS200_SUPPORT=y
 CONFIG_MMC_DW=y
 CONFIG_MMC_DW_ROCKCHIP=y
 CONFIG_MMC_SDHCI=y
diff --git a/configs/radxa-e25-rk3568_defconfig b/configs/radxa-e25-rk3568_defconfig
index 5a613abe0d2d..fedb137877ab 100644
--- a/configs/radxa-e25-rk3568_defconfig
+++ b/configs/radxa-e25-rk3568_defconfig
@@ -60,8 +60,6 @@ CONFIG_ROCKCHIP_GPIO=y
 CONFIG_SYS_I2C_ROCKCHIP=y
 CONFIG_MISC=y
 CONFIG_SUPPORT_EMMC_RPMB=y
-CONFIG_MMC_HS200_SUPPORT=y
-CONFIG_SPL_MMC_HS200_SUPPORT=y
 CONFIG_MMC_DW=y
 CONFIG_MMC_DW_ROCKCHIP=y
 CONFIG_MMC_SDHCI=y
```

### 1.2.1 补丁 1/2 的适用性

这两个补丁只影响 U-Boot 的模式选择：

- 补丁 1 删除 U-Boot board DTS 的 `mmc-ddr-1_8v`，避免 U-Boot 用 DDR52 写 eMMC；
- 补丁 2 在 RK3568/RK3588 的 Kconfig 中默认启用 HS200。

Linux 侧当前 DTS 没有 `mmc-ddr-1_8v`，HS200 已正常工作，HS400ES 由 `mmc_select_hs400es()` 选择；因此补丁 1/2 **没有可直接移植到 Linux HS400ES 的代码修改**。
它们的价值是上下文证据：RK35xx 的慢速/DDR52 写不可靠，HS200 是可靠基线；但它们不解释 HS400ES @100MHz 的首笔 DATA_CRC。

### 1.3 `[PATCH v2 3/3] mmc: rockchip_sdhci: Fix HS400 mode write on RK3568`

完整原始 diff：

```diff
diff --git a/drivers/mmc/rockchip_sdhci.c b/drivers/mmc/rockchip_sdhci.c
index 285332d9f4fd..706fb1235796 100644
--- a/drivers/mmc/rockchip_sdhci.c
+++ b/drivers/mmc/rockchip_sdhci.c
@@ -71,7 +71,6 @@
 #define DLL_RXCLK_NO_INVERTER		BIT(29)
 #define DLL_RXCLK_ORI_GATE		BIT(31)
 #define DLL_TXCLK_TAPNUM_DEFAULT	0x10
-#define DLL_TXCLK_TAPNUM_90_DEGREES	0x9
 #define DLL_TXCLK_TAPNUM_FROM_SW	BIT(24)
 #define DLL_TXCLK_NO_INVERTER		BIT(29)
 #define DLL_STRBIN_TAPNUM_DEFAULT	0x4
@@ -314,8 +313,10 @@ static int rk3568_sdhci_config_dll(struct sdhci_host *host, u32 clock, bool enab
 	int val, ret;
 	u32 extra, txclk_tapnum;

-	if (!enable)
+	if (!enable) {
+		sdhci_writel(host, 0, DWCMSHC_EMMC_DLL_CTRL);
 		return 0;
+	}

 	if (clock >= 100 * MHz) {
 		/* reset DLL */
@@ -648,7 +649,7 @@ static const struct sdhci_data rk3568_data = {
 	.config_dll = rk3568_sdhci_config_dll,
 	.flags = FLAG_INVERTER_FLAG_IN_RXCLK,
 	.hs200_txclk_tapnum = DLL_TXCLK_TAPNUM_DEFAULT,
-	.hs400_txclk_tapnum = DLL_TXCLK_TAPNUM_DEFAULT,
+	.hs400_txclk_tapnum = 0x8,
 };

 static const struct sdhci_data rk3588_data = {
@@ -656,7 +657,7 @@ static const struct sdhci_data rk3588_data = {
 	.set_clock = rk3568_sdhci_set_clock,
 	.config_dll = rk3568_sdhci_config_dll,
 	.hs200_txclk_tapnum = DLL_TXCLK_TAPNUM_DEFAULT,
-	.hs400_txclk_tapnum = DLL_TXCLK_TAPNUM_90_DEGREES,
+	.hs400_txclk_tapnum = 0x9,
 };
```

---

## 2. 第三个补丁逐项解释

### 2.1 修改 1：disable 时停止 DLL

修改前：

```c
if (!enable)
        return 0;
```

修改后：

```c
if (!enable) {
        sdhci_writel(host, 0, DWCMSHC_EMMC_DLL_CTRL);
        return 0;
}
```

- `DWCMSHC_EMMC_DLL_CTRL` 是 DLL 控制寄存器（offset 0x800）。
- 写 `0` 会停止 DLL，而不是重新锁定/改 tap。
- 之前 `config_dll(enable=false)` 直接返回，DLL 可能仍在运行；之后在关闭高速时钟/模式时会显式停止 DLL。
- 这个改动是生命周期/模式切换修复，不直接改变 HS400/ES 的 TX tap 值。

### 2.2 修改 2：RK3568 HS400 TX tap 0x10 -> 0x8

```c
rk3568_data:
-  .hs400_txclk_tapnum = DLL_TXCLK_TAPNUM_DEFAULT;  /* 0x10 */
+  .hs400_txclk_tapnum = 0x8;
```

- `hs200_txclk_tapnum` 保持 `DLL_TXCLK_TAPNUM_DEFAULT` = 0x10；
- HS200 不受影响；
- 只有 `selected_mode == MMC_HS_400 || selected_mode == MMC_HS_400_ES` 时使用 `hs400_txclk_tapnum`；
- 因此 HS400 和 HS400ES 都受影响，且是同一个值；
- 该值最终写入 `DWCMSHC_EMMC_DLL_TXCLK` 的低 8 位：
  - 修改前 `TXCLK=0x29000010`；
  - 修改后 `TXCLK=0x29000008`。

### 2.3 修改 3：RK3588 宏替换为字面量

```c
- #define DLL_TXCLK_TAPNUM_90_DEGREES 0x9
...
- .hs400_txclk_tapnum = DLL_TXCLK_TAPNUM_90_DEGREES,
+ .hs400_txclk_tapnum = 0x9,
```

- RK3588 的值仍是 `0x9`，行为不变；
- 只是移除一个仅 RK3588 使用的宏；
- 不应把这个 `0x9` 套到 RK3568。

### 2.4 补丁没有修改什么

第三补丁**没有**：

- 修改 RXCLK；
- 修改 STRBIN；
- 修改 CMDOUT；
- 修改 DLL 锁定/起始点/增量；
- 修改 HS200 tap；
- 修改 DDR52/HS 模式。

U-Boot 在补丁前已经写 CMDOUT（`DLL_CMDOUT_TAPNUM_90_DEGREES`）和 STRBIN=4；第三补丁只动 TX tap 和 disable 时的 DLL stop。

---

## 3. 对 HS400 与 HS400ES 的适用性

### 3.1 U-Boot

```c
txclk_tapnum = data->hs200_txclk_tapnum;
if (mmc->selected_mode == MMC_HS_400 ||
    mmc->selected_mode == MMC_HS_400_ES) {
        txclk_tapnum = data->hs400_txclk_tapnum;
        ...
}
```

源码：`u-boot-mainline-src/drivers/mmc/rockchip_sdhci.c:368-371`。

**结论：**

- 对 **HS400**：使用 tap 8；
- 对 **HS400ES**：使用 tap 8；
- 对 **HS200**：仍使用 0x10；
- 对 DDR52/HS：不使用该 tap。

补丁提交信息说 “HS400 modes”，与代码一致，包括 HS400ES。

### 3.2 Linux

Linux 没有 `MMC_HS_400_ES` timing 枚举；HS400ES 通过：

```text
MMC_TIMING_MMC_HS400
+ host->ios.enhanced_strobe = true
```

设置：

`drivers/mmc/core/mmc.c:1453-1459`。

所以 Linux 的可移植修复应：

```c
if (host->mmc->ios.timing == MMC_TIMING_MMC_HS400)
        txclk_tapnum = priv->hs400_txclk_tapnum;
else
        txclk_tapnum = priv->txclk_tapnum;
```

一个 HS400 分支同时覆盖 HS400 与 HS400ES，保持 HS200 为 0x10。

---

## 4. 当前项目 U-Boot 是否包含补丁

### 4.1 版本与提交

本地源码：

```text
/home/hsy/project/rea-z97a/u-boot-mainline-src
git describe: v2026.01-63-g10942055fd
```

相关提交在 `HEAD` 历史中：

```text
ebe293bae7 rockchip: rk35xx: Remove use of eMMC DDR52 mode
6de9d7b2f1 rockchip: rk35xx: Enable eMMC HS200 mode by default
5c053f3adc mmc: rockchip_sdhci: Fix HS400 mode write on RK3568
```

`5c053f3adc` 的 diff 与 2024-02 邮件完全一致（上述 9 行改动），说明补丁已合入且未被替代。

### 4.2 当前 U-Boot RK3568 数据

```text
u-boot-mainline-src/drivers/mmc/rockchip_sdhci.c:690-700
rk3568_data:
  .flags = FLAG_INVERTER_FLAG_IN_RXCLK
  .hs200_txclk_tapnum = DLL_TXCLK_TAPNUM_DEFAULT (0x10)
  .hs400_txclk_tapnum = 0x8
  .hs400_cmdout_tapnum = DLL_CMDOUT_TAPNUM_90_DEGREES (0x8)
  .hs400_strbin_tapnum = DLL_STRBIN_TAPNUM_DEFAULT (0x4)
  .ddr50_strbin_delay_num = DLL_STRBIN_DELAY_NUM_DEFAULT (0x10)
```

`hs400_cmdout_tapnum` / `hs400_strbin_tapnum` 是后续 `d64144a467`“Extend variant configuration”添加的；第三个补丁当时使用固定宏，后来被扩展成 driver data，但 RK3568 的 TX tap 仍是 0x8，未回退。

### 4.3 当前 U-Boot 实际写出的寄存器

当前 `rk3568_sdhci_config_dll`（`:319-393`）：

```text
RXCLK:
  DLYENA | ORI_GATE | NO_INVERTER
  = 0x08000000 | 0x80000000 | 0x20000000 = 0xA8000000

TXCLK (HS400/ES):
  DLYENA | FROM_SW | TX source-select | tap 8
  = 0x08000000 | 0x01000000 | 0x20000000 | 0x8 = 0x29000008

CMDOUT (HS400/ES):
  SRC_CLK_NEG | BOTH_CLK_EDGE | DLYENA | tap 8 | FROM_SW
  = 0x10000000 | 0x40000000 | 0x08000000 | 0x8 | 0x01000000 = 0x59000008

STRBIN:
  DLYENA | tap 4 | FROM_SW
  = 0x09000004
```

当前 U-Boot 在 `hs400_cmdout_tapnum` 上写 0x8，并且 `CMDOUT` 对 HS400/HS400ES 都写；Linux revision 0 不写 CMDOUT。

---

## 5. 与 Linux 7.2.12 对比

### 5.1 当前 Linux 配置

```text
sdhci-of-dwcmshc.c:828-830  RX rev0: DLYENA | NO_INVERTER = 0x28000000
sdhci-of-dwcmshc.c:850-852  HS200/HS400 共用 priv->txclk_tapnum
sdhci-of-dwcmshc.c:865-869  TX: DLYENA | FROM_SW | bit29 | tap 0x10 = 0x29000010
sdhci-of-dwcmshc.c:871-874  STRBIN: DLYENA | FROM_SW | tap 4 = 0x09000004
sdhci-of-dwcmshc.c:854-863  revision 1 才写 90° TX + CMDOUT；rev0 不写
```

与本次故障寄存器完全对应：

```text
TXCLK  = 0x29000010
RXCLK  = 0x28000000
STRBIN = 0x09000004
CMDOUT = 0
DLL    = 0x50201
STAT0  = 0x171
```

### 5.2 差异表

| 项 | Linux 7.2.12 | U-Boot v2026.01 | 差异来源 | 与本次 DATA_CRC 的关系 |
|---|---|---|---|---|
| HS400/ES TX tap | 0x10 (`0x29000010`) | 0x8 (`0x29000008`) | 第三补丁 `5c053f3adc` | **最直接**；写方向 TX 数据时序 |
| RXCLK | `0x28000000`（无 ORI_GATE） | `0xA8000000`（含 ORI_GATE） | U-Boot 旧代码/后续配置 | 可能影响 CRC status/读采样，证据弱于 TX |
| CMDOUT | `0` | `0x59000008` | U-Boot 旧代码 + `d64144a467` | CMD25 命令阶段无 CMD 错误；对 DATA_CRC 证据弱 |
| STRBIN | `0x09000004` | `0x09000004` | 相同 | 无差异 |
| HS200 TX tap | 0x10 | 0x10 | 相同 | HS200 正常 |
| DLL stop on disable | `set_clock(0)` 只关 clock | `config_dll(false)` 写 0 停 DLL | 第三补丁 | 模式切换生命周期，非首写 DATA_CRC |

### 5.3 是否存在相同问题

**是，至少参数层面相同：**

- Linux rev0 的 HS400/ES TX tap 仍是 0x10；
- U-Boot 的修复把 RK3568 HS400/ES TX tap 改为 0x8；
- 本次 Linux 首错 DATA_CRC 正是 HS400/ES 写方向；
- Linux 没有独立的 HS400 tap 字段，无法表达 U-Boot 的“HS200=0x10、HS400=0x8”。

### 5.4 可移植逻辑

推荐的最小移植：

```c
struct rk35xx_priv {
        ...
        u8 txclk_tapnum;       /* HS200/default */
        u8 hs400_txclk_tapnum; /* HS400/ES */
};

/* rk3568 pdata */
.hs400_txclk_tapnum = 0x8;

/* set_clock */
if (host->mmc->ios.timing == MMC_TIMING_MMC_HS400)
        txclk_tapnum = priv->hs400_txclk_tapnum;
else
        txclk_tapnum = priv->txclk_tapnum;
```

不要：

- 把 RK3588 的 0x9/0xA 套到 RK3568；
- 同时改 RX/STRBIN/CMDOUT；
- 用 `rockchip,txclk-tapnum` 全局改 HS200/HS400 共用值（会污染 HS200 基线；除非只是临时实验）。

---

## 6. 对本次真实故障的适用性判断

本次：

```text
HS400ES @100MHz
DATA_CRC
DLL LOCKED
TXCLK=0x29000010
RXCLK=0x28000000
STRBIN=0x09000004
```

### 6.1 直接对应

- `TXCLK=0x29000010` 是 U-Boot **修复前**的 HS400 tap 0x10；
- U-Boot 修复后是 `0x29000008`；
- 本次首错是写方向 DATA_CRC；
- 因此 **TX tap 8 是最明确的下一轮单参数方向**。

### 6.2 不能直接对应

- U-Boot 第三补丁的验证上下文是 HS400/HS400ES **200MHz**；本次是 **100MHz**。
  tap 是 DLL delay step，相位随 clock 改变；0x8 在 100MHz 不一定等价于 200MHz 的 0x8。
- U-Boot 的 `CMDOUT=0x59000008`、`RXCLK=0xA8000000` 不是第三补丁新增的；它们是当前 U-Boot 的其他上下文差异。
  本次首错没有 CMD 错误，CMDOUT 主要影响命令输出，因此不能把它排在第一。
- `DLL LOCKED=1` 只证明锁定，不证明 tap 相位窗口正确；U-Boot 的 tap 8 修复正是“锁定但相位不合适”的已知案例。

### 6.3 是否提供了明确方向

**是，但仅限一个方向：**

- 明确：**HS400/ES TXCLK tap=0x8**；
- 不明确：RXCLK ORI_GATE、CMDOUT、STRBIN、DLL stop 是否也是 Linux 必需；
- 不明确：100MHz 下 tap 8 是否有效；
- 不明确：U-Boot 的 PIO/写路径与 Linux ADMA 写路径的差异是否影响。

因此下一轮应先做“单变量 TX tap 8”，而不是同时套用 U-Boot 全部寄存器。

---

## 7. 下一轮实验建议（与根因报告一致）

详细方案见 `docs/HS400_DATA_CRC_ROOT_CAUSE_ANALYSIS.md`。这里给出 U-Boot 审计后的优先级：

1. **A：只读基线**
   - 增加 `DLL_STATUS1`、`ADMA_ERROR` 前后只读快照；不改 PHY。
2. **B1：TXCLK tap 0x8**
   - 只改 HS400/ES，HS200 保持 0x10；
   - 100MHz 先测；一次 512MiB 写；首错即停；
   - 通过则 3 次冷启动 + A/B 回退（恢复 0x10 确认错误回来）。
3. **B2：CMDOUT 0x8**（仅 B1 失败）
   - U-Boot 当前值；但命令阶段无错，证据弱。
4. **B3：RXCLK ORI_GATE bit31**（仅 B1/B2 失败）
   - U-Boot 当前值；可能影响 CRC status/读采样。
5. **B4：DLL stop-on-disable**
   - 模式切换/时钟切换稳定性，不作为首写 DATA_CRC 首选项。
6. 不扫描 STRBIN/TX tap；不套 RK3588 参数；不同时改多个 PHY 参数。

---

## 8. 可信度与限制

| 结论 | 来源 | 可信度 |
|---|---|---|
| 第三补丁原始 diff | u-boot 邮件归档 + 本地 commit `5c053f3adc` | 高 |
| 当前 U-Boot 包含补丁且 TX tap=0x8 | 本地 `v2026.01-63` 源码/commit | 高 |
| 补丁影响 HS400 和 HS400ES，不影响 HS200 | 当前 U-Boot `selected_mode` 条件 | 高 |
| 补丁验证在 200MHz | 2023/2024 U-Boot 提交信息、U-Boot mode 表 | 高 |
| Linux 当前 TX tap=0x10 | Linux 7.2.12 源码 + 实测寄存器 | 高 |
| vendor BSP `hs400_tx_tap=8` | GitHub vendor 镜像 | 中 |
| Linux 100MHz 下 tap 8 是否有效 | 无实验 | 未验证 |
| RX/CMDOUT 是否需要移植 | 当前 U-Boot 差异，但非补丁 3 | 中低 |
| RK3568 TRM 的 DLL vendor 位定义 | 公开 TRM 未完整核验 | 低 |

**限制：**

- patchwork.ozlabs.org 被 Anubis 防护；使用邮件归档原始 diff。
- 本轮不编译、不刷写、不执行 U-Boot 或 Linux 硬件测试。
- 当前 U-Boot 是预编译部署；源码审计基于本仓库 `u-boot-mainline-src`，未做二进制反汇编确认。
- 100MHz 与 200MHz 的 tap 相位关系未建模/未实测。

---

## 9. 引用

- U-Boot 系列 cover：
  `https://lists.u-boot-project.org/pipermail/u-boot/2024-February/545267.html`
- 1/3：
  `https://lists.u-boot-project.org/pipermail/u-boot/2024-February/545269.html`
- 2/3：
  `https://lists.u-boot-project.org/pipermail/u-boot/2024-February/545268.html`
- 3/3：
  `https://lists.u-boot-project.org/pipermail/u-boot/2024-February/545270.html`
- 本仓库 U-Boot：
  `u-boot-mainline-src`，`v2026.01-63-g10942055fd`
  - `drivers/mmc/rockchip_sdhci.c:327-330` stop DLL
  - `:361-366` RXCLK
  - `:368-371` HS400/ES tap
  - `:373-380` CMDOUT
  - `:382-387` TXCLK
  - `:389-393` STRBIN
  - `:690-700` rk3568_data
- 相关提交：
  - `ebe293bae7` rk35xx remove DDR52
  - `6de9d7b2f1` rk35xx enable HS200
  - `5c053f3adc` fix HS400 write on RK3568
  - `d64144a467` extend variant configuration
- Linux：
  - `sources/linux/drivers/mmc/host/sdhci-of-dwcmshc.c:828-874`, `2125-2138`
  - `sources/linux/drivers/mmc/core/mmc.c:1453-1459`
- 现有证据：
  - `docs/HS400_FIRST_WRITE_DIAG_REPORT.md`
  - `build/out/dwcmshc-diag/hs400/`
- 交叉参考：
  - Barebox revision 0/STRBIN 补丁：
    `https://www.mail-archive.com/barebox@lists.infradead.org/msg58719.html`
    `http://lists.infradead.org/pipermail/barebox/2026-May/056570.html`
  - vendor BSP 镜像：
    `https://raw.githubusercontent.com/li-chiga/RK3588-machine-learning/main/kernel/drivers/mmc/host/sdhci-of-dwcmshc.c`
