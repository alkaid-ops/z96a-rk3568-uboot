#!/usr/bin/env python3
"""Offline verification of the Z96A vendor V1 artifacts (no device access)."""

import hashlib
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path("/home/hsy/project/rea-z97a")
SRC = ROOT / "artifacts/vendor-uboot"
OUT = ROOT / "build/vendor-v1"

VA_BASE = 0xA00000
ENV_OFF = 0xC0ED0
ENV_LEN = 0xEBC
CHARGE_STR_OFF = 0xF6019
CHARGE_STR_SPAN = 23
NOP_SITES = [0x3F58, 0x3F5C]
FIT_A_OFF = 0x800000
FIT_B_OFF = 0xA00000
P2_START = 0x800000
FIT_HASH_FIT_OFF = 0x18C
FIT_PAYLOAD_POS = 0x1000

ok = True


def check(name, cond, detail=""):
    global ok
    print(("PASS  " if cond else "FAIL  ") + name + (f"  [{detail}]" if detail else ""))
    if not cond:
        ok = False


def sha(b):
    return hashlib.sha256(b).hexdigest()


def env_entries(blob):
    out = []
    i = 0
    while i < len(blob):
        j = blob.find(b"\x00", i)
        if j < 0:
            raise ValueError("unterminated env")
        seg = blob[i:j]
        if seg == b"":
            break
        n, v = seg.split(b"=", 1)
        out.append((n.decode(), v.decode()))
        i = j + 1
    return out, i


