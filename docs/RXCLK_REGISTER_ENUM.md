# RXCLK 寄存器位域与取值枚举（只读）

日期：2026-11-14
平台：RK3568 / DWCMSHC
本轮只读枚举，未写 RXCLK 寄存器，未执行硬件扫描。

---

## 1. 寄存器

```text
DWCMSHC_EMMC_DLL_RXCLK = 0x804
```

当前项目 Linux 7.2.12 实测值：

| 模式 | 频率 | RXCLK |
|---|---:|---:|
| HS200 | 200MHz | `0x28000000` |
| HS400ES | 100MHz | `0x28000000` |
| HS400ES | 200MHz | `0x28000000` |

即三者的 RXCLK 完全相同。

---

## 2. 位域定义

字段定义来自 OpenBSD `dwmshc.c`（RK3568 公开实现，2023）：

```text
bits[4:0]    RX_TAP_NUM             0x00–0x1F   (5-bit)
bits[15:8]   RX_TAP_VALUE           0x00–0xFF   (8-bit)
bits[23:16]  RX_DELAY_NUM           0x00–0xFF   (8-bit)
bit24        RX_TAP_NUM_SEL
bit25        RX_TAP_VALUE_SEL
bit26        RX_DELAY_NUM_SEL
bit27        RX_CLK_OUT_SEL         (Linux 称为 DLYENA)
bit28        RX_CLK_CHANGE_WINDOW
bit29        RX_CLK_SRC_SEL         (NO_INVERTER/INVERTER select)
bit31        ORI_GATE
```

Linux 侧对应：

```text
DWCMSHC_EMMC_DLL_RXCLK_SRCSEL = 29
DLL_RXCLK_NO_INVERTER          = 1
DLL_RXCLK_ORI_GATE             = BIT(31)
DLL_RXCLK_DLYENA               = BIT(27)
```

---

## 3. 当前 `0x28000000` 的解码

```text
0x28000000 = bit27 | bit29
  bit27 = 1  RX_CLK_OUT_SEL / DLYENA
  bit29 = 1  RX_CLK_SRC_SEL = NO_INVERTER
  tap_num   = 0
  tap_value = 0
  delay_num = 0
  bit24/25/26 = 0（未选 software tap/value/delay）
  bit28 = 0
  bit31 = 0（ORI_GATE 关闭）
```

这同时是：

- Linux 7.2.12 rev0 的默认；
- OpenBSD `dwmshc` 的默认；
- 当前 HS200/HS400ES 实测值。

---

## 4. 已知的 RXCLK 取值

| 值 | 来源/含义 |
|---|---|
| `0x00000000` | DLL/legacy 关闭；OpenBSD legacy 路径写 0 |
| `0x08000000` | 仅 `RX_CLK_OUT_SEL`/DLYENA |
| `0x20000000` | 仅 source select |
| `0x28000000` | **当前**：OUT_SEL + NO_INVERTER |
| `0xA8000000` | U-Boot 当前高速路径：`ORI_GATE + OUT_SEL + NO_INVERTER` |
| `0x80000000` | Linux `<=52MHz` 路径：仅 `ORI_GATE` |

U-Boot 源码：

```text
drivers/mmc/rockchip_sdhci.c:361-366
  extra = DLYENA | ORI_GATE;
  if (FLAG_INVERTER_FLAG_IN_RXCLK)
      extra |= DLL_RXCLK_NO_INVERTER;
  // = 0xA8000000
```

Linux `<=52MHz`：

```text
sdhci-of-dwcmshc.c:803
  sdhci_writel(host, DLL_RXCLK_ORI_GATE, DWCMSHC_EMMC_DLL_RXCLK);
  // = 0x80000000
```

---

## 5. 可选的软件 tap/delay 组合

如果硬件使用 software tap 选择位，合法组合可按字段拼装：

```text
base = 0x28000000

tap num 选中：
  0x29000000 | (tap_num & 0x1F)
  范围 0x29000000–0x2900001F

tap value 选中：
  0x2A000000 | ((tap_value & 0xFF) << 8)
  范围 0x2A000000–0x2A00FF00

delay num 选中：
  0x2C000000 | ((delay_num & 0xFF) << 16)
  范围 0x2C000000–0x2CFF0000

再叠加 ORI_GATE：
  |= 0x80000000
```

注意：这些是位域组合，不代表都已经验证可用。
当前 Linux、U-Boot、Rockchip vendor BSP 的常规路径均未写 RX tap；HS400ES 读采样主要使用 STRBIN/DS。

---

## 6. 结论

- RXCLK tap 字段是 5-bit（0x00–0x1F），tap value 是 8-bit；
- 但当前 Linux/U-Boot/BSP 都不使用 RX tap；
- 实测 HS200 200MHz、HS400ES 100MHz、HS400ES 200MHz 均为 `0x28000000`；
- 当前唯一差异是 U-Boot 额外设置 `ORI_GATE`（`0xA8000000`），Linux 没有；
- 本轮没有写 RXCLK，没有扫描 0x00–0x1F，也没有改变任何 PHY 参数。

---

## 7. 来源

- Linux：`drivers/mmc/host/sdhci-of-dwcmshc.c:90,100,113,116,803,827-830`
- U-Boot：`drivers/mmc/rockchip_sdhci.c:361-366`
- OpenBSD `dwmshc.c`：
  `http://www.mail-archive.com/tech@openbsd.org/msg74447.html`
- 现有实测：`build/out/dwcmshc-diag/tap08/`、`build/out/dwcmshc-diag/scan/`
