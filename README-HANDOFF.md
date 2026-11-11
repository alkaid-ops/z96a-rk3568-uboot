# Z96A 原厂 U-Boot → 主线 U-Boot 移植交接

生成时间：2026-11-10 17:15（Asia/Shanghai）  
目标机器：`.132`，目录 `/home/hsy/project/rea-z97a`  
任务性质：**静态反编译、源码对照、移植设计和离线构建**。未获新的明确授权前，不要刷写 eMMC、U-Boot、idbloader、GPT 或分区表。

## 1. 任务目标

目标不是整体替换主线 U-Boot，而是把原厂 U-Boot 中已经验证存在、且主线缺失的硬件初始化语义移植到当前主线 U-Boot：

- 原厂正常的 Type-C / 充电器初始化路径；
- `board_late_init` 前后的电源、GPIO、USB、充电状态机；
- 开机 Logo、充电显示、LED、reboot 等与充电问题可能相关的差异；
- 保留当前主线已经具备的 USB-first、extlinux、eDP、USB3 `combphy1` 等能力。

必须区分以下两件事：

1. **原厂代码里确实存在某段初始化**；
2. **该初始化与当前充电故障有因果关系**。

目前第 1 项已经发现多处差异；第 2 项仍未完成单变量证明。

## 2. 当前真机快照

2026-11-10 17:12 只读核查：

```text
host: z96a-devuan
root: /dev/sda2              # 从 U 盘启动
kernel: 7.2.12
charger: online=1, status=Charging, current≈192000 µA
```

当前启动链：

- eMMC 上的主线 U-Boot 2026.01 接管启动；
- U 盘 p1/p2 上的 Devuan/Armbian 系统运行；
- U-Boot 当前第二个 4 MiB 区域现场哈希：

```text
87e522e3b5838e7df24dd7d5622d4c5bd4d4c0f067d412043d3ca94ec824949c  current-emmc-uboot-region.bin
```

原厂 Android 完整备份：

```text
z96a_full_no_userdata_20260818.img
sha256: 02c47675a9bd7c69ce1f85984f91f111cd7bbe356de89bda745d2a249696a4d2
```

当前 U 盘文件：

```text
0995128a92c4c96bf29e27eb42aeedc320b71e40ee5f52ff6b1cc6c7f42ef410  /boot/uInitrd
06fe8814f103bf6d803c38ea090067941db3e69467395d9b227c6b8c9ebbd557  tcpci_rt1711h.ko (v5)
ccaf267be51c98a69cd76a38b20778666e14e52eb18aed7adbea053895ad8c93  /boot/rockchip/rk3568-sunniwell-z96a-typec.dtb
```

## 3. 安全约束

**必须先读 `docs/CHARGER-VERIFIED-MEMORY.md`。** 它是充电问题的当前权威快照。

用户已明确取消所有充电定时和自动恢复服务；没有新的明确授权，不得恢复：

- `.84` 的 `z96a-charger-runtime-recovery`；
- `.132` 的充电监控 cron；
- 任何整机断电、插座轮询或自动重绑定流程。

当前工作应停留在 .132 的离线分析、源码修改、编译和静态检查。若确需真机测试，必须先向用户说明测试项、回滚方法和是否会重启。

## 4. 已确认的 U-Boot 版本

### 原厂

```text
U-Boot 2017.09-gf06e383e-dirty #liuguodong
Mar 04 2025
```

原厂 U-Boot proper：

```text
size: 1,220,096 bytes
sha256: 7b1438d3b4253604cb36e04a54bbd5a53024ecb3358f07a0d03224ca9491cfb7
load address: 0x00800000
```

原厂完整 16 MiB 启动区备份：

```text
factory-first16m.bin
sha256: f761cac65d410a629dc3b6610e4791c6b2a3aa86a6ce3d7fdae6c3f8a76c87e1
```

### 当前主线

```text
U-Boot 2026.01-armbian
```

源码钉住版本：

```text
repo: https://github.com/kemp233/u-boot-1.git
commit: 8e474aa605a86de64316ba6da8dc089402976380
```

同一源码树上的本机历史产物：

```text
u-boot.bin FIT sha256: efd764f4aaf53ec588884447b38c1a5b4b3d301cbf3081398330f9272e7a2895
idbloader.bin sha256: 7d799106bd6942bd57e25c56d7a1c89cd354ab836bdeeed3a1ddcee86c09fe4f
u-boot-rk3568-z96a-v2_2026.01-1_arm64.deb
sha256: dcf2c64543ab403477468117e1749f0020db9dfdd2137de237a5299d38283549
```

注意：`efd764...` 是构建输出 FIT；`87e522...` 是从当前 eMMC 第二个 U-Boot 区域抓取的现场 4 MiB 区域。不要把两个哈希混用。

## 5. 目前最重要的差异

### 5.1 原厂正常启动路径有早期 GPIO 初始化

原厂静态路径：

```text
0x00a03e14 -> 0x00a03064 -> 0x00a02d10
```

`0x00a02d10` 重建语义：

