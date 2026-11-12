# Z96A eDP LOGO — Phase 2（C 阶段定版补丁）

本目录是 eDP 内屏 LOGO 引导链的**完整源码补丁**，相对一个干净基线可直接应用。

## 基线

| 项目 | 值 |
|---|---|
| 仓库 | `https://github.com/kemp233/u-boot-1.git` |
| 提交 | `8e474aa605a86de64316ba6da8dc089402976380` |
| 版本 | U-Boot 2026.01 |

## 应用

```sh
git checkout 8e474aa605a86de64316ba6da8dc089402976380
git apply 0016-z96a-edp-logo-final.patch
git apply 0017-z96a-keep-vop-scanout.patch
```

`0017`：去掉 `rk3568_vop` 的 `DM_FLAG_OS_PREPARE`，交接时不再置 VP1 POST STANDBY，
消除 U-Boot→Linux 的链路重锁黑屏（fitfix-v3）。

补丁包含 Phase 1（显示栈移植）与 Phase 2（LOGO 运行时加载、背光延迟、启动提速）
的最终态，共 24 个文件。它**取代**了早期的
`armbian-uboot/patch/u-boot/legacy/board_z96a-v2-mainline/0015-z96a-edp-logo-phase1.patch`
对最终树的描述（0015 只覆盖 Phase 1，且基线已前进，无法单独干净应用）。

## 补丁要点

- VOP2 / Naneng eDP PHY / eDP 控制器驱动移植，VOP 工作配置与 Linux 运行态逐位一致。
- eDP 视频时序寄存器显式编程（`total/active/v/h` 系列），`STRM_VALID` 可在无像素时误报的缺陷被排除。
- `z96a_logo` 命令：eMMC `load mmc 0:1` 读取 `z96a-logo.bin.gz` → `gunzip` → 帧缓冲 → 背光。
- 背光延迟：`z96a_defer_backlight` 使亮屏第一帧必须是 LOGO（或白色 fallback）。
- 生产提速：`CONFIG_BOOTDELAY=0`、`z96a_logo` 先于 `usb start`、eDP 面板等待 200→100 ms。
- DIAG 构建（`z96a-rk3568-laptop-v2-diag_defconfig`）保留 USB 直读 LOGO、状态 `fatwrite`、
  观察窗口与 BIST，仅供 RAM 测试；生产 defconfig 不含这些。

## 产物

定版镜像与刷写步骤见 `build/out/fitfix-v2/`，原理与时间线见
`docs/RELEASE-V2-fitfix2.md`。

## 硬性约束

手工 FIT 的 ITS 必须显式写 `atf-1 entry = <0x00040000>` 与 `u-boot os = "u-boot"`，
否则 SPL 会跳过 TF-A 或丢弃控制 DTB，板子完全不启动。详见
`build/usb/phase2-ramtest-emmc/DIAGNOSTICS-2026-11-11.md` 的根因段落。
