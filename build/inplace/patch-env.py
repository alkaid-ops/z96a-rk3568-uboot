#!/usr/bin/env python3
"""Data-only patch for the deployed Z96A U-Boot proper image.

This changes NO executable code and NO device tree / TF-A bytes.

It rewrites the built-in `bootcmd` string in place (same total length,
same four USB rail GPIO commands, same `usb start` and
`bootflow scan -lb`), adding one `gpio s A27` before `usb start` so the
white power LED lights as soon as autoboot starts.  It also changes
`bootdelay=3` to `bootdelay=1`.

Facts used:
  * U-Boot cmd_gpio only looks at the first character of the subcommand,
    so `gpio s A27` == `gpio set A27`.
  * Rockchip GPIO bank names are 'A'+bank: GPIO0 -> "A", so A27 is
    GPIO0 offset 27 = GPIO0_D3, the white front LED (active high).
  * The four original GPIO commands enable the USB 5V rails and the
    USB keyboard rail; they are kept verbatim.
  * If `gpio s A27` fails for any reason, cmd_gpio prints an error and
    the `;`-separated bootcmd continues with `usb start` as before.

Run:
  patch-env.py <u-boot-image0.bin> <u-boot-image0-led.bin>
"""
import sys
import hashlib

# Verified against the live eMMC image (hash 28bafb63... image0,
# efd764f4... FIT).
BOOTCMD_OFF = 717620       # 'bootcmd=' first byte
BOOTCMD_LEN = 91           # 8 + 82 value chars + NUL
BOOTCMD_OLD = (b"bootcmd=gpio set A6; gpio set E11; gpio set E9; "
               b"gpio set C31; usb start; bootflow scan -lb\x00")
BOOTCMD_NEW = (b"bootcmd=gpio s A6;gpio s E11;gpio s E9;gpio s C31;"
               b"gpio s A27;usb start;bootflow scan -lb  \x00")

BOOTDELAY_OFF = 717721     # the '3' in 'bootdelay=3'
BOOTDELAY_OLD = b"3"
BOOTDELAY_NEW = b"1"

assert len(BOOTCMD_OLD) == BOOTCMD_LEN
assert len(BOOTCMD_NEW) == BOOTCMD_LEN, len(BOOTCMD_NEW)


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    src, dst = sys.argv[1], sys.argv[2]
    src_data = open(src, "rb").read()
    data = bytearray(src_data)

    if bytes(data[BOOTCMD_OFF:BOOTCMD_OFF + BOOTCMD_LEN]) != BOOTCMD_OLD:
        print("ERROR: bootcmd range does not match expected deployed image")
        print(repr(bytes(data[BOOTCMD_OFF:BOOTCMD_OFF + BOOTCMD_LEN])))
        return 1
    if bytes(data[BOOTDELAY_OFF:BOOTDELAY_OFF + 1]) != BOOTDELAY_OLD:
        print("ERROR: bootdelay byte does not match")
        return 1

    data[BOOTCMD_OFF:BOOTCMD_OFF + BOOTCMD_LEN] = BOOTCMD_NEW
    data[BOOTDELAY_OFF:BOOTDELAY_OFF + 1] = BOOTDELAY_NEW
    open(dst, "wb").write(data)

    diffs = [i for i, (a, b) in enumerate(zip(src_data, data)) if a != b]
    allowed = set(range(BOOTCMD_OFF, BOOTCMD_OFF + BOOTCMD_LEN)) | {BOOTDELAY_OFF}
    unexpected = [i for i in diffs if i not in allowed]
    if unexpected:
        print("ERROR: unexpected byte changes at", unexpected[:10])
        return 1

    print(f"input : {src}")
    print(f"output: {dst}")
    print(f"changed bytes: {len(diffs)} (bootcmd text + bootdelay only)")
    print("input  sha256:", hashlib.sha256(src_data).hexdigest())
    print("output sha256:", hashlib.sha256(bytes(data)).hexdigest())
    print("new bootcmd:", repr(BOOTCMD_NEW))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
