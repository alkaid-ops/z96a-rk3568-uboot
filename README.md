# Z96A RK3568 Laptop V2 — 主线 U-Boot 内屏 LOGO 引导链

在 Sunniwell Z96A（RK3568 / 4 GB LPDDR4X / 14" BOE NV140FHM-N43 eDP 内屏）上，
用 **主线 U-Boot 2026.01** 实现开机内屏 LOGO，并生产化刷入 eMMC 引导区。

**当前定版：`fitfix-v4`** — 上电约 2 s 电源灯亮，≤3 s 屏幕亮起且**第一帧就是 LOGO**；
LOGO 一直显示到 Linux 接管，**交接不再黑屏**。相对 v3，本版新增：

- **`z96a-bootfb`**：把 U-Boot framebuffer 以 `/chosen` 的 `simple-framebuffer` +
  `reserved-memory(no-map)` 写进 Linux DTB（配合 `skip_board_fixup`），
  让 Linux 早期就有可用 framebuffer；
- **U 盘优先启动门控**：检测到 U 盘时先从 U 盘启动，而不是停在充电模式；
  充电模式仅在 `ON_SOURCE` 校验通过后进入；
- 早期 LED 反馈。

- 发布页：<https://github.com/alkaid-ops/z96a-rk3568-uboot/releases/tag/fitfix-v4>
- 定版说明：[`docs/RELEASE-V4-fitfix4.md`](docs/RELEASE-V4-fitfix4.md)
- 上一版：[`docs/RELEASE-V3-fitfix3.md`](docs/RELEASE-V3-fitfix3.md)（回滚用）
- 再上一版：[`docs/RELEASE-V2-fitfix2.md`](docs/RELEASE-V2-fitfix2.md)

## 引导链

```text
BootROM → idbloader (DDR 1560MHz v1.13 + A SPL, sector 0x40)
        → FIT (sector 0x4000): TF-A atf-1 → U-Boot + 控制 DTB
        → z96a_logo（eMMC BOOT 分区 /z96a-logo.bin.gz → gunzip → framebuffer → 背光）
        → （v4）z96a-bootfb：把 framebuffer 作为 simple-framebuffer 写进 Linux DTB
        → bootflow scan（USB 优先 → eMMC 回退；v4 含充电模式门控）
```

`z96a_logo` 在 `usb start` **之前**执行，因此 LOGO 显示不依赖 U 盘。

| 组件 | 标识 |
|---|---|
| idbloader（不变） | `35c368f3ad8d84acd756e4bb45bcbff56912b9b4a8892ac605ff35809bf683ac` |
| U-Boot FIT `fitfix-v4`（2523 扇区） | `4a2d6982d6846300de224d6178736b3a29d7a07512dc62eff12457bcde923cde` |
| U-Boot FIT `fitfix-v3`（回滚，2516 扇区） | `56398fb629f0cafa05cea2c930c8562ce6969325b3ce45e197086118cb2b691f` |
| U-Boot FIT `fitfix-v2`（回滚） | `9e523af1b197451d7b8197dae7f701dcce91678de4a216b214b6acbe62efb838` |
| LOGO `z96a-logo.bin.gz` | `ec7e63662a764028881510940980f7773b9f9f9a18b2c7b2e96567337341f6fa` |
| 回滚 FIT（最早 P1） | `3dd682033885ed88c3c95fa02d152747bf577fadfd678372f59304dcc7f215ea` |

## LOGO 资产：位置与规格（必须放对，否则白屏）

生产版 U-Boot（v2/v3/v4）**只从 eMMC 读取 LOGO**，没有 USB 直读回退。缺失、损坏或尺寸
不符时 U-Boot 会回退为**白色填充**（即“白屏”现象）。

- **分区**：eMMC 的 BOOT 分区 = `mmcblk0p1`，FAT，LABEL `BOOT`（496 MB）
- **路径**：**该分区根目录**

```text
/z96a-logo.bin.gz
```

在 Linux 中把该分区挂到 `/boot` 后，就是 `/boot/z96a-logo.bin.gz`
（注意不是 `/boot/rockchip/...` 之类的子目录）。

- **文件规格**：
  - gzip 压缩
  - 解压后必须**精确等于 8,294,400 字节** = `1920 × 1080 × 4`（BGRA / ARGB8888）
  - 获取方式：GitHub Release `fitfix-v3/v4` 资产 `z96a-logo.bin.gz`
    （1,445,947 字节；本地产物同在 `build/logo/`，大二进制不入 Git）
- **校验**：

```sh
sha256sum z96a-logo.bin.gz     # ec7e6366...
gzip -l z96a-logo.bin.gz       # uncompressed 8294400
```

- **更新方式**（设备内）：

```sh
mount -t vfat /dev/mmcblk0p1 /mnt     # 已挂载到 /boot 时可直接 cp
cp z96a-logo.bin.gz /mnt/z96a-logo.bin.gz
sync
umount /mnt
```