def main():
    payload_in = (SRC / "uboot-2017.bin").read_bytes()
    factory = (SRC / "factory-first16m.bin").read_bytes()
    patched = (OUT / "u-boot-patched.bin").read_bytes()
    idb = (OUT / "vendor-idbloader.bin").read_bytes()
    p2 = (OUT / "vendor-p2-uboot.bin").read_bytes()

    check("payload size unchanged", len(patched) == len(payload_in), f"{len(payload_in)}")
    check("input payload sha256", sha(payload_in) ==
          "7b1438d3b4253604cb36e04a54bbd5a53024ecb3358f07a0d03224ca9491cfb7")
    new_hash = sha(patched)
    print(f"      patched payload sha256 = {new_hash}")

    # 1. environment
    orig_entries, orig_end = env_entries(payload_in[ENV_OFF:ENV_OFF + ENV_LEN])
    new_entries, new_end = env_entries(patched[ENV_OFF:ENV_OFF + ENV_LEN])
    check("env terminator present (double NUL)", new_end < ENV_LEN,
          f"end={new_end:#x}")
    check("env fits in 0xebc", new_end <= ENV_LEN, f"{new_end}/{ENV_LEN}")
    check("env re-parses to expected entry count", len(new_entries) == len(orig_entries) - 1,
          f"{len(orig_entries)} -> {len(new_entries)}")
    d_new = dict(new_entries)
    d_old = dict(orig_entries)
    check("bootcmd replaced", d_new["bootcmd"] != d_old["bootcmd"])
    check("partitions dropped", "partitions" not in d_new)
    check("stdio pinned to serial",
          (d_new["stdin"], d_new["stdout"], d_new["stderr"]) == ("serial", "serial", "serial"))
    check("preboot empty", d_new["preboot"] == "")
    check("bootdelay 0", d_new["bootdelay"] == "0")
    check("eMMC fallback var present", "bootcmd_mmc0" in d_new)
    check("usb_boot var present", "usb_boot" in d_new)
    for k in ("boot_targets", "scriptaddr", "kernel_addr_r", "fdt_addr_r", "ramdisk_addr_r"):
        check(f"kept {k}", d_new.get(k) == d_old.get(k), str(d_new.get(k)))

    # 2. charge preboot string
    old_str = payload_in[CHARGE_STR_OFF:CHARGE_STR_OFF + CHARGE_STR_SPAN]
    new_str = patched[CHARGE_STR_OFF:CHARGE_STR_OFF + CHARGE_STR_SPAN]
    check("charge string was original",
          old_str == b"setenv preboot; charge\x00", repr(old_str))
    check("charge string patched", new_str == b"setenv preboot;\x00" + b"\x00" * 7, repr(new_str))

    # 3. NOP call sites
    for off in NOP_SITES:
        insn = struct.unpack("<I", patched[off:off + 4])[0]
        check(f"NOP at {VA_BASE + off:#x}", insn == 0xD503201F, f"{insn:#010x}")
        orig = struct.unpack("<I", payload_in[off:off + 4])[0]
        check(f"was BL at {VA_BASE + off:#x}", (orig & 0xFC000000) == 0x94000000, f"{orig:#010x}")

    # 4. nothing else changed in the payload
    diffs = [i for i in range(len(patched)) if patched[i] != payload_in[i]]
    ranges = []
    for i in diffs:
        if ranges and i == ranges[-1][1] + 1:
            ranges[-1][1] = i
        else:
            ranges.append([i, i])
    allowed = [(ENV_OFF, ENV_OFF + ENV_LEN - 1),
               (CHARGE_STR_OFF, CHARGE_STR_OFF + CHARGE_STR_SPAN - 1)] + \
              [(o, o + 3) for o in NOP_SITES]
    inside = all(any(a <= i <= b for a, b in allowed) for i in diffs)
    check("payload diff confined to env / charge string / NOPs", inside,
          f"{len(ranges)} ranges, {len(diffs)} bytes")
    for r in ranges:
        print(f"      diff {VA_BASE + r[0]:#x}..{VA_BASE + r[1]:#x} ({r[1] - r[0] + 1} bytes)")

    # 5. idbloader blob
    check("idbloader blob == factory bytes", idb == factory[0x8000:0xD2000],
          f"{len(idb)} bytes")
    check("idbloader blob starts with RKNS", idb[:4] == b"RKNS")
    check("idbloader second copy present", idb[0x80000:0x80004] == b"RKNS")

    # 6. p2 region: only FIT headers + payload data changed
    orig_p2 = factory[P2_START:P2_START + len(p2)]
    diffs2 = [i for i in range(len(p2)) if p2[i] != orig_p2[i]]
    ranges2 = []
    for i in diffs2:
        if ranges2 and i == ranges2[-1][1] + 1:
            ranges2[-1][1] = i
        else:
            ranges2.append([i, i])
    payload_in_allowed = [(FIT_A_OFF - P2_START + FIT_PAYLOAD_POS,
                           FIT_A_OFF - P2_START + FIT_PAYLOAD_POS + len(patched) - 1),
                          (FIT_B_OFF - P2_START + FIT_PAYLOAD_POS,
                           FIT_B_OFF - P2_START + FIT_PAYLOAD_POS + len(patched) - 1),
                          (FIT_A_OFF - P2_START + FIT_HASH_FIT_OFF,
                           FIT_A_OFF - P2_START + FIT_HASH_FIT_OFF + 31),
                          (FIT_B_OFF - P2_START + FIT_HASH_FIT_OFF,
                           FIT_B_OFF - P2_START + FIT_HASH_FIT_OFF + 31)]
    inside2 = all(any(a <= i <= b for a, b in payload_in_allowed) for i in diffs2)
    check("p2 diff confined to FIT hash + payload slots", inside2,
          f"{len(ranges2)} ranges, {len(diffs2)} bytes")

    for fit_off in (FIT_A_OFF - P2_START, FIT_B_OFF - P2_START):
        hoff = fit_off + FIT_HASH_FIT_OFF
        check(f"FIT hash updated @{P2_START + hoff:#x}",
              p2[hoff:hoff + 32] == bytes.fromhex(new_hash))
        poff = fit_off + FIT_PAYLOAD_POS
        check(f"FIT payload == patched @{P2_START + poff:#x}",
              p2[poff:poff + len(patched)] == patched)
        # other images untouched inside this FIT payload tail
        check(f"FIT data after payload unchanged @{P2_START + poff:#x}",
              p2[poff + len(patched):fit_off + 0x200000] ==
              orig_p2[poff + len(patched):fit_off + 0x200000])

    # 7. FIT A and FIT B header equality (both were identical in the factory image)
    check("FIT A/B headers identical after patch",
          p2[FIT_A_OFF - P2_START:FIT_A_OFF - P2_START + 0x1000] ==
          p2[FIT_B_OFF - P2_START:FIT_B_OFF - P2_START + 0x1000])

    # 8. external tool checks
    for name, off in (("A", FIT_A_OFF - P2_START), ("B", FIT_B_OFF - P2_START)):
        fit = p2[off:off + 0x200000]
        tmp = Path(f"/tmp/opencode/verify-fit-{name}.bin")
        tmp.write_bytes(fit)
        r = subprocess.run(["dumpimage", "-l", str(tmp)], capture_output=True, text=True)
        check(f"dumpimage parses FIT {name}", r.returncode == 0 and "FIT Image" in r.stdout)
        check(f"dumpimage reports new uboot hash in FIT {name}", new_hash in r.stdout)
        check(f"dumpimage reports unchanged atf-1 hash in FIT {name}",
              "b5946ac63df8fb3569407a75cf2f6aa396551ba011e1a84bec654dc9982ba063" in r.stdout)

    print()
    print("RESULT:", "ALL CHECKS PASSED" if ok else "FAILURES PRESENT")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
