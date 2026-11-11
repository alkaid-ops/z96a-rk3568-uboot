Z96A U-Boot RAM test package
===========================

Copy this whole directory to the FAT boot partition of the USB stick
(the partition Linux mounts as /boot).

At the U-Boot serial prompt (Ctrl+C to stop autoboot):

  usb start
  ls usb 0:1 /
  load usb 0:1 0x00800000 u-boot-fit-dtb-baseline.bin
  load usb 0:1 0x10000000 z96a_chainload.bin
  go 0x10000000

That should reboot into the *baseline* rebuilt U-Boot and then boot the
USB stick normally.  This proves the RAM chain-load path; eMMC is not
touched.

Then repeat with the candidate:

  load usb 0:1 0x00800000 u-boot-fit-dtb-ledcharge-v2.bin
  load usb 0:1 0x10000000 z96a_chainload.bin
  go 0x10000000

Candidate behaviour to check at the new U-Boot prompt:

  z96a_charge

  - power LED on immediately, red/green follow charger state
  - short power-key press prints "power key pressed, booting"
    and returns to the prompt
  - Ctrl+C also returns
  - `boot` continues normal USB-first boot

If anything hangs: power-cycle the board.  The eMMC U-Boot boots
unchanged.

Full details: ../RUNBOOK.md
