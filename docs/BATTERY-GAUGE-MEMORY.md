# Z96A 电池电量计：已核实记录

更新时间：2026-11-13。此文是当前快照；操作前先核对设备实际状态。

## 电池规格

用户确认的电池：

```text
SHT 3585130-2S
5000mAh / 37Wh
8.4V（2S）
```

原厂 DTS 的 `design_capacity` 是 `0x1405`（5125mAh）、`design_qmax` 是
`0x157c`（5500mAh）、`design_max_voltage` 是 `0x206c`（8300mV）；
那套参数来自原厂 BSP，和用户给的 5000mAh/8.4V 规格接近但不完全一致。
mainline 的 `simple-battery` 应先按用户规格用 5000mAh / 8.4V，OCV 表沿用原厂
21 点（8362mV→100% ... 6600mV→0%）。

## 原厂路径（不要重复反编译）

原厂 Android DTB 在 RK809 PMIC 下有：

```dts
battery {
	compatible = "rk817,battery";
	ocv_table = <...21 点...>;
	design_capacity = <0x1405>;   /* 5125mAh */
	design_qmax = <0x157c>;       /* 5500mAh */
	bat_res = <0x96>;             /* 150mΩ，电池内阻 */
	sleep_enter_current = <0x12c>;  /* 300 */
	sleep_filter_current = <0x64>;  /* 100 */
	power_off_thresd = <0x19c8>;
	sample_res = <0xa>;           /* 10, 10mΩ 采样电阻 */
	bat_res_up = <0x8c>;          /* 140 */
	bat_res_down = <0x14>;        /* 20 */
	design_max_voltage = <0x206c>;/* 8300mV */
	io-channels = <&saradc 5>;
	io-channel-names = "battery-chan-5";
};
```

原厂 kernel 二进制（`artifacts/android/extracted/boot-kernel.bin`）包含
`rk817-bat:` 驱动字符串；原厂 DTB 的属性已由 `fdtget` 复核。
原厂 4.19.232 驱动源码已保存在：

```text
analysis/vendor/rk817_battery.c
```

来源：

```text
https://raw.githubusercontent.com/rockchip-linux/kernel/develop-4.19/drivers/power/supply/rk817_battery.c
```

关键行为（RK809）：

- `chip_id == RK809_ID` 时电压校准用 `600..1050mV`（RK817 是 `2300..4025mV`）；
- 电池电压、PWRON 电压、OCV 电压都要按
  `vol = vol * bat_res_up / bat_res_down + vol` 做分压还原（本板 140/20）；
- `sample_res == 20` 时 `res_div=2`，本板 `sample_res=10`，`res_div=1`；
- 充电状态在 RK809 上用电压/电流判断（`design_max_voltage` 附近 + 小电流为满）；
- 原厂 `rk817_battery` 是一套完整的 3434 行电量算法（zero/smooth/OCV/NVRAM），
  不是简单的 OCV 查表。

## mainline 现状（2026-11-13 实测）

`drivers/power/supply/rk817_charger.c` 是 mainline 的 **RK817 单节**适配版：

- 驱动 probe 需要 PMIC 下名为 `charger` 的子节点；
- 用 RK817 校准常数 `2300..4025mV`，没有 RK809 分压；
- `rk817_bat_calib_cap` 的 sanity 上限硬编码 `4.45V`。

`.84` 实测（已加 `charger` 子节点后）：

```text
rk817-battery: capacity=1%, voltage_avg=3,996,920uV, current_avg=549,368uA
bq257xx-fgu:   voltage_now=8,320,000uV, current_now=640,000uA
```

结论：**库仑计电流/采样路径正常**（0.549A 与 0.64A 同量级，10mΩ 方向正确）；
**电压路径错误**：RK809 2S 的 ~8.3V 被当成 RK817 1S 的 ~4.0V，OCV 初始化成 0/1%。

当前回退：

- `bq257xx-fgu` 仍用 `adc_battery_helper` + `power_supply_batinfo_ocv2cap()`，
  即电压 OCV 模拟，当前约 97%；
- 不准确，但比错误的 1% 好；
- `rk817_charger` 已临时 blacklist，DTB 已恢复为 `f06d...`（无 `charger` 节点）。

仓库状态：

```text
0ea0a9e board: enable z96a-v2 RK817 PMIC coulomb-counter gauge
0f1ed58 board: keep rk817 coulomb gauge disabled until RK809/2S support
```

## 最小 driver patch 方案

不改整个 vendor 3434 行驱动，给 mainline `rk817_charger.c` 补 RK809/2S 分支：

1. `rk808->variant == RK809_ID` 识别 PMIC；
2. `rk817_bat_calib_vol` 用 `600..1050mV` 校准；
3. 从 `charger` 节点解析 `rockchip,bat-res-up` / `rockchip,bat-res-down`，
   对 `BAT_VOL`、`PWRON_VOL`、`OCV_VOL` 做分压还原；
