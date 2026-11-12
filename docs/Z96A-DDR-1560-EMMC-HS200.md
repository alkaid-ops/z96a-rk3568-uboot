# Z96A DDR 1560MHz 对齐与 eMMC HS200/HS400 结论

日期：2026-11-11

## 1. 结论摘要

- DRAM：Rayson **RS1G32LO4D2BDS-53BT**，LPDDR4X，x32，3733Mbps，4GB。
- 原厂固件：DDR init 为 **v1.13，1560MHz**；eMMC 为 **HS200（max 200MHz），没有启用 HS400**。
- 我们之前 eMMC 上的 A idbloader 用的是 Android 归档里的自定义 **v1.25 1184MHz** blob，属于降频，与原厂不一致。
- 现已把 eMMC 扇区 `0x40` 的 idbloader 换成原厂 v1.13 DDR blob（1560MHz）+ 原 A SPL，并成功启动验证。
- eMMC 写入在 HS400 下会报 I/O error；原厂、mainline eMMC Linux 都用 HS200，USB Devuan 的 DTB 是唯一启用 HS400 的异常配置。

## 2. 原厂 DDR 证据

### 2.1 原厂 idbloader 里的 DDR blob

文件：`build/vendor-v1/vendor-idbloader.bin`

```text
偏移 0x800..0xF000：原厂 DDR 初始化 blob，59,392 字节
字符串：DDR Version V1.13 20220218
SHA-256：763947ffa9da78c01401ad679bb7648ead920e03cf07d71bfc59bc24a685d9e3
```

### 2.2 原厂 Android 内核 DTS

文件：`docs/ANDROID-archive-rk-kernel.dts`

```dts
dmc-opp-table {
    opp-1560000000 {
        opp-hz = <0x0 0x5cfbb600>;   // 1,560,000,000 Hz
        opp-microvolt = <0xdbba0>;   // 0.9V
    };
};

lpddr4x-params {
    freq_0 = <0x618>;   // 1560 MHz
    freq_1 = <0x144>;   // 324
    freq_2 = <0x210>;   // 528
    freq_3 = <0x30c>;   // 780
    pd_dis_freq = <0x42a>;   // 1066
    sr_dis_freq = <0x320>;   // 800
    dram_dq_odt_en_freq = <0x320>; // 800
};
```

`0x618 = 1560`；DMC OPP 只有 1560MHz 一档。原厂把这颗 3733Mbps 的 LPDDR4X 跑在 1560MHz（3120Mbps），低于颗粒额定。

### 2.3 我们之前的配置

`armbian-a/idbloader.img` 偏移 `0x800` 逐字节等于：

```text
rk3568_ddr_1184MHz_android5042_z96a.bin
DDR v1.25，SHA-256 a26184e4eed95f01b1bb2dd8005521774f30f9da5cab09b2581f1d51528fe680
```

Armbian 构建日志确认安装了该自定义 DDR blob。它是 Android 5.04.2 归档里的 blob，不是原厂 live idbloader 的 v1.13。因此原厂 1560MHz，我们之前 1184MHz。

## 3. idbloader 结构与重打包

### 3.1 布局

A idbloader（204,800 字节）：

```text
0x0000..0x0800   RKNS 头
0x0800..0xF000   DDR 初始化 blob（59,392 字节）
0xF000..0x32000  SPL（143,360 字节）
```

### 3.2 头里的校验

RKNS 头不是可以随便改的：

```text
0x90  32 字节：DDR blob 的 SHA-256
0xE0  8 字节 0 + 32 字节：SPL 的 SHA-256
0x600 32 字节：整体哈希
```

直接替换 DDR 段、保留旧头会生成无效镜像（`*.invalid-stale-header`），BootROM/SPL 校验会失败。必须用工具重打包。

### 3.3 正确重打包命令

```sh
mkimage -n rk3568 -T rksd \
  -d <ddr-blob>:<spl-region> \
  <out.img>
```

该命令同时计算 DDR 与 SPL 的哈希。SPL 段直接使用 A idbloader 的 `0xF000..0x32000`，保持已验证的 SPL 不变。

### 3.4 两个 1560MHz 候选

| 文件 | DDR 来源 | SHA-256 |
|---|---|---|
| `idbloader-1560-vendor-rksd.img` | 原厂 live idbloader v1.13 | `35c368f3ad8d84acd756e4bb45bcbff56912b9b4a8892ac605ff35809bf683ac` |
| `idbloader-1560-v126-rksd.img` | rkbin `rk3568_ddr_1560MHz_v1.26.bin` | `2e9d327347e51af4bf055a54eb3844d9841612c5cd66853b55c72424ec7d7efd` |

