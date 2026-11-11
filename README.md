# Z96A RK3568 Laptop V2 — 主线 U-Boot 内屏 LOGO 引导链

在 Sunniwell Z96A（RK3568 / 4 GB LPDDR4X / 14" BOE NV140FHM-N43 eDP 内屏）上，
用 **主线 U-Boot 2026.01** 实现开机内屏 LOGO，并生产化刷入 eMMC 引导区。

**定版：`fitfix-v2`** — 上电约 2 s 电源灯亮，≤3 s 屏幕亮起且**第一帧就是 LOGO**，
之后 USB 优先启动、无 U 盘时回退 eMMC。详见
[`docs/RELEASE-V2-fitfix2.md`](docs/RELEASE-V2-fitfix2.md)。

## 引导链

```text
BootROM → idbloader (DDR 1560MHz v1.13 + A SPL, sector 0x40)
        → FIT (sector 0x4000): TF-A atf-1 → U-Boot + 控制 DTB
        → z96a_logo（eMMC /boot/z96a-logo.bin.gz → gunzip → framebuffer → 背光）
        → bootflow scan（USB 优先 → eMMC 回退）
```

| 组件 | 标识 |
|---|---|
| idbloader | `35c368f3ad8d84acd756e4bb45bcbff56912b9b4a8892ac605ff35809bf683ac` |
| U-Boot FIT (`fitfix-v2`) | `9e523af1b197451d7b8197dae7f701dcce91678de4a216b214b6acbe62efb838` |
| LOGO (`z96a-logo.bin.gz`) | `ec7e6366...`（1,445,947 字节，位于 eMMC p1 `/boot`） |
| 回滚 FIT (P1) | `3dd682033885ed88c3c95fa02d152747bf577fadfd678372f59304dcc7f215ea` |

## 仓库结构

| 路径 | 内容 |
|---|---|
| `docs/` | 定版说明、DDR/eMMC 事实、原厂分析、充电/LED 记录 |
| `build/edp-logo/` | eDP LOGO 移植计划、端口映射、**Phase 2 定版补丁** |
| `build/out/` | 各阶段产物与 `SHA256SUMS`、刷写说明 |
| `build/usb/` | RAM 链式测试包与诊断记录（含 FIT 根因分析） |
| `build/recovery-20261111/` | maskrom 恢复包（P1 FIT、idbloader、MiniLoaderAll） |
| `build/patches/` | 早期 U-Boot 板级补丁系列（0001–0005） |
| `analysis/`、`artifacts/` | 原厂二进制静态分析数据（大文件不入 Git） |
| `armbian-uboot/` | Armbian 构建配方中的 Z96A 板级配置与内核补丁 |

大二进制（FIT、idbloader、DTB、镜像）不进 Git，随 GitHub Release 发布并在
`SHA256SUMS` 中登记。

## 复现构建

```sh
git clone https://github.com/kemp233/u-boot-1.git u-boot
cd u-boot && git checkout 8e474aa605a86de64316ba6da8dc089402976380
git apply /path/to/build/edp-logo/phase2/0016-z96a-edp-logo-final.patch

make z96a-rk3568-laptop-v2_defconfig CROSS_COMPILE=aarch64-linux-gnu-
make -j$(nproc) CROSS_COMPILE=aarch64-linux-gnu-

mkimage -f z96a-uboot.its u-boot.itb   # 模板见 build/out/fitfix-v2/
truncate -s %512 u-boot.itb
```

binman 会报 `rockchip-tpl`/`tee-os` fake blob —— 这是预期的，本方案不使用
binman 的 `simple-bin`，只需 `u-boot-nodtb.bin` 与 `u-boot.dtb` 手工打包。

## 刷写与恢复

刷写步骤、预期画面、回滚命令见
[`build/out/fitfix-v2/FLASH-COMMANDS.txt`](build/out/fitfix-v2/FLASH-COMMANDS.txt)
与 [`docs/RELEASE-V2-fitfix2.md`](docs/RELEASE-V2-fitfix2.md)。

**永远不要**在未授权情况下写 eMMC 引导区；eMMC 写入必须逐次取得明确授权。
写引导区前先只读备份目标扇区，写后用 `O_DIRECT` 回读校验哈希。

## 手工 FIT 的关键约束

手工 ITS 必须显式包含：

```dts
atf-1 { load = <0x00040000>; entry = <0x00040000>; };
u-boot { os = "u-boot"; };
```

缺失 `entry` 会让 SPL 跳过 TF-A 直接跳 U-Boot，缺失 `os` 会让 SPL 丢弃控制 DTB，
两者都会使板子完全静默。RAM 链式测试（`go` 裸跳）不解析 FIT，无法发现此类缺陷。
根因分析见 [`build/usb/phase2-ramtest-emmc/DIAGNOSTICS-2026-11-11.md`](build/usb/phase2-ramtest-emmc/DIAGNOSTICS-2026-11-11.md)。

## 已知限制

- eMMC HS200 限速 50 MHz（USB Devuan DTB 已改，原文件备份 `.z96a-bak`）；
  eMMC p2 ext4 journal 仍有写 I/O 错误，只能 `ro,noload` 挂载，不影响引导链。
- `CONFIG_BOOTDELAY=0`：U-Boot 阶段无法按键打断进 shell；USB/eMMC/maskrom 不受影响。
- 生产构建不含 DIAG 的 USB 直读 LOGO、状态 `fatwrite`、观察窗口与 BIST。
