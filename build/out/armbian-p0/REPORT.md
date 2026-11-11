# Z96A U-Boot P0 构建报告（未刷写）

日期：2026-11-11
基底：Armbian 2026.01 U-Boot（`kemp233/u-boot-1` @ `8e474aa605a86de64316ba6da8dc089402976380`）
补丁：`armbian-uboot/patch/u-boot/legacy/board_z96a-v2-mainline/0005..0011`
构建：`./compile.sh uboot BOARD=z96a-v2-mainline BRANCH=mainline`，Docker，gcc 11.4.0，
`SOURCE_DATE_EPOCH=1793132605`

## 产物

| 文件 | 大小 | SHA-256 |
|---|---|---|
| `idbloader.img` | 202752 | `7d799106bd6942bd57e25c56d7a1c89cd354ab836bdeeed3a1ddcee86c09fe4f` |
| `u-boot.itb` | 1220096 | `2e252a339c97306610a1a69741811a854e0d72fabfb5f367956dddc4ef8c49d8` |

对照（当前已部署 B2）：

```text
B2  idbloader.img  7d799106bd6942bd57e25c56d7a1c89cd354ab836bdeeed3a1ddcee86c09fe4f   <- 完全相同
B2  u-boot.itb     4d409961e7145b445e140f763c8bbbfa21bdea9897e62e034e4599634599eb55
```

## P0 改动（唯一）

`board/rockchip/z96a_rk3568/board.c`：SC8886 充电电流写入从 BQ25700 的 `0x14`
改为 BQ25703 的 `0x02`，掩码从 `0x7f00` 改为 `0x1fc0`，值仍为 `0x0400`（1024 mA）。

原厂反编译依据：`0xa7e3bc` 对 SC8886/BQ25703 写 `0x02 = 0x0400`。
现场读回：`0x02 = 0x03c0`（有效），`0x14 = 0xffff`（无效）。

## 静态验证结果

1. **idbloader 与 B2 逐字节相同**（`cmp` 通过）→ SPL / DDR / BL31 未变。
2. **FIT 结构未变**：`dumpimage -l` 对比 B2，只有 `uboot` 的 hash 行不同，
   ATF 1..6、OP-TEE、fdt 的 hash 与大小完全一致。
3. **U-Boot payload 只差 4 字节**（2 条指令）：

```text
原（B2）  0x804420: mov w2, #0x7f00
           0x804424: mov w1, #0x14
新（P0）  0x804420: mov w2, #0x1fc0
           0x804424: mov w1, #0x2
```

4. **启动链未改**：payload 中 bootcmd 仍为

```text
bootcmd=gpio s A6;gpio s E11;gpio s E9;gpio s C31;gpio s A27;usb start;bootflow scan -lb
```

`BOOT_TARGETS`、`preboot`、idbloader、GPT 均未改动。

## P0-2（`CONFIG_SYSRESET_PSCI=y`）被推迟

试验发现它会改 SPL：

```text
drivers/sysreset/Kconfig:
  config SYSRESET_PSCI
      select SPL_ARM_PSCI_FW if SPL_SYSRESET

当前 defconfig 已有 CONFIG_SPL_SYSRESET=y
```

结果：

```text
idbloader.img  202752 -> 204800（+2 KiB，SPL 里多了 PSCI FW 代码）
hash           7d799106... -> 367d32ac...（不再与现场一致）
```

SPL 运行在 ATF 之前，PSCI 固件尚不存在，因此给 SPL 加 PSCI 重置没有实际意义，
而且破坏了“idbloader 不变”这条护栏。所以本次不包含该项。

可选方案（需要时再定）：

```text
A. 接受 SPL 变化，单独评估并刷 idbloader
B. 在 board.c 里用 #ifndef CONFIG_SPL_BUILD 注册 proper-only 的 sysreset 驱动
   （SPL 零变化）
C. 保持现状：只影响 U-Boot 命令行 `reset`，本项目无串口，实际不可达
```

当前影响面：Linux `reboot` 不经 U-Boot，本来就不受影响。

## 未做

```text
未刷写 eMMC / 未写 U 盘
未改 GPT、p1、p2
未改 idbloader / BL31 / DDR blob
未改 BOOT_TARGETS / BOOTCOMMAND / preboot
未改 DTS
```
