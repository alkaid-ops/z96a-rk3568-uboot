# Z96A C 阶段 1b：eDP Logo 显示栈（只编译、不调用、不刷写）

日期：2026-11-11
基底：Armbian 2026.01 U-Boot（`kemp233/u-boot-1` @ `8e474aa...`）
补丁：`0005..0014` + `0015-z96a-edp-logo-phase1.patch`（1b 修正版）
配置：`CONFIG_Z96A_EDP_LOGO` **未启用**（默认 n）

## 1. 产物（阶段 1b）

| 文件 | 大小 | SHA-256 |
|---|---|---|
| `idbloader.img` | 204800 | `a20c25d77350f785d4d7cd20126631c5e1833b18709eb423de4038555708a8d3` |
| `u-boot.itb` | 1290240 | `3fe9652150e566ac2ad1696aca776d9ba33e65bb64bc1e237165e8b66fa5f0b8` |
| `u-boot.bin` | 1093168 | `30c019343af796d6c197b05beb67f81aebb1a3d2a6e877d07c58dfadf2e98fb7` |
| `u-boot.dtb` | 76864 | `38a20c94e733c47f8d449a93debf12a720bcfe8706694be24f4a7bbc8318ab14` |
| `u-boot-spl.bin` | 142486 | `e79130cc51de383c5ccc838c5a4786f597a9e8d150b93a1525a67dd15d064a67` |

对照 P1 现场：

```text
P1 idbloader   204800  2e6d0ce2...   （现场仍用 A 的 367d32ac...）
P1 u-boot.itb  1221120 3dd68203...   （当前现场）
P1 SPL         142486  153a1b0d...   （从 P1 idbloader 提取）
P1 u-boot data 949880
P1 fdt         74240
```

## 1b. 实机 EDID/DRM/PWM 校正

通过 SSH 只读读取当前 Z96A V2（未写 eMMC）：

```text
EDID: BOE NV140FHD-N43
1920x1080 @ 60.024 Hz, 140.24 MHz
Hfront 48 / Hsync 32 / Hback 124 -> htotal 2084
Vfront 3 / Vsync 6 / Vback 11 -> vtotal 1100
Hsync positive, Vsync negative
8 bpc
```

DRM/PWM：

```text
card0-eDP-1 status = connected
eDP-1 output_bpc = Maximum: 8
VOP2 plane Smart1-win0 -> video_port1
/sys/kernel/debug/pwm: platform/fe6f0010.pwm (PWM9) -> backlight
```

阶段 1b 修正：

```text
1. panel timing 139.8/2080x1120 -> 140.24/2084x1100
2. hsync-active 0 -> 1（EDID Hpol P）
3. vsync-len 5 -> 6，vback 32 -> 11
4. bpc 6 -> 8，IN_BPC_6 -> IN_BPC_8
5. DCLK_VOP1 139.8 -> 140.24 MHz
6. VPLL 699 -> 701.2 MHz
7. backlight default 255 -> 220（匹配实机 max_brightness）
8. rk_vop2_disable 增加 VP post standby
9. eDP PHY 增加 set_lanes，按 1/2/4 lane 处理 TX_PD/TX_IDLE
10. clk_rk3568 增加 701.2MHz 分数 PLL 表项，并用 CONFIG_XPL_BUILD 排除在 SPL 之外
```

以下参数保持不变（实机已验证）：

```text
背光 PWM = PWM9（fe6f0010）
背光 enable = GPIO2_D5
VP1 plane = Smart1
eDP 时钟 = CLK_EDP_200M + PCLK_EDP_CTRL + HCLK_VO
面板 enable GPIO3_A3 不加（当前 V2 live DT 没有）
```

## 2. 阶段 1 实际改动

### 2.1 主线 VOP2 补丁子集

新增：

```text
arch/arm/include/asm/arch-rockchip/vop_rk3568.h
drivers/video/rockchip/rk_vop2.c
drivers/video/rockchip/rk_vop2.h
drivers/video/rockchip/rk3568_vop.c
```

