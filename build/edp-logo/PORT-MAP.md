# C / eDP-Logo 阶段 0：寄存器与调用链映射

状态：静态映射，**未写驱动代码，未刷写**。  
目标：mainline U-Boot 在「USB 判定无 U 盘」后、eMMC 启动前显示第一屏 Logo。

## 1. 启动钩子（U 盘零影响）

当前 B2 的 bootcmd：

```text
gpio set A6; gpio set E11; gpio set E9; gpio set C31; usb start; bootflow scan -lb
```

C 的目标形式：

```text
gpio set A6; gpio set E11; gpio set E9; gpio set C31;
usb start;
setenv boot_targets usb;
bootflow scan -lb;          // 只尝试 USB；有可启动 U 盘则启动，不返回
z96a_logo;                  // 只有 USB 扫描返回才执行
setenv boot_targets mmc1 mmc0;
bootflow scan -lb;          // eMMC/SD 启动
```

需要阶段 0 验证：

```text
1. bootflow scan 是否按 boot_targets=usb 只扫 USB
2. bootflow scan -lb 在无 USB bootflow 时是否返回而不是继续扫 mmc
3. usb0 的 bootdev label 与 boot_targets 的 usb 是否对应
4. z96a_logo 返回后，boot_targets=mmc1 mmc0 的扫描是否正常
```

如果 `bootflow` 的分支不可靠，退路：

```text
setenv boot_targets usb0; run distro_bootcmd;
z96a_logo;
setenv boot_targets mmc1 mmc0; run distro_bootcmd
```

## 2. 硬件地址与时钟

从 Linux 7.2 DTS 和 vendor/Linux 驱动提取：

| 模块 | 地址/资源 | 说明 |
|---|---|---|
| VOP2 | `0xfe040000`，长度 `0x3000` | `reg-names = "vop"` |
| VOP2 gamma LUT | `0xfe044000`，长度 `0x1000` | `reg-names = "gamma-lut"` |
| VOP2 时钟 | `ACLK_VOP`、`HCLK_VOP`、`DCLK_VOP0/1/2` | eDP 走 **VP1** |
| VOP2 GRF | `&grf` | `rockchip,grf` |
| VOP2 电源域 | `RK3568_PD_VO` | U-Boot 当前 `CONFIG_POWER_DOMAIN=n` |
| eDP ctrl | `0xfe0c0000`，长度 `0x10000` | `rockchip,rk3568-edp` |
| eDP 时钟 | `CLK_EDP_200M`、`PCLK_EDP_CTRL`、`HCLK_VO` | U-Boot `clk_rk3568.c` 未列出 EDP gates |
| eDP 复位 | `SRST_EDP_24M`、`SRST_P_EDP_CTRL` | 需补 reset ID/驱动 |
| eDP PHY GRF | `0xfdcb0000`，长度 `0x100` | `rockchip,rk3568-edp-phy-grf` |
| eDP PHY | `edp_phy` 子节点 | `rockchip,rk3568-edp-phy` |
| eDP PHY 时钟 | `XIN_OSC0_EDPPHY_G`、`PCLK_EDPPHY_GRF` | 需确认 U-Boot 时钟 ID |
| Panel 电源 | GPIO0_C7 (`vcc3v3_lcd0_n`) | active high |
| Backlight enable | GPIO2_D5 | active high |
| Backlight PWM | PWM9，25 kHz | `CONFIG_PWM_ROCKCHIP` 当前未开 |

## 3. 显示输出链路

```text
VOP2 VP1
  -> vp1_out_edp
  -> eDP controller (0xfe0c0000)
  -> eDP PHY (GRF 0xfdcb0000)
  -> panel (BOE NV140FHM-N43)
  -> backlight (PWM9 + GPIO2_D5)
```

Linux DTS 关键连接：

```text
&vp1 { vp1_out_edp: endpoint@ROCKCHIP_VOP2_EP_EDP0 { ... }; };
&edp { force-hpd; status = "okay"; aux-bus { panel: panel { compatible = "edp-panel"; no-hpd; }; }; };
&edp_in { edp_in_vp1: endpoint { remote-endpoint = <&vp1_out_edp>; }; };
```

U-Boot 当前 DTS 状态：

