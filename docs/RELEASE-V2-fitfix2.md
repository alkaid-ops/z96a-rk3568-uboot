# Z96A RK3568 定版 v2：eDP 内屏 LOGO 引导链（fitfix-v2）

日期：2026-11-12
状态：**定版产物，eMMC 刷写实测待执行**（RAM 链式加载路径与 fitfix-v1 已实测通过）
基线：U-Boot 2026.01，`kemp233/u-boot-1` @ `8e474aa605a86de64316ba6da8dc089402976380`

## 1. 定版内容

| 项目 | 值 |
|---|---|
| 固件 ID | `fitfix-v2` |
| eMMC 写入位置 | sector `0x4000`（8 MiB，即 16 MiB 分区前的 U-Boot 区） |
| 文件 | `u-boot.itb`，1,288,192 字节（512 扇区对齐） |
| SHA-256 | `9e523af1b197451d7b8197dae7f701dcce91678de4a216b214b6acbe62efb838` |
| idbloader | **不变**：`35c368f3ad8d84acd756e4bb45bcbff56912b9b4a8892ac605ff35809bf683ac`（vendor v1.13 DDR 1560 MHz + A SPL HS200） |
| FIT 结构 | `firmware=atf-1`，`loadables=u-boot,atf-2..atf-6`，`fdt-1`（与已验证的 P1 一致） |
| 回滚 | maskrom 写 `build/recovery-20261111/u-boot-p1.itb`（`3dd68203...`）到 `0x4000` |

FIT 打包模板：`build/out/fitfix-v2/z96a-uboot.its`（**必须沿用**，见 §4）。
源码补丁：`build/edp-logo/phase2/0016-z96a-edp-logo-final.patch`（相对上述基线 `8e474aa`）。

## 2. 刷写步骤（Windows RKDevTool / maskrom）

1. 设备进 maskrom，打开 RKDevTool v3.37。
2. 「高级功能」加载 `MiniLoaderAll.bin`，点下载到 RAM 地址 `0xCCCCCCCC`（只进 RAM，不动 flash）。
3. 「下载镜像」清空表格，只加一行：
   - 路径：`u-boot.itb`
   - 地址：`0x4000`（单位：扇区）
   - 勾选 **强制按地址写**
4. 执行，重启。
5. **不要**写 GPT / parameter / 分区表；**不要**动 idbloader（`0x40`）。

写入后可用 USB Devuan 以 `O_DIRECT` 回读 sector 16384 起 2516 个扇区，校验 SHA-256 必须等于 `9e523af1...`。

## 3. 预期表现

| 时间点 | 现象 |
|---|---|
| 0 s | 上电，屏幕全黑（背光故意不开） |
| ~2 s | 电源灯亮（前置固定成本：BootROM + DDR 1560 MHz + SPL + TF-A + U-Boot） |
| ≤3 s | **屏幕亮起，第一帧就是 LOGO**（无白屏、无彩条、无黑底亮屏） |
| 之后 | 正常启动系统：USB 优先，无 U 盘时回退 eMMC |

第一帧即 LOGO 的机制：`z96a_logo_display()` 在显示探测**之前**置 `z96a_defer_backlight = 1`，VOP2/eDP 初始化期间背光保持关闭；LOGO（或加载失败时的白色 fallback）进入帧缓冲后才调用 `z96a_edp_backlight_on()`。因此亮屏的第一帧不可能是未初始化内容。

本版相对 fitfix-v1 的提速项：

| 改动 | 节省 |
|---|---|
| `CONFIG_BOOTDELAY 3 → 0` | ~3 s |
| `z96a_logo` 提前到 `usb start` 之前（生产构建） | USB 枚举 ~0.5–1.5 s |
| eDP 面板上电等待 `mdelay(200) → mdelay(100)` | 0.1 s |

## 4. 手工 FIT 打包的硬性约束（四次刷写全灭的根因）

手工 ITS 必须显式包含以下两个属性，缺一即不可启动：

```dts
atf-1 {
    ...
    load  = <0x00040000>;
    entry = <0x00040000>;      /* 缺此项 SPL 会跳第一个 loadable（0x800000），跳过 TF-A */
};

u-boot {
    ...
    os    = "u-boot";          /* 缺此项 SPL 不把 fdt-1 附加到 U-Boot 尾部 */
};
```

原理见 `common/spl/spl_fit.c`：

- `atf-1` 无 `entry` 时，SPL 按 `:919-921` 的注释回退到**第一个 loadable** 的 load 地址作为跳转目标，于是直接跳 `0x800000` 裸进 U-Boot，ATF 从未完成安全世界/DRAM 交接 → 板子完全静默。
- `u-boot` 无 `os` 时，`os_takes_devetree()`（`:939-940`）为假，SPL 不附加控制 DTB。

binman 生成 P1 时会自动写这两个属性，所以只有 P1 一直能启动。手工打包后**必须**用 `dtc -I dtb -O dts` 反编译核对 `atf-1 entry` 与 `u-boot os`，并补齐 512 扇区整数倍。RAM 链式测试（`go 0x10000000`）不解析 FIT，因此**无法**暴露此类缺陷。

## 5. 复现构建

```sh
# 1) 基线 + 补丁
git clone https://github.com/kemp233/u-boot-1.git u-boot && cd u-boot
git checkout 8e474aa605a86de64316ba6da8dc089402976380
git apply /path/to/build/edp-logo/phase2/0016-z96a-edp-logo-final.patch

# 2) 构建（binman 的 simple-bin 会因缺少 rockchip-tpl/tee-os 报 fake blob，
#    这是预期；我们手工打包，只需 u-boot-nodtb.bin 与 u-boot.dtb）
make z96a-rk3568-laptop-v2_defconfig CROSS_COMPILE=aarch64-linux-gnu-
make -j$(nproc) CROSS_COMPILE=aarch64-linux-gnu-

# 3) 手工 FIT（模板已含 entry/os 修复）
mkimage -f z96a-uboot.its u-boot.itb
truncate -s %512 u-boot.itb
```

## 6. 已知限制

- `CONFIG_BOOTDELAY=0` 后，U-Boot 阶段无法按任意键打断进 shell；USB 启动、eMMC 回退与 maskrom 恢复均不受影响。
- eMMC p2 ext4 journal 仍有写 I/O 错误（仅 `ro,noload` 可挂载），属独立问题，不影响引导链。
- eMMC HS200 目前限速 50 MHz（USB Devuan 的 DTB 中已改，原文件备份为 `.z96a-bak`）。
- 本版未加启动计时戳；若实测仍 >3 s，下一轮按 `z96a-boottime.txt`（`timer_get_us` + `fatwrite`）数据裁剪。

## 7. 变更记录

| 版本 | 内容 | 结果 |
|---|---|---|
| `logo-mmc-v1` / `logo-embed-v1` | LOGO 运行时加载方案 | RAM 实测通过（用户目视确认无白帧） |
| `fitfix-v1` | 修正 its 的 `entry`/`os` 后重打包 | eMMC 刷入成功：2 s 电源灯，8 s LOGO，USB/eMMC 正常 |
| **`fitfix-v2`（本版）** | `BOOTDELAY=0`、LOGO 先于 `usb start`、eDP 等待 100 ms | 产物完成，待刷写实测 |
