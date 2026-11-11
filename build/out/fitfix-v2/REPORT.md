# Z96A fitfix-v2 产物报告

日期：2026-11-12
配置：`z96a-rk3568-laptop-v2_defconfig`（`Z96A_EDP_LOGO=y`，`Z96A_DISPLAY_DIAG` 未启用）
基线：U-Boot 2026.01 @ `8e474aa605a86de64316ba6da8dc089402976380` + `build/edp-logo/phase2/0016-z96a-edp-logo-final.patch`

## 产物

| 文件 | 大小 | SHA-256 | 是否入 Git |
|---|---|---|---|
| `u-boot.itb` | 1,288,192 | `9e523af1b197451d7b8197dae7f701dcce91678de4a216b214b6acbe62efb838` | 否（GitHub Release 资产） |
| `u-boot-nodtb.bin` | 1,015,688 | `85f0d5f280b515c10519ef46086ce1aa69105672529699db859d2833d1703392` | 否 |
| `u-boot.dtb` | 76,832 | `29b2f31abe72ca23d6264241f17ce492dfb58183e9c193ef3d794c17b2031ab0` | 否 |
| `z96a-uboot.its` | 2,708 | `e55277903dd517650fa78cc0b013d573245e73c026b10f1a38feaace74becfbf` | 是 |
| `SHA256SUMS` | — | — | 是 |

## 校验

```text
FIT magic            d00dfeed
FIT 大小             1,288,192 = 512 × 2516（扇区对齐）
images               u-boot, atf-1..atf-6, fdt-1
config-1             firmware=atf-1, fdt=fdt-1, loadables=u-boot, atf-2..atf-6
atf-1                load=0x00040000 entry=0x00040000
u-boot               os="u-boot" load=0x00800000 entry=0x00800000
bootcmd              尾部 = gpio s A6;gpio s E11;gpio s E9;gpio s C31;gpio s A27;
                             z96a_logo;usb start;bootflow scan -lb
```

`dtc -I dtb -O dts` 反编译核对通过（`entry`/`os` 两项为必需，缺失会导致 SPL 跳过 TF-A 或不附加 DTB）。

## 相对 fitfix-v1 的差异

- `CONFIG_BOOTDELAY 3 → 0`
- 生产 bootcmd：`z96a_logo` 移到 `usb start` 之前（DIAG 构建保留 `usb start;` 前缀）
- `rk_edp.c` 面板上电等待 `mdelay(200) → mdelay(100)`

## 刷写

见 `FLASH-COMMANDS.txt`。目标 eMMC sector `0x4000`，idbloader `35c368f3...` 不动，
回滚使用 `build/recovery-20261111/u-boot-p1.itb`（`3dd68203...`）。