评估：
- 原厂 v1.13 是这块板子原厂实际运行的 DDR 初始化，板级参数最匹配，风险最低。
- rkbin v1.26 更新，但属于 RK3568 通用配置，未在此 x32 LPDDR4X 拓扑上验证。
- 最终选择原厂 v1.13。

## 4. eMMC HS200/HS400：原厂运行级别

### 4.1 原厂 U-Boot 2017

文件：`artifacts/vendor-uboot/uboot-2017.dts`

```dts
sdhci@fe310000 {
    compatible = "rockchip,dwcmshc-sdhci", "snps,dwcmshc-sdhci";
    max-frequency = <0xbebc200>;   // 200MHz
    bus-width = <0x8>;
    u-boot,dm-spl;
    mmc-hs200-1_8v;
};
```

- 明确启用 **HS200**
- **没有** `mmc-hs400-1_8v`
- SPL 也使用该节点（`u-boot,dm-spl`），所以原厂 SPL 侧同样是 HS200 级别

### 4.2 原厂 Android 内核

文件：`docs/ANDROID-archive-rk-kernel.dts`

```dts
sdhci@fe310000 {
    ...
    bus-width = <0x8>;
    supports-emmc;
    non-removable;
    max-frequency = <0xbebc200>;   // 200MHz
};
```

- 没有 `mmc-hs400-1_8v`，也没有 `mmc-hs200-1_8v`
- `supports-emmc` + 200MHz 上限，驱动不会进入 HS400；实际协商最高为 HS200（或更低）
- 结论：原厂内核配置级别为 **HS200，非 HS400**

### 4.3 各系统对比

| 系统 / DTB | max-frequency | HS400 属性 | 实际 eMMC 级别 |
|---|---|---|---|
| 原厂 U-Boot 2017 | 200MHz | 无 | HS200 |
| 原厂 Android 内核 | 200MHz | 无 | HS200（配置上限） |
| mainline eMMC Linux DTB | 200MHz | 无 | HS200 |
| USB Devuan 原始 DTB | 200MHz | **有 `mmc-hs400-1_8v`** | HS400，写 eMMC 报 I/O error |
| USB Devuan 修改后 DTB | **50MHz** | 无 | high speed / HS200 级别，写入可靠 |

### 4.4 HS400 写失败的实测

USB Devuan 原始 DTB（HS400）下：

```text
dmesg: mmc0: new HS400 MMC card
I/O error, dev mmcblk0, sector 64 (WRITE)
I/O error, dev mmcblk0, sector 32769 (WRITE)   // eMMC p1
I/O error, dev mmcblk0, sector 34848 (WRITE)
```

`dd` 报告成功，但 `O_DIRECT` 读回仍是旧内容 `367d32ac...`。

去掉 `mmc-hs400-1_8v`、把 `max-frequency` 改为 `0x2faf080`（50MHz）后：

```text
dmesg: mmc0: new high speed MMC card
dd ... bs=512 seek=64 conv=fsync,sync  -> 204,800 bytes
读回 SHA-256 = 35c368f3...（与源文件一致）
dmesg 干净：mmcblk0: p1 p2
```

USB DTB 原文件备份：

```text
/boot/rockchip/rk3568-sunniwell-z96a-typec.dtb.z96a-bak
```

当前 USB DTB 是 HS200/50MHz；若要恢复 HS400：

```sh
cp /boot/rockchip/rk3568-sunniwell-z96a-typec.dtb.z96a-bak \
   /boot/rockchip/rk3568-sunniwell-z96a-typec.dtb
```

若要匹配原厂 HS200/200MHz，可把 `max-frequency` 改为 `0xbebc200` 并保持无 HS400 属性。

## 5. 当前 eMMC 状态与回滚

已写入并读回校验：

```text
扇区 0x40   idbloader  SHA-256 35c368f3...
扇区 0x4000 P1 FIT      SHA-256 3dd68203...
```

启动验证：
- U 盘 Devuan 正常启动
- `free -m` 显示 3921MB（4GB）
- 无 DDR/EDAC/Oops 错误
- eMMC 以 high speed / 50MHz 运行

回滚文件：

```text
build/recovery-20261111/idbloader-a.img  SHA-256 367d32ac...
```

回滚方法（Windows RKDevTool / maskrom）：
1. `MiniLoaderAll.bin` 下载到 `0xCCCCCCCC`（RAM，不落盘）
2. `idbloader-a.img` 写到扇区 `0x40`
3. P1 FIT 不需要重写

完整恢复包：`build/recovery-20261111/`，含 `SHA256SUMS` 和 `FLASH-COMMANDS.txt`。

## 6. 大尺寸 U-Boot 嵌入 logo 的失败记录

