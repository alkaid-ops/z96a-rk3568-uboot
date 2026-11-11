#!/usr/bin/env python3
"""Replay the Z96A factory U-Boot SC8886/GPIO pre-probe state.

This is a reversible diagnostic helper. It does not reset the charger or
rebind tcpm. It writes the same SC8886 registers observed from factory
U-Boot's bq257xx path immediately before Linux modules are loaded.
"""
import argparse
import ctypes
from pathlib import Path
import fcntl
import os
import shutil
import subprocess
import struct
import sys
import time
import traceback

I2C_RDWR = 0x0707
I2C_M_RD = 0x0001
I2C_ADDR = 0x6B
I2C_BUS = "/dev/i2c-0"
GPIO_LINE = "21"  # GPIO0_C5, HUSB311 INT/power_usb_hub in factory U-Boot

# i2c_msg is 16 bytes on 64-bit Linux: addr, flags, len, padding, buf pointer.
class I2CMsg(ctypes.Structure):
    _fields_ = [
        ("addr", ctypes.c_uint16),
        ("flags", ctypes.c_uint16),
        ("len", ctypes.c_uint16),
        ("_pad", ctypes.c_uint16),
        ("buf", ctypes.c_void_p),
    ]

class I2CRdwrData(ctypes.Structure):
    _fields_ = [
        ("msgs", ctypes.POINTER(I2CMsg)),
        ("nmsgs", ctypes.c_uint32),
        ("_pad", ctypes.c_uint32),
    ]

def i2c_transfer_read16(fd, reg):
    tx = (ctypes.c_ubyte * 1)(reg)
    rx = (ctypes.c_ubyte * 2)()
    msgs = (I2CMsg * 2)(
        I2CMsg(I2C_ADDR, 0, 1, 0, ctypes.cast(tx, ctypes.c_void_p)),
        I2CMsg(I2C_ADDR, I2C_M_RD, 2, 0, ctypes.cast(rx, ctypes.c_void_p)),
    )
    data = I2CRdwrData(ctypes.cast(msgs, ctypes.POINTER(I2CMsg)), 2, 0)
    fcntl.ioctl(fd, I2C_RDWR, data)
    return rx[0] | (rx[1] << 8)

def i2c_transfer_write16(fd, reg, value):
    tx = (ctypes.c_ubyte * 3)(reg, value & 0xFF, (value >> 8) & 0xFF)
    msg = I2CMsg(I2C_ADDR, 0, 3, 0, ctypes.cast(tx, ctypes.c_void_p))
    data = I2CRdwrData(ctypes.pointer(msg), 1, 0)
    fcntl.ioctl(fd, I2C_RDWR, data)

def read_regs(fd, regs):
    for reg in regs:
        print(f"SC8886 0x{reg:02x}=0x{i2c_transfer_read16(fd, reg):04x}")

def pulse_gpio(line, hold_ms):
    """Drive GPIO0_C5 high briefly through libgpiod, then release it."""
    gpioset = shutil.which("gpioset")
    if gpioset:
        proc = subprocess.Popen(
            [gpioset, "-c", "gpiochip0", "-C", "z96a-boot-state-replay",
             f"{line}=1"],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        time.sleep(hold_ms / 1000.0)
        proc.terminate()
        try:
            out, err = proc.communicate(timeout=1)
        except subprocess.TimeoutExpired:
            proc.kill()
            out, err = proc.communicate()
        if proc.returncode not in (0, -15):
            raise RuntimeError(f"gpioset failed rc={proc.returncode}: {err.strip()}")
        return

    # Fallback for older images without libgpiod. The sysfs global number is
    # dynamic (base + offset), so use gpiochip0/base instead of offset alone.
    root = "/sys/class/gpio"
    chip = next((p for p in Path(root).glob("gpiochip*") if
                 (Path(root) / p.name / "label").read_text().strip() == "fdd60000.gpio"),
                Path(root) / "gpiochip512")
    base = int((chip / "base").read_text().strip())
    global_line = base + int(line)
    path = Path(root) / f"gpio{global_line}"
    if not path.exists():
        (Path(root) / "export").write_text(str(global_line))
        time.sleep(0.05)
    try:
        (path / "direction").write_text("out")
        (path / "value").write_text("1")
        time.sleep(hold_ms / 1000.0)
    finally:
        try:
            (path / "direction").write_text("in")
        except OSError:
            pass
        try:
            if path.exists():
                (Path(root) / "unexport").write_text(str(global_line))
        except OSError:
            pass

def replay(fd, gpio, hold_ms, dry_run):
    if dry_run:
        read_regs(fd, (0x00, 0x02, 0x0C, 0x0E, 0x14, 0x20, 0x32, 0x35, 0x36, 0x37))
        return

    option0 = i2c_transfer_read16(fd, 0x00)
    status20 = i2c_transfer_read16(fd, 0x20)
    ac_present = bool(status20 & 0x8000)

    # Exact factory-U-Boot bq257xx initialization order.
    i2c_transfer_write16(fd, 0x00, option0 & 0x1FFF)  # clear watchdog field
    i2c_transfer_write16(fd, 0x0C, 0x1200)             # min system voltage
    i2c_transfer_write16(fd, 0x0E, 0x1E00 if ac_present else 0x0A00)
    if ac_present:
        i2c_transfer_write16(fd, 0x02, 0x0400)

    # Factory U-Boot drives GPIO0_C5 high before handoff. This is the
    # HUSB311 interrupt line on the Linux DTS, not a reset or enable GPIO.
    pulse_gpio(gpio, hold_ms)

    print(f"replay done: AC={int(ac_present)} GPIO={gpio} hold={hold_ms}ms")
    read_regs(fd, (0x00, 0x02, 0x0C, 0x0E, 0x14, 0x20, 0x32, 0x35, 0x36, 0x37))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--gpio", default=GPIO_LINE)
    ap.add_argument("--hold-ms", type=int, default=20)
    args = ap.parse_args()

    fd = os.open(I2C_BUS, os.O_RDWR)
    try:
        replay(fd, args.gpio, args.hold_ms, args.dry_run)
    finally:
        os.close(fd)

if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"z96a-boot-state-replay: {exc}", file=sys.stderr)
        traceback.print_exc()
        sys.exit(1)