采用主线 2026-07 `[PATCH 1/9]` 的 VOP2/VP1 实现，并做 backport 修复：

```text
1. rk_vop2.h 的 platdata 从函数指针改为 void *
2. V_ESMART_REGION0_DATA_FMT 掩码 0x16 -> 0x1f
3. eDP/DP 输出模式改为 AAAA(15)，而不是 RGB888(0)
4. 去掉 DM_FLAG_PRE_RELOC，改为 z96a_logo 手动 bind
5. 增加 VO/VOP 时钟门控解门控
6. 增加 rk_vop2_disable() 失败清理，避免 VOP DMA 留给 Linux
7. 打开 DM_FLAG_OS_PREPARE，bootm 前清理 plane
```

采用：

```text
[4/9] clk_rk3568.c 的 VPLL 分频修正
[5/9] rk3568_vop.c 的 regs 空指针修复
```

跳过：

```text
[2/9] &vop bootph-some-ram   → 保持 SPL DT 不变
[3/9] HDMI driver
[6/9][7/9][8/9] HDMI 时钟/PHY
[9/9] nanopi-r5c defconfig
```

### 2.2 eDP PHY

新增：

```text
drivers/phy/rockchip/phy-rockchip-naneng-edp.c
include/phy-dp.h
```

基于 Rockchip vendor `next-dev` 的 418 行 PHY 驱动，改 U-Boot API：

```text
syscon_get_regmap 父节点
generic_phy .configure(void *params)
regmap_read_poll_timeout PLL_RDY
.priv_auto
```

### 2.3 eDP controller

修改 `drivers/video/rockchip/rk_edp.c`：

```text
RK3568_EDP chip type
外部 Naneng PHY：get/set_mode/power_on/configure
RK3568 controller 初始化（Linux analogix_dp_reg.c 顺序）
  analog param / interrupt / SW function / analog func / AUX
IN_BPC_8_BITS + IN_COLOR_F_RGB（实机 EDID 8bpc）
link training 时 PHY set_rate/set_voltages
read_timing 从 simple-panel 取固定时序
backlight 前 50ms
panel 上电后 200ms
```

### 2.4 DTS

`dts/upstream/src/arm64/rockchip/rk3568.dtsi`：

```text
edp_phy_grf / edp_phy / edp
```

`rk3568-z96a-laptop-v2.dts`：

```text
vop status = "disabled"
vp1 -> edp_in_vp1
edp status = "okay", force-hpd, rockchip,panel
panel: simple-panel + display-timings
backlight: pwm-backlight, PWM9 25kHz, GPIO2_D5
vcc3v3_lcd0_n: regulator-fixed, GPIO0_C7
pwm9 status = "okay"
assigned clocks: DCLK_VOP1=140.24MHz, VPLL=701.2MHz
```

### 2.5 配置和命令

`configs/z96a-rk3568-laptop-v2_defconfig`：

```text
CONFIG_VIDEO=y
CONFIG_DISPLAY=y
CONFIG_VIDEO_ROCKCHIP=y
CONFIG_DISPLAY_ROCKCHIP_EDP=y
CONFIG_PHY_ROCKCHIP_NANENG_EDP=y
CONFIG_DM_PWM=y
CONFIG_PWM_ROCKCHIP=y
CONFIG_BACKLIGHT=y
CONFIG_BACKLIGHT_PWM=y
CONFIG_PANEL=y
CONFIG_SIMPLE_PANEL=y
CONFIG_VIDEO_LOGO=y
# CONFIG_CONSOLE_MUX is not set
# CONFIG_Z96A_EDP_LOGO is not set
```

`board/rockchip/z96a_rk3568/Kconfig` 增加 `Z96A_EDP_LOGO`，默认 n。
`board.c` 增加 `z96a_logo` 命令 stub；`CONFIG_Z96A_EDP_LOGO=n` 时直接返回成功。
命令和手动 bind 代码只在 U-Boot proper 编译（`#ifndef CONFIG_SPL_BUILD`）。