- 曾把 8.29MB 的 BGRA logo 压缩成 1.45MB 嵌入 U-Boot 本体，使 U-Boot 从约 1.0MB 增至 2.46MB、FIT 增至 2.73MB。
- 同一份 U-Boot+DTB 通过 RAM chainload 能正常显示 logo，但真实 SPL 加载该大 FIT 后板子不启动。
- 源码层未发现 SPL 栈/BSS/FIT 缓冲与 U-Boot 加载区的重叠；最大嫌疑是 eMMC 上预编译 SPL 对大镜像的实际限制（或供电问题），未最终定因。
- 已通过 maskrom 恢复 A idbloader + P1 FIT；之后不再在未做 SPL 等价验证前把大镜像嵌入 U-Boot 本体。
- 安全 logo 方案：U-Boot 保持约 1.0MB，压缩 logo 放 eMMC `/boot`，运行时 `load` 读取、解压、失败回退白屏；先 RAM chainload 验证，再单独申请刷 FIT。

## 7. 关键文件与哈希

```text
build/vendor-v1/vendor-idbloader.bin              原厂 live idbloader（DDR v1.13）
build/out/armbian-a/idbloader.img                 旧 A idbloader（DDR 1184MHz）367d32ac...
build/out/armbian-p1/u-boot.itb                   P1 FIT 3dd68203...
build/recovery-20261111/idbloader-1560-vendor-rksd.img  35c368f3...
build/recovery-20261111/idbloader-1560-v126-rksd.img    2e9d3273...
build/recovery-20261111/idbloader-a.img           367d32ac...
build/recovery-20261111/MiniLoaderAll.bin         3750caaf...
```

## 8. 参考

- `build/usb/phase2-ramtest-emmc/DIAGNOSTICS-2026-11-11.md`
- `docs/ANDROID-archive-rk-kernel.dts`
- `artifacts/vendor-uboot/uboot-2017.dts`
- `artifacts/vendor-uboot/uboot-decompiled.c`
- `build/recovery-20261111/FLASH-COMMANDS.txt`

## 9. 与原厂仍存在的其他差异（本地 DTS 对比）

对比对象：

- 原厂 PD/Type-C 版 Android DTS：`docs/ANDROID-archive-rk-kernel.dts`（model `Sunniwell RK3568 RH218 V01 Board`）
- 原厂 U-Boot 2017 DTS：`artifacts/vendor-uboot/uboot-2017.dts`
- mainline eMMC Linux DTB：`/tmp/opencode/emmc-linux.dts`（model `Sunniwell Z96A (Type-C charging)`）
- 当前 mainline U-Boot DTB：`build/out/logo-embed-v1/u-boot.dtb`

### 已对齐

- eMMC：mainline U-Boot 节点有 `cap-mmc-highspeed`、`mmc-hs200-1_8v`、`max-frequency=200MHz`，与原厂 U-Boot 的 HS200 一致；mainline eMMC Linux 也没有 HS400。
- SD 卡：两边都是 disabled / 150MHz。
- 背光 PWM 周期：两边都是 `0x61a8`（25,000ns，25kHz）。
- 音频硬件：两边都使用 ES7202 PDM 麦克风。
- 大部分 I2C/GPIO/regulator 基础节点一致。

### 仍未对齐 / 需要评估

按优先级排序：

1. **构建默认 DDR 仍是 1184MHz**（高优先级）
   - `armbian-uboot/custom/config/sources/families/rockchip-rk3568-z96a.conf` 第 68 行默认是 `rk3568_ddr_1560MHz_v1.13.bin`，但第 85 行在发现自定义 blob 时覆盖为 `rk3568_ddr_1184MHz_android5042_z96a.bin`。
   - 自定义 1184 blob 仍存在于 `armbian-uboot/packages/blobs/rk35/` 和 `custom/packages/blobs/rk35/`。
   - 结果：下次 Armbian 重建会再次生成 1184MHz idbloader，与当前 live 1560MHz 不一致。
   - 修复：删除/改名这两个 1184 blob，或把覆盖分支改为原厂 v1.13 blob。

2. **WiFi SDIO 速度**（已验证：1.8V / 200MHz SDR104）
   - 原厂 `dwmmc@fe2c0000`：`max-frequency = 0xbebc200`（200MHz），有 `sd-uhs-sdr104`。
   - 旧 mainline：`max-frequency = 0x2faf080`（50MHz），没有 `sd-uhs-sdr104`；直接加 200MHz/SDR104 会 CMD11 超时并回落 50MHz。
   - 根因：RTL8822CS 在 1.8V 下误报 S18A=1，内核发出无意义的 CMD11；同时 `vccio_sd` 固定 3.3V，1.8V 切换无法完成。
   - 已验证方案：`vccio_sd` 固定 1.8V + `vqmmc = &vccio_sd` + 200MHz/SDR104；内核加“1.8V 时跳过 CMD11”和“UHS 失败回退”补丁。实测 `Bus speed = 200MHz`、`tuned phase 122–126`、5G 局域网 16.6–18.0 MB/s。
   - 细节见镜像仓库 `boards/z96a-v2/docs/VENDOR-ALIGNMENT.md` 第 3 节。

