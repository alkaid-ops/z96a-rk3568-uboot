#!/usr/bin/env python3
"""
Z96A vendor U-Boot 2017.09 V1 builder (offline only).

Inputs (read-only):
  artifacts/vendor-uboot/uboot-2017.bin        vendor U-Boot proper payload, VA base 0xa00000
  artifacts/vendor-uboot/factory-first16m.bin  vendor 16MiB boot area (idbloader + p2 FIT A/B)

Outputs (build/vendor-v1/):
  u-boot-patched.bin        patched U-Boot proper payload
  vendor-idbloader.bin      bytes 0x8000..0xc8000 (both idblock copies), unchanged content
  vendor-p2-uboot.bin       bytes 0x800000..0xc00000 (FIT A + FIT B, patched)
  env-patched.txt           patched default environment, readable
  report.txt                verification report

No device access, no flashing.
"""

import hashlib
import struct
import sys
from pathlib import Path

ROOT = Path("/home/hsy/project/rea-z97a")
SRC = ROOT / "artifacts/vendor-uboot"
OUT = ROOT / "build/vendor-v1"

PAYLOAD = SRC / "uboot-2017.bin"
FACTORY = SRC / "factory-first16m.bin"

VA_BASE = 0xA00000
ENV_OFF = 0xC0ED0          # default env blob, VA 0xaf0ed0
ENV_LEN = 0xEBC            # 3772 bytes, fixed by FUN_00ab58c4 call
ENV_COPY_LEN = 0xEBC

CHARGE_STR_OFF = 0xF6019   # "setenv preboot; charge"
CHARGE_STR_LEN = 22
CHARGE_STR_NEW = "setenv preboot;"

CALL_SITES = [             # board_late_init 0xa03e14: video/logo calls -> NOP
    0x3F58,                # bl 0xa7c5d4  charge animation / video
    0x3F5C,                # bl 0xa52710  logo / DRM / eDP
]
NOP = b"\x1f\x20\x03\xd5"  # d503201f nop

FIT_A_OFF = 0x800000
FIT_B_OFF = 0xA00000
P2_START = 0x800000
P2_END = 0xC00000
FIT_HASH_FIT_OFF = 0x18C   # uboot hash value inside FIT dtb
FIT_PAYLOAD_POS = 0x1000

IDB_START = 0x8000
IDB_END = 0xD2000          # covers both idblock copies (first ends 0x51a4f, second 0xd1a4f)

BOOTCMD = (
    "setenv devnum 0; "
    "usb start; "
    "if usb dev 0; then setenv devtype usb; run scan_dev_for_boot_part; fi; "
    "run bootcmd_mmc0; "
    "sysboot mmc 0:1 any 0x00c00000 /extlinux/extlinux.conf"
)

ENV_OVERRIDES = {
    "bootcmd": BOOTCMD,
    "stdin": "serial",
    "stdout": "serial",
    "stderr": "serial",
}
ENV_DROP = {"partitions"}   # freed to make room; only used by gpt/part writers


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def parse_env(blob: bytes):
    entries = []
    for raw in blob.split(b"\x00"):
        if not raw:
            continue
        if b"=" not in raw:
            sys.exit(f"unexpected env entry without '=': {raw!r}")
        name, value = raw.split(b"=", 1)
        entries.append((name.decode(), value.decode()))
    return entries


def build_env(payload_source: bytes):
    blob = payload_source[ENV_OFF:ENV_OFF + ENV_LEN]
    entries = parse_env(blob)

    out = []
    seen = set()
    for name, value in entries:
        if name in ENV_DROP:
            continue
        if name in ENV_OVERRIDES:
            value = ENV_OVERRIDES[name]
        out.append((name, value))
        seen.add(name)
    for name, value in ENV_OVERRIDES.items():
        if name not in seen:
            out.append((name, value))

    body = "\x00".join(f"{n}={v}" for n, v in out) + "\x00\x00\x00"
    raw = body.encode()
    if len(raw) > ENV_LEN:
        sys.exit(f"env too large: {len(raw)} > {ENV_LEN} (need {len(raw) - ENV_LEN} more bytes)")
    padded = raw + b"\x00" * (ENV_LEN - len(raw))
    return out, padded, entries