## 3. 静态验证

### 3.1 U-Boot proper DT

```text
/vop@fe040000 status            disabled
/edp@fe0c0000 status            okay
/syscon@fdcb0000/edp-phy status okay
/panel compatible               simple-panel
/backlight compatible           pwm-backlight
vop assigned-clock-rates        140240000 701200000
vp1 endpoint                    endpoint@3

panel timing                    140240000, 1920x1080,
                                hfront 48 / hsync 32 / hback 124,
                                vfront 3 / vsync 6 / vback 11,
                                hsync 1 / vsync 0
/edp bpc                        8
backlight pwms                  phandle 176 = pwm@fe6f0010 (PWM9)
pwm9 status                     okay
```

### 3.2 SPL DT

```text
spl/dts/dt-spl.dtb 中没有 edp/vop/panel/backlight/pwm/regulator-vcc3v3-lcd 节点
```

### 3.3 SPL 二进制

P1 SPL 和新 SPL 都是 **142486 字节**。逐字节比较只有 **42 字节**不同，位置在版本字符串/构建元数据区域；SPL 代码和 SPL DT 未增加显示节点。  
idbloader 哈希变化来自这些构建元数据，不是新增显示代码。

### 3.4 USB 路径零影响

`CONFIG_CONSOLE_MUX` 和 `CONFIG_SYS_CONSOLE_IS_IN_ENV` 都未启用，`stdio` 会扫描 `UCLASS_VIDEO`。  
但 `vop` 节点是 `disabled`，DM 不会绑定它；eDP/PHY/panel/backlight/PWM 都不是 `UCLASS_VIDEO`。因此启动时没有 `UCLASS_VIDEO` 设备可 probe，显示栈不会在 USB 路径运行。

`z96a_logo` 命令存在但 `CONFIG_Z96A_EDP_LOGO=n`，属于 no-op；bootcmd 未修改。

## 4. 未验证 / 风险

```text
未刷写：idbloader / u-boot.itb 都在 build/out，eMMC 未动。
未运行：没有串口，阶段 1 只做编译和静态检查。
未验证：eDP PHY PLL、AUX、link training、panel 上电、背光、Linux 接管。
```

高风险点仍在阶段 2/3：

```text
Naneng eDP PHY PLL / AUX
eDP link training
VOP2 DCLK/VPLL 140.24MHz
panel 8bpc 与 AAAA 32-bit 输出匹配
失败清理是否足以让 Linux VOP2 正常接管
```

## 5. 下一步（阶段 2）

阶段 2 才修改 bootcmd：

```text
usb start;
setenv boot_targets usb;
bootflow scan -lb;
z96a_logo;
setenv boot_targets mmc1 mmc0;
bootflow scan -lb
```

并把 `CONFIG_Z96A_EDP_LOGO` 切到 y，先在 eMMC 路径验证显示 probe/AUX，再进入阶段 3 点屏。  
阶段 2 需要另行授权刷写或 RAM 测试；阶段 1 到此为止，不写 eMMC。

## 6. 复现构建

```bash
cd armbian-uboot
SOURCE_DATE_EPOCH=$(date +%s) ./compile.sh uboot \
  BOARD=z96a-v2-mainline BRANCH=mainline \
  DOWNLOAD_MIRROR=china EXPERT=yes \
  KERNEL_CONFIGURE=no RELEASE=bookworm
```

说明：不设置 `SOURCE_DATE_EPOCH` 时，Armbian 的 `dpkg-deb` 打包步骤会因为空时间戳失败；U-Boot 本身已经编译成功。  
`CONFIG_VIDEO_LOGO=y` 是为了满足 U-Boot 2026.01 `video-uclass.c` 对 `__splash_u_boot_logo_begin` 的无条件链接引用；因为 `vop` 节点 disabled，启动时没有 `UCLASS_VIDEO` 设备，所以它不会显示，也不会 probe 显示栈。阶段 4 会在 `z96a_logo` probe 后清屏并绘制自己的 BMP。
