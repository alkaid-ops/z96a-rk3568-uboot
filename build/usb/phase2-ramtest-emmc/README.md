# Z96A Phase 2 RAM test (eMMC path, USB recovery preserved)

Purpose: validate the phase-2 **display path** and eMMC fallback entirely in
RAM.  eMMC is not written.  The USB extlinux configuration is restored by the
U-Boot script before jumping, so a power-cycle falls back to USB Linux if the
new U-Boot hangs.

This is the lowest-risk first display test:

```text
RAM U-Boot bootcmd:
  gpio pokes
  z96a_logo
  boot_targets=mmc1 mmc0; bootflow scan -lb
```

It does **not** scan USB in the new U-Boot, so the restored extlinux does not
cause a chainload loop.  The USB-only boot_targets branch is tested separately
after this display test passes.

## Files

```text
u-boot-fit-dtb-phase2-emmc.bin   phase-2 U-Boot + DTB (RAM image)
z96a_chainload.bin               12-byte jump stub
boot.scr                         loads image, restores extlinux, jumps
boot.scr.txt                     source text
SHA256SUMS
```

The image is built with gcc 11.4 in the Armbian environment.

## Procedure

Copy the three files to the FAT boot partition (mounted as `/boot`):

```text
u-boot-fit-dtb-phase2-emmc.bin
z96a_chainload.bin
boot.scr
```

Temporarily rename only the extlinux **file**, keeping the directory:

```bash
sudo mv /boot/extlinux/extlinux.conf \
        /boot/extlinux/extlinux.conf.z96a-bak
```

Then reboot.

## What happens

```text
P1 eMMC U-Boot
  -> USB scan no longer finds extlinux
  -> finds boot.scr
  -> load phase-2 U-Boot at 0x00800000
  -> load chainload stub at 0x10000000
  -> fatload extlinux.conf.z96a-bak, fatwrite extlinux.conf
  -> fatrm boot.scr
  -> go 0x10000000

Phase-2 U-Boot (RAM only)
  -> z96a_logo
  -> boot_targets=mmc1 mmc0; bootflow scan -lb
```

Expected: the internal panel fills white for 30 seconds, then the eMMC
bootflow starts.  The new U-Boot writes its diagnostic result to
`/boot/z96a-logo-status.txt` on the USB FAT partition:

```text
result=ok error=0 stage=4 top=20000000 bottom=1e000000 fb=1f800000
```

The image uses a fixed post-relocation framebuffer window at
0x1f800000..0x20000000 because VOP2 is bound after `video_reserve()` has
already run.  Without that window `video_post_bind()` returns `-ENOSPC`.

## Recovery

If the new U-Boot hangs or the panel stays dark:

1. Power-cycle the board.
2. P1 eMMC U-Boot boots.
3. The script already restored `/boot/extlinux/extlinux.conf`; the USB stick
   boots Linux again.

If the script could not restore extlinux (it aborts before `go`), restore
manually:

```bash
sudo mv /boot/extlinux/extlinux.conf.z96a-bak \
        /boot/extlinux/extlinux.conf
```

If `boot.scr` is still present, remove it.

## Risk boundary

```text
eMMC: untouched
idbloader: A 367d32ac
u-boot.itb: P1 3dd68203
USB extlinux: restored before jump; power-cycle recovery
```

If this RAM test passes, the next step is the USB-only branch RAM test, then
a separately authorised eMMC `u-boot.itb` flash.