3. **充电器 / 电池属性**（高优先级，涉及安全）
   - 原厂 `sc8886@6b` 使用 `southchip,sc8886`，带完整参数：`ti,charge-current = 0xf4240`（1A）、`ti,max-charge-voltage = 0x803c20`（8.4V，2S）、`ti,max-input-current`、SDP/DCP/CDP 电流、`ti,minimum-sys-voltage`、OTG 电压/电流、NTC 表、温度通道。
   - mainline `charger@6b` 使用 `southchip,sc8886s`，只有 `input-current-limit-microamp = 0x2dc6c0`（3A）和 `monitored-battery`，**没有**上述充电电压/电流/NTC 参数。
   - mainline 另有 `simple-battery`（1A、8.4V、OCV 表）；原厂电池信息在 `rk817,battery`。
   - 影响：充电电流/终止电压/NTC 保护可能依赖驱动默认值，与原厂不一致；需要核对这些参数是否由 EC/驱动另行设置。

4. **Type-C / USB-PD**（中优先级）
   - 原厂 HUSB311 有 `switch-ctl-gpios`，`source-pdos/sink-pdos = 0x40190c8`。
   - mainline 没有 `switch-ctl-gpios`，PDO 是 `0x220190c8`。
   - 影响：USB-C 数据角色切换和 PD 协商电压/电流可能不同；需要确认是否影响充电/OTG。

5. **NPU**（中优先级，功能缺失）
   - 原厂 `npu@fde40000` status okay，有 `npu-opp-table`、`rknpu-supply`。
   - mainline eMMC Linux DTB **没有 NPU 节点**。
   - 影响：mainline 下 NPU 不可用；若需要，需移植 rknpu 节点和驱动。

6. **CPU/GPU OPP 与 PVTM/leakage**（中优先级，性能/热）
   - 原厂 CPU OPP 带 `rockchip,pvtm-voltage-sel`、`leakage`/`pvtm`/`mbist-vmin` nvmem、`thermal-zone`、low-temp 调整。
   - mainline CPU OPP 频率档位相同（408M–1992M），但只使用固定电压，没有 PVTM/leakage 选择。
   - 原厂 GPU 有 `simple-power-model`，mainline GPU 节点没有。
   - 影响：高负载下电压/频率策略更保守或更耗电，性能/温度表现可能与原厂不同。

7. **SPI flash（sfc@fe300000）**（低-中优先级）
   - 原厂 status okay。
   - mainline eMMC Linux DTB status disabled。
   - 影响：如果板上有 SPI NOR 且原厂用于 EC/恢复数据，mainline 不会访问；当前启动不依赖它。

8. **背光亮度曲线**（低优先级）
   - 原厂 `brightness-levels` 是 0x00–0xff（0–255）。
   - mainline 是 `<0x14 0xdc>`（20–220）加 `num-interpolated-steps=200`。
   - PWM 周期相同（25kHz），但同一亮度值对应的占空比不同；默认 200 级下实际亮度可能不同。

9. **eDP 显示路径**（低优先级，当前可用）
   - 原厂 eDP 节点 `edp@fe0c0000` 有 `spdif` 时钟、`sunniwell,rh218 = <1>` 和 panel/endpoint 连接；原厂还有两个 `pwm-backlight`（backlight/backlight1）。
   - mainline 使用 `aux-bus` + `edp-panel` + EDID + 单个 backlight，没有 `spdif` 时钟和 `sunniwell,rh218`。
   - 显示已可用；差异主要在面板发现方式、音频 over DP 和属性命名，不是当前故障点。

10. **音频麦克风节点**（低优先级）
    - 原厂用 `ES7202_PDM_ADC_1`/`ES7202_PDM_ADC_2` 两个节点。
    - mainline 用 `everest,es7202` 三个节点。
    - 硬件同为 ES7202；差异是驱动 compatible 和节点数量，需按实际麦克风数量核对。

### 有意保留的差异

- USB Devuan DTB 目前是 HS200/50MHz（原厂 HS200/200MHz）。这是为可靠写 eMMC 的临时措施；写入已验证后可改回 200MHz HS200（仍不要 HS400）。
- mainline U-Boot 不显示 logo、USB 优先启动，是项目既定策略，不属于硬件对齐差异。

### 建议顺序

1. 先修构建默认 DDR：删除/改名 1184 blob，确保重建仍是原厂 v1.13 1560。
2. 再评估 WiFi 200MHz/SDR104 和充电器参数（这两项影响最直接）。
3. NPU/OPP/GPU power-model、SPI flash、背光曲线、eDP/音频按需求逐步处理。