```c
gpio_request(0x15, "power_usb_hub");
gpio_direction_output(0x15, 1);
gpio_set_value(0x15, 1);

gpio_request(0x50, "power_usb_cam");
gpio_direction_output(0x50, 1);
gpio_set_value(0x50, 1);

gpio_request(0, "power_wifi_module");
gpio_direction_output(0, 1);
gpio_set_value(0, 1);
```

映射提示：

- `0` = GPIO0_A0，当前 Linux DTS 为 Wi-Fi `vcc_wifi`；
- `0x15` = GPIO0_C5，当前 Linux DTS 是 HUSB311 IRQ，**不是复位脚**；
- `0x50` = GPIO2_D0，需结合硬件和 pinctrl 再确认具体功能。

限制：原厂代码忽略上述调用的返回值；静态代码不能证明每次启动都成功改变物理电平。

### 5.2 原厂有完整 BQ257xx / SC8886 充电框架

原厂 U-Boot 中存在：

- `bq25700_charger`；
- `charge-animation`；
- `uboot-charge / android-charge`；
- NTC 温度表；
- USB charger / DC / AC 类型判断；
- 低电量充电和退出充电状态机；
- 充电 Logo/LED 相关逻辑。

对 SC8886/BQ25703 系列可识别的原厂初始化语义：

```text
读 0x00，清 watchdog 字段后写回
写 0x0C = 0x1200
根据 USB 类型写 0x0E = 0x1E00 或 0x0A00
若 AC_STAT 存在，写 0x02 = 0x0400（1024 mA）
```

入口和参考：

```text
charger DTS 解析入口: 0x00a7dcc0
SC8886/BQ25703 初始化: 0x00a7e3bc
```

原厂路径没有发现明确的 QC 芯片 GPIO 复位，也没有写 `RESET_VINDPM`。不要把“充电恢复需要 GPIO 复位”当成已证实事实。

### 5.3 当前主线只有简化的 `z96a-pwr`

当前 2026.01 主线：

```text
FUN_008036c4 (board late init) -> FUN_00804284 (z96a-pwr)
```

它只做：

```text
读 SC8886 0x20 / 0x21 / 0x26
写 0x36 = 0x65
写 0x37 = 0x4A
AC 存在时清 EN_OTG、清 EN_LWPWR、写 0x14
设置环境变量 z96a_power_src
```

主线没有原厂完整 charger framework、charge animation、NTC 和早期 GPIO 电源初始化。这是“主线不等于原厂”的直接证据。

### 5.4 原厂 U-Boot 没有发现完整 HUSB311/tcpm 驱动

字符串和静态分析没有发现原厂 U-Boot 中的 `tcpm/husb311` 驱动栈。原厂可能只是通过 GPIO、USB PHY、VBUS 电源和充电器寄存器给 Linux 留下不同的初始状态。

因此当前最合理的方向是：

> 先把原厂 U-Boot 的硬件初始化顺序和寄存器语义完整还原，再判断它如何影响 Linux 首次 probe；不要先假设需要 GPIO 复位。

## 6. 已知实验，不可重复滥用

此前做过两轮“只重放 SC8886 寄存器初值”的临时测试：

- 第 1 次适配器断电/恢复约 8 秒恢复；
- 第 2 次未恢复，随后由当时仍存在的看门狗整机断电恢复；
- 测试钩子已回滚，日志在 `.84:/root/z96a-boot-state-replay-results/`。

结论：

- 仅重放 SC8886 初值不足以根除；
- GPIO0_C5 在 rootfs 启动脚本执行前已被占用，说明驱动很可能由 initramfs 提前加载；
- 该实验没有完成“probe 前 GPIO + 充电器全量初态”的严格单变量测试；
- 不要在没有新授权时重复充电测试或恢复自动恢复服务。

## 7. 建议的移植路线

### 第一步：建立准确调用图

在原厂 U-Boot 中继续确认：

1. 正常启动是否确实执行 `0x00a03064 -> 0x00a02d10`；
2. `0x00a03064` 的其他分支如何改变 `preboot`；
3. 充电器初始化是否仅在充电模式执行，还是正常启动也执行；
4. 充电 Logo/LED 与正常启动路径是否共用状态机；
5. `reboot` 表现成关机是否与 PMIC、PSCI、看门狗或原厂 board code 相关；
6. 所有 GPIO 写入最终对应的 bank/offset、pinmux、active level 和调用返回。

### 第二步：主线中增加一个明确的工厂初始化层

建议在主线 board 目录中新增独立函数，而不是把原厂反编译代码直接贴入：

```text
z96a_factory_power_init()
z96a_factory_charger_init()
z96a_factory_charge_animation()   # 仅在证据支持时
```

目标是保留当前主线 API、DM、regulator 和 extlinux，不破坏现有 USB-first 启动。

### 第三步：逐项单变量移植

建议顺序：

1. 早期 GPIO 电源状态；
2. SC8886 完整初始化；
3. 充电器状态读取、AC 检测和日志；
4. 充电动画、LED、Logo；
5. reboot/关机路径。