4. `rk817_bat_calib_cap` 的 sanity 改成 2S 范围（约 6.0..8.6V）；
5. `charge_status` 对 RK809 用库仑计电流方向和 `design_max_voltage` 判断，
   不依赖 PMIC 内部充电器（本板充电走外部 SC8886）；
6. `simple-battery` 改成 5000mAh / 8.4V（OCV 表保持原厂 21 点）；
7. DTB 的 `charger` 节点增加 `rockchip,bat-res-up = <140>`、
   `rockchip,bat-res-down = <20>`；
8. 用当前 7.2.12 的 `/proc/config.gz` 编译 `rk817_charger.ko`
   （`CONFIG_CHARGER_RK817=m`、`CONFIG_MODVERSIONS` 未开，vermagic
   `7.2.12 SMP preempt mod_unload aarch64`），安装后重启；
9. 验证通过再移除 `blacklist-rk817-gauge.conf`，重新启用 DTB 节点和 0020。

## 已验证（2026-11-13）

- `0021-z96a-rk809-2s-gauge.patch`：给 mainline `rk817_charger.c` 加 RK809/2S 分支
  （`RK809_ID`、`600..1050mV` 校准、`bat_res_up/down` 分压、2S sanity、电流状态）。
- `0022-z96a-battery-5000mah-divider.patch`：`simple-battery` 改 5000mAh，
  `charger` 节点加 `rockchip,bat-res-up = <140>`、`bat-res-down = <20>`。
- `0024-z96a-rk809-battery-present.patch`：RK809 的 `BAT_EXS` 不可靠，强制
  `battery_present = true`；否则满电校准不跑，容量会停在 5%。
- 已安装 `0021 + 0024` 编出的 `rk817_charger.ko`（vermagic
  `7.2.12 SMP preempt mod_unload aarch64`）和基于当前 `f06d` 的
  `...-gauge-v2.dtb`；`blacklist-rk817-gauge.conf` 已禁用/移除。
- 实测：
  `rk817-battery present=1 status=Full capacity=100 voltage_avg=8.387V
  current_avg≈69–75mA charge_full=5Ah`
- `bq257xx-charger` 仍在充电（约 64mA）；`bq257xx-fgu`（电压 OCV）已 unbind
  并在 `/etc/modprobe.d/blacklist-bq257xx-fgu.conf` 永久 blacklist。
- upower 只剩一块 `battery_rk817_battery`：100% / 42.02Wh / 8.387V；
  不再把两块“电池”叠加成 83Wh / 99%。
- 仍需一个完整充放电周期观察库仑计长期漂移。

## 托盘显示

- 电池图标来自自定义 `z96a-system-tray`，不是 LXQt 自带插件。
- 默认设备已从 `/sys/class/power_supply/bq257xx-fgu` 改为
  `/sys/class/power_supply/rk817-battery`；`Z96A_BATTERY_DEVICE` 仍可覆盖。
- `lxqt-powermanagement` 的 `enableBatteryWatcher=false` 应保持关闭，
  否则会出现重复电池图标；它只负责电源动作。
- 2026-11-13 放电跟踪：`rk817-battery` 97% / 8.179V / -773mA，
  upower 99→98→97% 连续下降，说明库仑计在正常积分。

## 构建产物

```text
rea-z97a/build/out/rk817-gauge/
  rk817_charger-rk809-2s.ko        # v2（0021 + 0024），当前 .84 已安装
  rk3568-sunniwell-z96a-typec-gauge-v2.dtb
  INSTALL-NOTES.md
```

## 验证清单

```bash
cat /sys/class/power_supply/rk817-battery/capacity
cat /sys/class/power_supply/rk817-battery/voltage_avg
cat /sys/class/power_supply/rk817-battery/current_avg
cat /sys/class/power_supply/rk817-battery/status
```

- `voltage_avg` 应接近 `bq257xx-fgu/voltage_now`（约 8.0–8.4V）；
- `current_avg` 与 `bq257xx-charger/current_now` 同向、同量级；
- 充电时 status 应为 Charging，满电/小电流时为 Full；
- 若 NVRAM 里残留旧的 1% SOC，第一次可能需要一次完整充放电或清
  `RK817_BAT_CON`（0x57 bit4）/冷启动，让驱动按 OCV 重新初始化。

## 安全与约束

- 不改 eMMC 引导区/U-Boot。
- 电池是 2S；不要让 PMIC 内部 1S charger 直接参与 8.4V 充电。
- 外部 SC8886 是实际充电器；`rk817-charger` 只作为 PMIC 状态/附带供应，
  不应当作主充电路径。
