# First eMMC flash plan — data-only environment patch

## Why this is the safest possible first write

The deployed U-Boot proper image on eMMC is exactly
`artifacts/mainline-uboot/extracted/u-boot-image0.bin`
(sha256 `28bafb63...`), and the first 1,220,096 bytes of the live eMMC
region hash to `efd764f4...`, verified read-only on 2026-11-10.

`patch-env.py` changes **74 bytes**, all inside the built-in default
environment string:

* `bootcmd` is rewritten in place with the *same total length*:
  - the four USB 5V rail commands (`A6`, `E11`, `E9`, `C31`) are kept,
  - `gpio set` -> `gpio s` (cmd_gpio only matches the first character),
  - one `gpio s A27` is added before `usb start` (GPIO0_D3, white LED),
  - `usb start` and `bootflow scan -lb` are kept unchanged;
* `bootdelay` 3 -> 1 (one byte).

No executable instruction, no device tree, no TF-A blob changes.

The FIT is then repacked with `mkimage -E -B 0x200`, and is checked
against the deployed FIT:

* `dumpimage` structure identical;
* every image's `data-offset`, `data-size`, `load`, `entry` identical;
* all 8 declared SHA-256 hashes match the actual payload.

If `gpio s A27` fails for any reason, cmd_gpio prints an error and the
`;`-separated bootcmd continues with `usb start`; USB-first boot is
untouched.

## Residual risk (stated honestly)

* This is the only U-Boot slot; there is no SD card and no A/B slot.
* If the write itself is corrupted, SPL's FIT hash check fails and the
  board hangs; read-back verification below is therefore mandatory.
* If the machine does not boot after this write, recovery is not
  available without USB maskrom/physical access.  The user accepted
  this non-zero risk explicitly.

## Steps

1. Read-only backup of the live 4 MiB U-Boot region to the USB stick.
2. Copy the new FIT to `/tmp` on the device and verify its SHA-256.
3. Write with `dd` at 8 MiB, `conv=fsync`.
4. Read back exactly `filesize` bytes and compare SHA-256.
5. Reboot and verify USB boot (boot id, uptime, root device, dmesg).
6. User visually confirms the white LED lights early.

## Rollback file

`/boot/z96a-uboot/emmc-uboot-region-before-inplace.bin`
(4 MiB, sha256 `87e522e3...`).  If Linux boots after the flash, this can
be written back with the same `dd` procedure.