每一步都在 .132 编译出独立 U-Boot FIT 和静态 diff，不直接写设备。

### 第四步：硬件测试需重新授权

至少需要：

- U-Boot 静态验证；
- FIT 哈希校验；
- USB-first、eMMC fallback 回归；
- eDP/键盘/触控/USB/WiFi 不回归；
- 多次冷启动和适配器循环；
- 明确的回滚镜像与恢复路径。

## 8. 目录内容

```text
README-HANDOFF.md
docs/
  CHARGER-VERIFIED-MEMORY.md                 # 权威快照，先读
  CHARGER-ROOT-CAUSE-STATUS-20261110.md
  CHARGER-RUNTIME-RECOVERY.md
  VENDOR-UBOOT-ANALYSIS.md                   # 原厂 U-Boot 详细静态分析
  MAINLINE-BUILD-PROJECT.md
  BUILD-PROVENANCE.md
  Z96A-DC-vs-TYPEC-dtb-diff.md
  ANDROID-rk-kernel-0.dts
  ANDROID-archive-rk-kernel.dts
artifacts/android/
  z96a_full_no_userdata_20260818.img      # 完整 Android 备份，只读反编译用
  archive-boot.img
  extracted/                               # boot/recovery/kernel/ramdisk/DTB/DTS
artifacts/vendor-uboot/
  uboot-2017.bin
  uboot-2017.dts
  uboot-default-env.txt
  uboot-decompiled.c
  factory-first16m.bin
  archive-uboot-decompiled.c
artifacts/mainline-uboot/
  u-boot.bin                                 # 2026.01 FIT
  extracted/u-boot-image0.bin                # FIT 中 U-Boot proper
  extracted/u-boot-fdt.dtb/.dts              # FIT 内嵌 U-Boot DTB
  extracted/fit-info.txt
  current-emmc-uboot-region.bin
  idbloader.bin
  u-boot-rk3568-z96a-v2_2026.01-1_arm64.deb
  source-snapshot/                           # board.c、DTS、defconfig 快照
  patches/                                   # 历史移植补丁，需核对是否适用于当前 commit
artifacts/linux/
  tcpci-rockchip.c
  tcpci_husb311-rockchip.c
  bq25700_charger-rockchip.c
  bq25700-charge-rockchip.h
  sc8886s.h
  rk3568-sunniwell-z96a-typec.dts
  tcpci_rt1711h.c
  bq257xx_charger.c
analysis/
  reconstructed/                             # 已有可读重建代码
  ghidra-scripts/                            # Ghidra/REA 辅助脚本
  PORT-SEED.md                               # 原厂语义到主线源码的落点
  port-seed.c                                # 不可直接编译的移植种子
  rea-wifi-decompile.json
tools/
  z96a_boot_state_replay.py                  # 历史诊断工具，不是修复
  ha_switch.py                               # Home Assistant 插座控制，测试前必须获授权
u-boot-mainline-src/                         # 钉住 commit 的主线 U-Boot 源码
rea-src/                                     # 最新 REA 源码快照
SOURCE-REVISIONS.txt                         # U-Boot/REA commit 记录
CHECKSUMS.sha256                             # 本包静态工件校验和
```

## 9. Ghidra / REA 建议

`.132` 已有 Ghidra 12.2：

```text
/home/hsy/project/z96a-vendor-re-analysis/ghidra_12.2_PUBLIC
```

原厂 U-Boot proper 的映射：

```text
loader: BinaryLoader
architecture: AArch64
base/entry: 0x00800000
```

主线 U-Boot image0 的映射相同。FIT 中的 `entry` 可以是 `0x00800000`，但不要把 FIT 文件本身误当成 ELF。

推荐 Ghidra headless 参数：

```sh
analyzeHeadless /home/hsy/project/rea-z97a/ghidra-projects Z96A-vendor \
  -import /home/hsy/project/rea-z97a/artifacts/vendor-uboot/uboot-2017.bin \
  -processor AARCH64:LE:64:v8A \
  -loader BinaryLoader \
  -loader-baseAddr 0x800000 \
  -scriptPath /home/hsy/project/rea-z97a/analysis/ghidra-scripts \
  -postScript ExportCharger.java /home/hsy/project/rea-z97a/analysis/out
```

如果 REA 可用，重点也是：

```text
vendor: 0x00a02d10, 0x00a03064, 0x00a03e14,
        0x00a7dcc0, 0x00a7e3bc, 0x00a68844
mainline: 0x008036c4, 0x00804284
```

## 10. 交接完成标准

新机器上应能独立完成：

1. 解包并验证 vendor/mainline 两个 U-Boot 二进制；
2. 重建原厂启动 GPIO、电源、充电器调用图；
3. 生成主线 board 层的最小移植 patch；
4. 编译 U-Boot 2026.01 并做 FIT、DTB、boot_targets、extlinux 静态检查；
5. 输出“原厂语义 → 主线实现 → 风险 → 测试项”表格；
6. 未经明确授权，不接触真机启动区。
