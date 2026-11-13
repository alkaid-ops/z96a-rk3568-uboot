# STRBIN 寄存器位域与取值枚举（只读）

日期：2026-11-14
平台：RK3568 / DWCMSHC
本轮只读枚举，未写 STRBIN，未执行硬件扫描。

---

## 1. 寄存器

```text
DWCMSHC_EMMC_DLL_STRBIN = 0x80c
```

当前项目 Linux 7.2.12 实测值：

| 模式 | 频率 | STRBIN |
|---|---:|---:|
| HS200 | 200MHz | `0x09000004` |
| HS400ES | 100MHz | `0x09000004` |
| HS400ES | 200MHz | `0x09000004` |
| HS400ES 200MHz TXCLK 扫描所有 tap | 0x04–0x10 | `0x09000004` |

即所有已测 HS200/HS400ES 配置的 STRBIN 完全相同。

---

## 2. 位域定义

字段定义来自 OpenBSD `dwmshc.c`（RK3568 公开实现，2023）：

```text
bits[4:0]    STRBIN_TAP_NUM          0x00–0x1F   (5-bit)
bits[15:8]   STRBIN_TAP_VALUE        0x00–0xFF   (8-bit)
bits[23:16]  STRBIN_DELAY_NUM        0x00–0xFF   (8-bit)
bit24        STRBIN_TAP_NUM_SEL
bit25        STRBIN_TAP_VALUE_SEL
bit26        STRBIN_DELAY_NUM_SEL
bit27        STRBIN_DELAY_ENA
```

Linux 对应宏：

```text
DLL_STRBIN_TAPNUM_DEFAULT      = 0x4
DLL_STRBIN_TAPNUM_FROM_SW      = BIT(24)
DLL_STRBIN_DELAY_NUM_SEL       = BIT(26)
DLL_STRBIN_DELAY_NUM_OFFSET    = 16
DLL_STRBIN_DELAY_NUM_DEFAULT   = 0x16
DWCMSHC_EMMC_DLL_DLYENA        = BIT(27)
```

---

## 3. 当前 `0x09000004` 解码

```text
0x09000004 = bit27 | bit24 | 0x4
  bit27 = 1  DELAY_ENA
  bit24 = 1  TAP_NUM_SEL
  tap_num = 4
  tap_value = 0
  delay_num = 0
  bit25/26 = 0
```

这同时是：

- Linux 7.2.12 高速路径默认；
- 当前 U-Boot v2026.01 RK3568 配置；
- Rockchip vendor BSP RK3568 `hs400_strbin_tap=4`。

---

## 4. 已知的 STRBIN 取值

| 值 | 来源/含义 |
|---|---|
| `0x00000000` | DLL/legacy 关闭；OpenBSD legacy 路径写 0 |
| `0x08000000` | 仅 `DELAY_ENA` |
| `0x09000004` | **当前**：`DELAY_ENA + TAP_NUM_SEL + tap 4` |
| `0x09000008` | OpenBSD 默认：`DELAY_ENA + TAP_NUM_SEL + tap 8` |
| `0x0C160000` | Linux `<=52MHz`：`DELAY_ENA + DELAY_NUM_SEL + delay 0x16` |

Linux 低时钟路径：

```text
sdhci-of-dwcmshc.c:927-929
  extra = DLYENA |
          DLL_STRBIN_DELAY_NUM_SEL |
          DLL_STRBIN_DELAY_NUM_DEFAULT << 16;
  // = 0x0C160000
```

Linux 高速路径：

```text
sdhci-of-dwcmshc.c:991-993
  extra = DLYENA |
          DLL_STRBIN_TAPNUM_DEFAULT |
          DLL_STRBIN_TAPNUM_FROM_SW;
  // = 0x09000004
```

---

## 5. 可选的软件 tap/delay 组合

如果使用软件 tap/delay 字段，按位域组合：

```text
base = 0x08000000

tap_num 选中：
  0x09000000 | (tap_num & 0x1F)
  0x09000000 – 0x0900001F

tap_value 选中：
  0x0A000000 | ((tap_value & 0xFF) << 8)
  0x0A000000 – 0x0A00FF00

delay_num 选中：
  0x0C000000 | ((delay_num & 0xFF) << 16)
  0x0C000000 – 0x0CFF0000
```

这些只是位域组合，不代表已验证可用。当前 Linux/U-Boot/BSP 常规路径只使用 `tap 4`。

---

## 6. HS400ES 相关性

HS400ES 的读采样使用 eMMC 的 data strobe（DS），STRBIN 是 DWCMSHC 的 strobe input 采样控制寄存器；因此它比 RXCLK tap 更直接参与 ES 读路径。

当前 STRBIN：

```text
tap = 4
tap_num_sel = 1
delay_ena = 1
delay_num = 0
```

---

## 7. 结论

- STRBIN tap 字段是 5-bit（0x00–0x1F）；
- 当前 Linux/U-Boot/BSP 均使用 `tap 4`，实际值 `0x09000004`；
- OpenBSD 默认使用 `tap 8`，值 `0x09000008`；
- Linux 低时钟路径使用 delay 0x16，值 `0x0C160000`；
- 已测 HS200 200MHz、HS400ES 100MHz、HS400ES 200MHz 及所有 TX tap 均为 `0x09000004`；
- 本轮没有写 STRBIN，没有扫描 0x00–0x1F，也没有改变任何 PHY 参数。

---

## 8. 来源

- Linux：`drivers/mmc/host/sdhci-of-dwcmshc.c:108-112,927-929,991-993`
- U-Boot：`drivers/mmc/rockchip_sdhci.c:389-393`
- OpenBSD `dwmshc.c`：
  `http://www.mail-archive.com/tech@openbsd.org/msg74447.html`
- 现有实测：`build/out/dwcmshc-diag/tap08/`、`build/out/dwcmshc-diag/scan/`
