# C 方案：主线 U-Boot + 延迟 Logo（仅 eMMC 路径）

状态：**阶段 0（静态映射/参考源码）**，未写启动链代码，未刷写。

## 目标

```text
正常启动（无 U 盘 / U 盘不可用）：
  U-Boot 在 eMMC 启动前显示第一屏 Logo

有可启动 U 盘：
  只启动 U 盘，不初始化显示，U 盘路径零影响
```

## 启动链设计

```text
上电
  -> gpio USB 5V
  -> usb start
  -> 只扫描 USB
       ├─ 找到可启动 U 盘 -> 启动 U 盘（显示代码不执行）
       └─ 没有/失败
            -> z96a_logo（显示初始化 + BMP）
            -> 启动 eMMC
```

关键点：

```text
显示代码只能出现在“USB 判定失败之后”。
不能放在 board_late_init，不能放在 preboot，不能放在 usb start 之前。
```

阶段 0 需要确认 mainline 2026.01 的 bootflow 具体语法，确定如何“只扫 USB、再扫 eMMC”；  
如果 `bootflow` 不方便分支，就改为 `boot_targets=usb0` -> `distro_bootcmd` -> `z96a_logo` -> `boot_targets=mmc0` -> `distro_bootcmd`。

## 显示栈范围（最小化）

只做：

```text
VOP2 单 plane
eDP 单屏
固定 BOE NV140FHD-N43：140.24MHz，2084x1100，8bpc，Hsync+/Vsync-
一个 BMP Logo
背光常亮
```

不做：

```text
多 plane / 多屏 / HDMI / DSI / LVDS
atomic / 色彩管理 / gamma / 缩放
vidconsole / 控制台接管
```

## 组件与参考

| 组件 | 原厂 U-Boot (next-dev) | Linux 7.2 | 主线现状 |
|---|---|---|---|
| VOP2 | `rockchip_vop2.c` 8928 行、`rockchip_vop_reg.c` | `rockchip_drm_vop2.c`、`rockchip_vop2_reg.c` | 只有 VOP1 `rk_vop.c` |
| eDP | `analogix_dp.c` 1603 行，含 `rockchip,rk3568-edp` | `analogix_dp-rockchip.c`、`analogix_dp_core.c` | `rk_edp.c` 仅 rk3288/rk3399 |
| eDP PHY | `inno_video_combo_phy.c` / vendor 栈 | `phy-rockchip-naneng-edp.c` | 无 |
| Panel | `rockchip_edp_panel.c` 1604 行 | `panel-edp.c` | 无 |
| Backlight | vendor panel 栈 | `pwm-rockchip.c` + DTS PWM9 | 无 |
| Logo | `rockchip_display.c` / BMP | 不适用 | `video_bmp` 框架有，无显示设备 |

参考源码已放到：

```text
build/edp-logo/vendor/   Rockchip U-Boot next-dev 的 drm/rockchip 源码
build/edp-logo/linux/    Linux 7.2 的 VOP2/eDP/PHY/panel/PWM 源码
```

## 安全约束（硬性）

```text
1. CONFIG_Z96A_EDP_LOGO 默认 n
2. 显示代码不调用 usb_init，不碰 USB PHY
3. stdout 保持 serial，不加 vidconsole
4. 所有等待点必须有超时：
   VOP2 PLL lock、eDP PHY lock、AUX、link training、panel delay
5. 任何失败直接返回，继续 eMMC 启动
6. Logo BMP 从 FIT 或 eMMC 固定位置读，不从 U 盘读
7. 阶段 1 只编译驱动、不调用，确认 U 盘/eMMC 启动不变
```

## 阶段

```text
阶段 0：静态映射（当前）
  - VOP2 寄存器/时钟/GRF、eDP AUX/link、PHY、panel/backlight
  - bootflow 分支语法
  - 输出 PORT-MAP.md

阶段 1：只编译驱动，不调用
  - CONFIG_Z96A_EDP_LOGO=n
  - 确认 idbloader 不变、u-boot.itb 仅新增代码

阶段 2：eDP PHY + AUX 探测
  - 只在无 U 盘路径调用
  - 不点屏，LED 反馈成功/失败

阶段 3：VOP2 + eDP 纯色输出
  - 单 plane，固定模式
  - 硬预算，失败放行

阶段 4：BMP Logo
  - video_bmp + UCLASS_VIDEO
  - 不接管 console

阶段 5（可选）：U 盘路径也显示 Logo
  - 前置显示，硬预算，接受固定延迟
```

## 风险

```text
高：VOP2 时钟/GRF/PLL、eDP link training、Naneng PHY、panel 时序
高：无串口，只能靠 LED / 是否启动判断
中：eMMC 路径显示驱动若卡住，eMMC 启动会被 hold
低：U 盘路径因为显示代码完全不执行，零影响
```

## 当前状态

```text
C 尚未写代码。
已提交 checkpoint：d520d3b（P0/A/P1 补丁、报告、分析）。
下一阶段：PORT-MAP 和 bootflow 分支验证。
```
