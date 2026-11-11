# B1 flash result — 2026-11-10

## Write method (root cause of earlier failures)

The eMMC is unstable at HS400/200 MHz for writes: the kernel reports
`Data CRC Errors` and every write request returns I/O error. Reads work.

Runtime workaround, no reboot / no DTB change:

```sh
mount -t debugfs none /sys/kernel/debug
echo 50000000 > /sys/kernel/debug/mmc0/clock
```

At 50 MHz, a 1 MiB write/read/restore test on the free area
(`bs=4096 seek=3750`) passed with `oflag=direct conv=fsync`.
At 100 MHz and 200 MHz writes fail; 25 MHz is unreliable.

## Flash sequence

1. Set clock 50 MHz.
2. Write baseline `efd764...` at 8 MiB (`bs=512 seek=16384 count=2383`).
3. Read back 1,220,096 bytes: hash matched `efd764...`.
4. Reboot: USB system booted, `/dev/sda2`, new boot id.
5. Write B1 `b1d7ab...` the same way.
6. Read back: hash matched `b1d7ab...`.
7. Reboot: USB system booted, `/dev/sda2`, new boot id.
8. Live eMMC FIT hash re-read after boot: `b1d7ab...`.
9. User visually confirmed the white LED now lights early (problem solved).

## B1 content

Built with the original Armbian/binman pipeline (gcc 11.4.0, BL31 v1.46,
custom DDR blob). Only 105 bytes differ from the deployed image:

- 32 bytes: FIT image0 SHA-256 value
- 76 bytes: bootcmd string

No instruction, DTB, ATF or FIT-layout changes.
`idbloader.img` remains byte-identical to the deployed one.

bootcmd:

```text
gpio s A6;gpio s E11;gpio s E9;gpio s C31;gpio s A27;
usb start;bootflow scan -lb
```

`A27` = GPIO0_D3 white LED, set before `usb start`.
If it fails, cmd_gpio prints an error and boot continues.

## Backups

```text
/boot/z96a-uboot/emmc-uboot-region-before-inplace.bin
sha256 87e522e3b5838e7df24dd7d5622d4c5bd4d4c0f067d412043d3ca94ec824949c
```

Baseline FIT and B1 FIT are in `build/out/armbian-baseline/` and
`build/out/armbian-b1/`.

Rollback: mount debugfs, set eMMC clock to 50 MHz, then write the 4 MiB
backup back at block 16384 (8 MiB) with `bs=512 oflag=direct conv=fsync`,
and read back/verify.
