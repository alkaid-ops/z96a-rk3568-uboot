# Z96A RK3568 定版 v3：交接不再关 VOP 输出（fitfix-v3）

日期：2026-11-12
状态：**eMMC 刷写 + 回读校验 + 重启实测通过**
基线：U-Boot 2026.01，`kemp233/u-boot-1` @ `8e474aa605a86de64316ba6da8dc089402976380`

## 1. 定版内容

| 项目 | 值 |
|---|---|
| 固件 ID | `fitfix-v3` |
| eMMC 写入位置 | sector `0x4000`（2516 扇区，止于 18900 < 分区起始 32768） |
| 文件 | `u-boot.itb`，1,288,192 字节（512 对齐） |
| SHA-256 | `56398fb629f0cafa05cea2c930c8562ce6969325b3ce45e197086118cb2b691f` |
| idbloader | **不变**：`35c368f3ad8d84acd756e4bb45bcbff56912b9b4a8892ac605ff35809bf683ac` |
| FIT 结构 | `firmware=atf-1`，`loadables=u-boot,atf-2..atf-6`，`fdt-1`（与 v2 一致） |
| 回滚 | fitfix-v2 `9e523af1…`，或 `/boot/z96a-B-backup/emmc-0x4000-pre-vopkeep.itb` |

源码补丁：

- `build/edp-logo/phase2/0016-z96a-edp-logo-final.patch`（v2 基线）
- `build/edp-logo/phase2/0017-z96a-keep-vop-scanout.patch`（本版新增）

## 2. 本版修的是什么

v2 的 `rk3568_vop` 带 `DM_FLAG_OS_PREPARE`。U-Boot 在 `bootm` 交接前执行
`dm_remove_devices_active()`，会调用 `rk3568_vop_remove()` → `rk_vop2_disable()`，
把 **eDP 实际输出通道 VP1 的 POST `DSP_CTRL` bit31（STANDBY）置 1**：

- 屏幕瞬间黑掉
- 面板需要重新锁定 eDP 链路（约 1 秒）
- 此后 Linux 侧 `z96a_handback` 虽然能在 0.12s 把 STANDBY 清掉，但链路重锁仍会可见

v3 把 `.flags` 改为 `0`：交接时不关闭任何输出，VOP 图层与 eDP 流保持，黑屏消失。

## 3. 与 v2 的字节级对比

| FIT 组件 | 结果 |
|---|---|
| `atf-1` … `atf-6` | 逐字节相同 |
| `fdt-1` | 逐字节相同 |
| `u-boot`（nodtb） | 仅 7 字节：6 字节构建时间串 + 1 字节 flags（`0x04 → 0x00`） |

受控重编（仅还原 `DM_FLAG_OS_PREPARE`）与 v3 对比同样只差 5 字节（时间串 + flags），
证明改动作用点唯一。

## 4. 刷写

```bash
dd if=u-boot.itb of=/dev/mmcblk0 bs=512 seek=16384 conv=fsync
dd if=/dev/mmcblk0 bs=512 skip=16384 count=2516 of=/tmp/v.itb
sha256sum /tmp/v.itb     # 56398fb6…
```

Windows RKDevTool / maskrom 步骤见 `build/out/fitfix-v3/FLASH-COMMANDS.txt`。

## 5. 实测

- 写入 → 回读 `56398fb6…` 一致
- 重启后再次回读仍一致
- 内核日志：`Z96HB pre: … port=1 post1_ctrl=00000000`
  → U-Boot 交接时未关输出（handback 自动 no-op）
- 显示 / WiFi / LED / X 均正常

## 6. 产物

- `build/out/fitfix-v3/`：`SHA256SUMS`、`REPORT.md`、`FLASH-COMMANDS.txt`、`z96a-uboot.its`
- GitHub Release `fitfix-v3`：`u-boot.itb`、`SHA256SUMS`、`0017` 补丁、刷写说明
