# Z96A Phase 2 RAM test (no eMMC write)

Purpose: validate the phase-2 U-Boot proper (USB-only scan -> z96a_logo ->
eMMC) entirely in RAM.  eMMC is not written.  If anything hangs, power-cycle
the board and the P1 eMMC U-Boot boots as before.

## Files

```text
u-boot-fit-dtb-phase2.bin   U-Boot proper + control DTB (RAM image)
z96a_chainload.bin          12-byte position-independent jump stub
boot.scr                    U-Boot script that loads the image and jumps
boot.scr.txt                source text for boot.scr
SHA256SUMS
```

The RAM image is built from the Armbian phase-2 test build
(`CONFIG_Z96A_EDP_LOGO=y`, phase-2 `CONFIG_BOOTCOMMAND`) with gcc 11.4,
the same compiler as the eMMC P1 build.

## Procedure (USB stick preparation)

Copy the four files to the FAT boot partition of the test USB stick
(the partition Linux mounts as `/boot`):

```text
u-boot-fit-dtb-phase2.bin
z96a_chainload.bin
boot.scr
```

Temporarily move the main USB boot configuration out of the way so the
current P1 U-Boot runs `boot.scr` instead:

```bash
sudo mv /boot/extlinux /boot/extlinux.z96a-bak
```

Then reboot the board.

## What happens

```text
P1 eMMC U-Boot (unchanged)
  -> bootflow scan finds USB boot.scr
  -> load phase-2 U-Boot to 0x00800000
  -> load chainload stub to 0x10000000
  -> fatrm boot.scr / extlinux.conf
  -> go 0x10000000

Phase-2 U-Boot (RAM only)
  -> usb start
  -> boot_targets=usb; bootflow scan -lb   (no USB bootflow left)
  -> z96a_logo                            (VOP2 + eDP + panel + backlight)
  -> boot_targets=mmc1 mmc0; bootflow scan -lb
```

Expected visual result: the internal panel lights up and shows the
U-Boot logo (CONFIG_VIDEO_LOGO is enabled).  Then the eMMC bootflow starts.

## Recovery

If the new U-Boot hangs or the panel stays dark:

1. Power-cycle the board.
2. The P1 eMMC U-Boot boots unchanged.
3. Restore the USB boot configuration:

```bash
sudo mv /boot/extlinux.z96a-bak /boot/extlinux
```

If the board boots eMMC Linux, mount the USB FAT partition and restore it there.
If eMMC Linux does not boot, remove the USB stick and restore `/boot/extlinux`
from another machine.

`boot.scr` is removed by the script itself before `go`; if it is still present,
delete it so the P1 U-Boot does not re-run the RAM test on the next boot.

## What this validates

```text
- the phase-2 U-Boot proper boots and relocates from RAM
- USB-only boot_targets scan returns when no USB bootflow exists
- z96a_logo binds/probes VOP2 and eDP
- eDP PHY / AUX / link training / panel / backlight
- eMMC fallback still boots
```

It does not validate the SPL/idbloader or the eMMC `u-boot.itb` flash path.
Those remain unchanged/P1 until a later, separately authorised flash.
