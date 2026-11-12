# Z96A fitfix-v3 产物报告

日期：2026-11-12
基线：U-Boot 2026.01 @ `8e474aa605a86de64316ba6da8dc089402976380`
补丁：`0016-z96a-edp-logo-final.patch` + `0017-z96a-keep-vop-scanout.patch`
状态：**eMMC 刷写 + 回读校验 + 重启实测通过**

## 1. 与 fitfix-v2 的差异（逐组件验证）

| FIT 组件 | v2 | v3 | 结果 |
|---|---|---|---|
| `u-boot`（nodtb） | 1,015,688 B | 1,015,688 B | 仅 7 字节不同 |
| `atf-1` … `atf-6` | — | — | **逐字节相同** |
| `fdt-1` | 76,832 B | 76,832 B | **逐字节相同** |

nodtb 的 7 字节差异：6 字节为构建版本/时间字符串，**1 字节为 `DM_FLAG_OS_PREPARE`
标志位**（offset 884650，`0x04 → 0x00`）。除此之外没有任何代码或配置漂移。

受控重编（仅把 `.flags` 换回 `DM_FLAG_OS_PREPARE`）与 v3 二进制对比同样只差这 5 字节
（4 字节时间串 + 1 字节 flags），证明补丁作用点唯一、可控。

## 2. 行为变化

v2 的 `rk3568_vop` 带 `DM_FLAG_OS_PREPARE`，U-Boot 在 `bootm` 交接前会执行
`dm_remove_devices_active()` → `rk_vop2_disable()`，把 **eDP 实际输出通道 VP1 的
POST `DSP_CTRL` bit31（STANDBY）置 1**，屏幕黑掉，面板需要重新锁定链路（约 1 秒）。

v3 去掉该标志（`.flags = 0`），交接时不再关闭任何输出，VOP 图层与 eDP 视频流保持，
**LOGO→Linux 交接不再黑屏/不闪**。

刷写后内核侧实测：

```
Z96HB pre: esmart0_mst=00000001 dsp_en=00004008 port=1
          post1_ctrl=00000000        <- U-Boot 没有关输出
```

（内核 `z96a_handback` 仍保留，作为未来若回到 v2 类 U-Boot 时的兜底；在 v3 上自动变为
no-op，只清 VP0/VP2 未用通道的历史位。）

## 3. 产物

| 文件 | 大小 | SHA-256 | 是否入 Git |
|---|---|---|---|
| `u-boot.itb` | 1,288,192 | `56398fb629f0cafa05cea2c930c8562ce6969325b3ce45e197086118cb2b691f` | 否（GitHub Release 资产） |
| `u-boot-nodtb.bin` | 1,015,688 | `f0207078aa9fb6662be30720e10f4789e16eae1f2c6afaec8f66d4423ed8f4e9` | 否 |
| `u-boot.dtb` | 76,832 | `29b2f31abe72ca23d6264241f17ce492dfb58183e9c193ef3d794c17b2031ab0` | 否 |
| `z96a-uboot.its` | 2,708 | `e55277903dd517650fa78cc0b013d573245e73c026b10f1a38feaace74becfbf` | 是 |

`idbloader` **不变**：`35c368f3ad8d84acd756e4bb45bcbff56912b9b4a8892ac605ff35809bf683ac`
（vendor v1.13 DDR 1560 MHz + A SPL HS200）。

## 4. 实测记录（eMMC 0x4000）

```
写入：dd if=u-boot.itb of=/dev/mmcblk0 bs=512 seek=16384 conv=fsync
回读：dd if=/dev/mmcblk0 bs=512 skip=16384 count=2516 | sha256sum
      56398fb6…  （与产物一致）
重启后再次回读：56398fb6…（一致）
```

FIT 内部 8 个组件的 sha256 全部与载荷一致；写入范围 `16384..18899`，远小于
分区起始 `32768`，未触碰 GPT / idbloader / 分区表。

## 5. 回滚

- 回 `fitfix-v2`：写 `9e523af1b197451d7b8197dae7f701dcce91678de4a216b214b6acbe62efb838`
  （GitHub Release `fitfix-v2` 的 `u-boot.itb`）。
- 或写设备备份 `/boot/z96a-B-backup/emmc-0x4000-pre-vopkeep.itb`
  （sha256 `05d998c5…`，带诊断 `sleep 10` 的过渡版本）。
- 均为 sector `0x4000`、`2516` 扇区、`conv=fsync`。
