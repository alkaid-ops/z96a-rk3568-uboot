# 原厂 U-Boot 语义 → 主线 U-Boot 落点

这是移植种子，不是已编译补丁。

## 当前主线板级落点

源码：

```text
u-boot-mainline-src/board/rockchip/z96a_rk3568/board.c
u-boot-mainline-src/arch/arm/dts/rk3568-z96a-laptop-v2.dts
u-boot-mainline-src/arch/arm/dts/rk3568-z96a-laptop-v2-u-boot.dtsi
u-boot-mainline-src/configs/z96a-rk3568-laptop-v2_defconfig
```

`rk_board_late_init()` 当前主要执行 SC8886 `z96a-pwr`：

```text
读 0x20 / 0x21 / 0x26
写 PROCHOT 0x36 / 0x37
清 EN_OTG / EN_LWPWR
AC 存在时写 charge current 0x14
```

## 原厂路径需要继续核实/移植

```text
0x00a03e14 -> 0x00a03064 -> 0x00a02d10
0x00a7dcc0 -> 0x00a7e3bc
```

建议新增独立函数：

```c
static int z96a_factory_power_gpio_init(void);
static int z96a_factory_charger_init(struct udevice *chip, u16 status);
```

不要直接把 2017.09 的反编译代码整体粘贴进 2026.01。需要先确认：

1. 全局 GPIO 编号 `0x15 / 0x50 / 0` 在当前 DTS 中能否用 `gpio_request` 获取；
2. 当前板级 pinctrl 是否与 2017 原厂 U-Boot 的默认 pinmux 一致；
3. 这些 GPIO 是否已经被 regulator、USB、HUSB311 节点占用；
4. 正常启动路径是否执行充电初始化，还是只在 charge boot mode 执行；
5. 原厂 `preboot=charge` / charge animation 是否改变进入 Linux 前的状态。

## 建议的首个移植范围

只增加早期 GPIO 初始化，不改变现有 USB-first、extlinux、eDP、Linux 设备树：

```c
/* pseudo-code, API details to verify */
static int z96a_factory_power_gpio_init(void)
{
    return 0; /* implement after confirmed mapping */
}
```

等静态差异明确后，再决定是否移植完整 BQ257xx framework、充电动画和 Logo。每项都必须可独立开关、可回退。