```text
vop@fe040000 在 rk356x-base.dtsi 中存在，rk3568.dtsi 覆盖 compatible = "rockchip,rk3568-vop"
U-Boot rk3568.dtsi 没有 edp / edp_phy 节点
board DTS 没有 panel/backlight/vcc3v3_lcd0_n
```

阶段 1 需要从 Linux DTS 移植这些节点，并只给 U-Boot 使用。

## 4. Panel 模式

实机 EDID（2026-11-11 从运行中的 Z96A V2 读取）：

```text
model        = BOE NV140FHD-N43
clock        = 140240 kHz
hdisplay     = 1920
hfront       = 48
hsync        = 32
hback        = 124
htotal       = 2084
vdisplay     = 1080
vfront       = 3
vsync        = 6
vback        = 11
vtotal       = 1100
bpc          = 8
Hsync        = positive
Vsync        = negative
```

注意：Linux `panel-edp.c` 里的静态 `boe_nv140fhm_n43` 表是 139.8MHz / 2080x1120 / 6bpc，和这块实机 EDID 不一致。U-Boot 固定模式必须用实机 EDID，而不是静态表。

eDP 平台参数（vendor `analogix_dp.c` 的 `rk3568_edp_platform_data`）：

```text
chip_type      = RK3568_EDP
ssc            = true
max_link_rate  = DP_LINK_BW_2_7
max_lane_count = 4
```

Naneng PHY 支持 1/2/4 lane；实际 lane 数由 eDP link training 读 DPCD 决定（Z96A 面板为 eDP1.2，可能是 2 lane）。  
面板是 `force-hpd` + `no-hpd`，所以不能依赖 HPD；按固定延迟/固定模式启动。

## 5. 驱动拆分（最小化）

| 文件（建议） | 职责 | 参考 |
|---|---|---|
| `rk3568_vop2.c` | VOP2 单 plane、VP1、dclk、GRF、OVL、DSP | vendor `rockchip_vop2.c` / Linux `rockchip_drm_vop2.c` |
| `rk3568_edp.c` | eDP AUX、DPCD、link training、video | vendor `analogix_dp.c` / mainline `rk_edp.c` / Linux `analogix_dp_core.c` |
| `rk3568_edp_phy.c` | Naneng eDP PHY rate/lane/power | Linux `phy-rockchip-naneng-edp.c` |
| panel 逻辑 | 固定模式、GPIO0_C7、PWM9、GPIO2_D5 | Linux `panel-edp.c` + DTS |
| `z96a_logo.c` | UCLASS_VIDEO 设备、framebuffer、video_bmp | mainline `video-uclass.c` / `video_bmp.c` |

VOP2 只做：

```text
一个 RGB plane
VP1 固定模式
无 atomic、无多 plane、无 HDMI/DSI/LVDS
```

eDP 只做：

```text
AUX 读写 DPCD
2.7 Gbps / 1-4 lane（DPCD 决定）/ SSC
固定模式 link training
```

## 6. 当前缺口

```text
1. U-Boot DTS 缺 eDP/eDP PHY/panel/backlight 节点
2. clk_rk3568.c 缺 EDP 200M/PCLK_EDP_CTRL/EDPPHY_GRF 时钟
3. CONFIG_PWM_ROCKCHIP 未开
4. CONFIG_POWER_DOMAIN 未开，PD_VO 需要 PMU/GRF 处理
5. CONFIG_VIDEO 未开
6. VOP2 单 plane 最小寄存器序列未提取
7. eDP PHY 的 GRF 序列未提取
8. bootflow 的 USB-only 分支未实机验证
9. Logo BMP 来源未定（FIT 内置 vs eMMC p1 /boot.bmp）
```

## 7. 阶段 1 的前置条件

```text
A. 确认 bootflow scan 的 boot_targets 分支
B. 确认 U-Boot clock/reset 能覆盖 EDP/VOP2
C. 确认 PD_VO 是否默认开启
D. 确认 PWM9/GPIO 在 U-Boot 下可控
E. 只编译驱动、不调用，U 盘/eMMC 启动零变化
```

## 8. 风险

```text
高：VOP2 8928 行 vendor 栈、Linux VOP2 5k+ 行；最小化仍需大量寄存器序列
高：eDP link training 依赖 AUX/PHY，失败会 hold eMMC 路径
中：U-Boot 时钟/复位/电源域缺失
中：无串口，调试只能靠 LED 和是否启动
低：U 盘路径因为 z96a_logo 在 USB 扫描之后，零影响
```