def patch_payload(payload: bytes, env_padded: bytes):
    buf = bytearray(payload)

    # 1. default environment
    buf[ENV_OFF:ENV_OFF + ENV_LEN] = env_padded

    # 2. cold-plug preboot: "setenv preboot; charge" -> "setenv preboot;"
    span = CHARGE_STR_LEN + 1
    old = bytes(buf[CHARGE_STR_OFF:CHARGE_STR_OFF + span])
    expect = b"setenv preboot; charge\x00"
    if old != expect:
        sys.exit(f"charge string mismatch at {CHARGE_STR_OFF:#x}: {old!r}")
    buf[CHARGE_STR_OFF:CHARGE_STR_OFF + span] = (CHARGE_STR_NEW + "\x00").encode().ljust(span, b"\x00")

    # 3. disable display init in board_late_init
    for off in CALL_SITES:
        insn = struct.unpack("<I", bytes(buf[off:off + 4]))[0]
        if (insn & 0xFC000000) != 0x94000000:
            sys.exit(f"expected BL at {off:#x}, got {insn:#010x}")
        target = (off + 0x4 * (((insn & 0x03FFFFFF) ^ 0x02000000) - 0x02000000)) & 0xFFFFFFFF
        buf[off:off + 4] = NOP
        print(f"NOP {VA_BASE + off:#x}: bl {target:#x}")

    return bytes(buf)


def main():
    payload = PAYLOAD.read_bytes()
    factory = FACTORY.read_bytes()

    if sha256(payload) != "7b1438d3b4253604cb36e04a54bbd5a53024ecb3358f07a0d03224ca9491cfb7":
        sys.exit("unexpected input payload sha256")
    if sha256(factory) != "f761cac65d410a629dc3b6610e4791c6b2a3aa86a6ce3d7fdae6c3f8a76c87e1":
        sys.exit("unexpected input factory image sha256")

    entries, env_padded, original_entries = build_env(payload)
    print(f"env entries: {len(original_entries)} -> {len(entries)}, "
          f"blob {ENV_LEN} bytes, used {len(env_padded.rstrip(b'\\x00'))} bytes")

    patched = patch_payload(payload, env_padded)
    if len(patched) != len(payload):
        sys.exit("payload size changed")

    new_hash = sha256(patched)
    if new_hash == sha256(payload):
        sys.exit("patched payload hash unchanged")

    # FIT A / FIT B: replace uboot hash value and payload bytes
    fit_region = bytearray(factory[P2_START:P2_END])
    old_hash = bytes.fromhex(sha256(payload))
    for fit_off in (FIT_A_OFF - P2_START, FIT_B_OFF - P2_START):
        hpos = fit_off + FIT_HASH_FIT_OFF
        if bytes(fit_region[hpos:hpos + 32]) != old_hash:
            sys.exit(f"hash not found at {hpos:#x}")
        fit_region[hpos:hpos + 32] = bytes.fromhex(new_hash)

        ppos = fit_off + FIT_PAYLOAD_POS
        if bytes(fit_region[ppos:ppos + len(payload)]) != payload:
            sys.exit(f"payload not found at {ppos:#x}")
        fit_region[ppos:ppos + len(patched)] = patched

    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "u-boot-patched.bin").write_bytes(patched)
    (OUT / "vendor-idbloader.bin").write_bytes(factory[IDB_START:IDB_END])
    (OUT / "vendor-p2-uboot.bin").write_bytes(fit_region)

    env_text = "\n".join(f"{n}={v}" for n, v in entries) + "\n"
    (OUT / "env-patched.txt").write_text(env_text)

    lines = []
    lines.append("Z96A vendor U-Boot V1 build report")
    lines.append("")
    lines.append(f"input payload        {sha256(payload)}  {len(payload)} bytes")
    lines.append(f"patched payload      {new_hash}  {len(patched)} bytes")
    lines.append(f"input factory 16MiB  {sha256(factory)}")
    lines.append(f"idbloader blob       {sha256(factory[IDB_START:IDB_END])}  "
                 f"{IDB_END - IDB_START} bytes (offsets {IDB_START:#x}..{IDB_END:#x})")
    lines.append(f"p2 uboot blob        {sha256(bytes(fit_region))}  {len(fit_region)} bytes "
                 f"(offsets {P2_START:#x}..{P2_END:#x})")
    lines.append("")
    lines.append("env entries after patch:")
    for n, v in entries:
        lines.append(f"  {n}={v}")
    (OUT / "report.txt").write_text("\n".join(lines) + "\n")
    print("\n".join(lines))


if __name__ == "__main__":
    main()
