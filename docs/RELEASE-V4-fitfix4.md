# Z96A RK3568 定版 v4：simple-framebuffer 交接 + U 盘优先启动门控（fitfix-v4）

日期：2026-11-23（产物实际构建于 2026-11-16 17:50）
状态：**eMMC 上运行中，完整 2523 扇区回读 = 产物哈希**
基线：U-Boot 2026.01，`kemp233/u-boot-1` @ `8e474aa605a86de64316ba6da8dc089402976380`
补丁：`build/edp-logo/phase2/0018-z96a-nov16-bootfb-and-gating.patch`
（**单独应用，取代 0016+0017**；0016/0017 保留仅作 v2/v3 历史）

## 1. 定版内容

| 项目 | 值 |
|---|---|
| 固件 ID | `fitfix-v4` |
| eMMC 位置 | sector `0x4000`，2523 扇区（`16384..18906` < p1 起始 `32768`） |
| 文件 | `u-boot.itb`，1,291,776 字节 |
| SHA-256 | `4a2d6982d6846300de224d6178736b3a29d7a07512dc62eff12457bcde923cde` |
| `u-boot-nodtb.bin` | 1,018,848，`0d820a65…` |
| `u-boot.dtb` | 76,832，`29b2f31a…`（与 v3 完全相同） |
| `idbloader` | 不变 `35c368f3…` |
| LOGO | 不变 `z96a-logo.bin.gz` = `ec7e6366…` |

## 2. 本版新增

相对 fitfix-v3（ATF 与 DTB 逐字节相同，只有 U-Boot 本体 +3,160 字节）：

1. **`z96a-bootfb`**：启动时把 U-Boot framebuffer 以
   `/chosen` 的 `simple-framebuffer` + `reserved-memory`（`no-map`）写进传给 Linux 的 DTB，
   并配套 `skip_board_fixup`，让 Linux 早期就能拿到可用 framebuffer
   （与 LOGO→桌面/启动动画衔接直接相关）。
2. 启动门控：U 盘存在时优先从 U 盘启动，而不是停留在充电模式；
   充电模式仅在 `ON_SOURCE` 校验通过后进入；
3. 早期 LED 反馈；
4. 保持 v3 的关键修复：`rk3568_vop` 去掉 `DM_FLAG_OS_PREPARE`（`.flags = 0`），
   交接时不关 VP1 输出。

## 3. 校验

```bash
# 必须按 2523 扇区回读（v3 的 2516 只够 v3）
dd if=/dev/mmcblk0 bs=512 skip=16384 count=2523 | sha256sum
# 4a2d6982…
```

FIT 内部 8 个组件的 sha256 与载荷一致；ATF 1–6、控制 DTB 与 v3 完全相同。

## 4. 复现

见 `build/out/fitfix-v4/REPORT.md` §4。已验证补丁应用后源码树与原始构建树逐文件一致；
用 `aarch64-linux-gnu-gcc 14.2.0-19` 重编，nodtb 大小/版本/DTB 哈希一致，
二进制仅差构建时间与路径（字符串集只差编译器名与时间两行）。

> 因此 `u-boot.itb` 的哈希依赖构建目录/时间；发布以本 Release 资产为准，
> 复现校验请用 `SHA256SUMS` 中的 nodtb / dtb 哈希。

## 5. 刷写与回滚

- 刷写：`build/out/fitfix-v4/FLASH-COMMANDS.txt`
- 回滚：v3 `56398fb6…`（2516 扇区）或 v2 `9e523af1…`，写同一 `0x4000`

## 6. 待办（下一版）

- 方案 B：在 `rk_board_late_init` 中通过 DM I2C（`i2c@fdd40000` / `0x20` / `0xC4`）
  以 RMW 方式把 `vdd_npu`（RK809 DCDC_REG4）设为 **0.9V**（`0x20`），
  覆盖 extlinux/Armbian 脚本路径；届时 DTB 的 `vdd_npu` 应改回宽范围以保留 DVFS。
- 该改动需要一次 eMMC 引导区刷写（需单独授权）。