- 说明：旧诊断版 U-Boot 会先尝试 `fatload usb 0:1 … z96a-logo.bin`（U 盘根目录的
  **未压缩** 8,294,400 字节文件），所以过去 LOGO 能从 U 盘读出；生产版已移除该路径。

## 版本演进

| 版本 | 关键变化 |
|---|---|
| `fitfix-v2` | eDP LOGO 引导链；`rk3568_vop` 带 `DM_FLAG_OS_PREPARE`（交接会黑 ~1s） |
| `fitfix-v3` | `.flags = 0`，交接不再关 VP1 输出（无交接黑屏）；硬件 RNG/无 initrd 属内核侧 |
| `fitfix-v4` | + `z96a-bootfb`（simple-framebuffer 交接）、U 盘优先启动门控、早期 LED |

字节级关系：v4 与 v3 的 ATF 1–6、控制 DTB **逐字节相同**；只有 U-Boot nodtb
+3,160 字节。详见 `docs/RELEASE-V4-fitfix4.md`。

## 仓库结构

| 路径 | 内容 |
|---|---|
| `docs/` | 定版说明、DDR/eMMC 事实、原厂分析、充电/LED 记录 |
| `build/logo/` | LOGO 资产 `z96a-logo.bin.gz`（eMMC BOOT 分区根目录用） |
| `build/edp-logo/` | eDP LOGO 移植计划、端口映射、Phase 2 补丁（**v4 用 0018**） |
| `build/out/` | 各阶段产物与 `SHA256SUMS`、刷写说明（`fitfix-v4/` 为当前定版） |
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

# fitfix-v4（2026-11-16 定版）：单独应用 0018，它已包含 eDP LOGO、
# bootfb、启动门控与 flags=0；0016/0017 仅用于复现 v2/v3。
git apply /path/to/build/edp-logo/phase2/0018-z96a-nov16-bootfb-and-gating.patch

make z96a-rk3568-laptop-v2_defconfig ARCH=arm CROSS_COMPILE=aarch64-linux-gnu-
make -j$(nproc) ARCH=arm CROSS_COMPILE=aarch64-linux-gnu- u-boot-nodtb.bin u-boot.dtb

mkimage -f z96a-uboot.its u-boot.itb   # 模板见 build/out/fitfix-v4/
truncate -s %512 u-boot.itb
```

已验证：0018 应用后源码树与原始构建树**逐文件一致**；用
`aarch64-linux-gnu-gcc 14.2.0-19` 重编，nodtb 大小/版本/DTB 哈希一致，
二进制仅差构建时间与路径。因此 `u-boot.itb` 的哈希与构建目录/时间有关，
发布以 Release 资产为准，复现校验用 `SHA256SUMS` 里的 nodtb/dtb 哈希。

binman 会报 `rockchip-tpl`/`tee-os` fake blob —— 这是预期的，本方案不使用
binman 的 `simple-bin`，只需 `u-boot-nodtb.bin` 与 `u-boot.dtb` 手工打包。

## 刷写与恢复

- 当前定版（v4）刷写步骤：[`build/out/fitfix-v4/FLASH-COMMANDS.txt`](build/out/fitfix-v4/FLASH-COMMANDS.txt)
- 定版说明：[`docs/RELEASE-V4-fitfix4.md`](docs/RELEASE-V4-fitfix4.md)
- 回滚：v3 `56398fb6…` / v2 `9e523af1…`，写同一位置 `0x4000`

v4 写入 2523 扇区（`16384..18906`），仍在 p1 起始 `32768` 之前，**不碰**
idbloader / GPT / 分区表。

> 回读校验必须用 `count=2523`；用 `count=2516`（v3 的尺寸）会得到截断前缀
> `94d850a3…`，看起来"不一致"，实际是少读了 7 个扇区。

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

- **LOGO 必须在 eMMC BOOT 分区根目录且规格正确**，否则白屏（生产版无 USB 直读回退）。
- v4 的 FIT 是 2523 扇区（1,291,776 B）；v3 是 2516 扇区。回读/回滚时注意尺寸不同。
- U-Boot 源码分支当前尚未发布到公开远端（63 个提交只在本地），
  因此发布仓以 `0018` 补丁形式固化完整源码；后续改动应继续以补丁提交。
- eMMC HS200 限速 50 MHz（USB Devuan DTB 已改，原文件备份 `.z96a-bak`）；
  eMMC p2 ext4 journal 仍有写 I/O 错误，只能 `ro,noload` 挂载，不影响引导链。
- `CONFIG_BOOTDELAY=0`：U-Boot 阶段无法按键打断进 shell；USB/eMMC/maskrom 不受影响。
- 生产构建不含 DIAG 的 USB 直读 LOGO、状态 `fatwrite`、观察窗口与 BIST。
