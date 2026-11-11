#!/usr/bin/env bash
# Build a "data-only" U-Boot FIT for the first eMMC flash.
#
# Base is the exact image0 currently on eMMC (hash efd764f4... FIT /
# 28bafb63... image0), with only 35 bytes of built-in environment
# changed: bootdelay 3->1 and a preboot that lights the white LED.
# No executable byte changes.
set -euo pipefail

# Deterministic FIT timestamps (same epoch as the deployed image)
export SOURCE_DATE_EPOCH=${SOURCE_DATE_EPOCH:-1793132605}

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
BASE_IMAGE=${BASE_IMAGE:-$ROOT/artifacts/mainline-uboot/extracted/u-boot-image0.bin}
BASE_DTB=${BASE_DTB:-$ROOT/artifacts/mainline-uboot/extracted/u-boot-fdt.dtb}
ATF_DIR=${ATF_DIR:-$ROOT/build/atf}
MKIMAGE=${MKIMAGE:-$ROOT/build/out/host-tools/tools/mkimage}
DUMPIMAGE=${DUMPIMAGE:-$ROOT/build/out/host-tools/tools/dumpimage}
OUT=${OUT:-$ROOT/build/out/inplace-led}

mkdir -p "$OUT"

python3 "$ROOT/build/inplace/patch-env.py" \
	"$BASE_IMAGE" "$OUT/u-boot-image0-led.bin" | tee "$OUT/patch.log"

ITS=$OUT/z96a-inplace.its
cat > "$ITS" <<EOF
/dts-v1/;

/ {
	description = "FIT image for U-Boot with bl31 (TF-A)";
	#address-cells = <1>;

	images {
		u-boot {
			description = "U-Boot";
			data = /incbin/("$OUT/u-boot-image0-led.bin");
			type = "standalone";
			arch = "arm64";
			os = "u-boot";
			compression = "none";
			load = <0x00800000>;
			entry = <0x00800000>;
			hash-1 { algo = "sha256"; };
		};
		atf-1 {
			description = "ARM Trusted Firmware";
			data = /incbin/("$ATF_DIR/atf-1.bin");
			type = "firmware";
			arch = "arm64";
			os = "arm-trusted-firmware";
			compression = "none";
			load = <0x00040000>;
			entry = <0x00040000>;
			hash-1 { algo = "sha256"; };
		};
		atf-2 {
			description = "ARM Trusted Firmware";
			data = /incbin/("$ATF_DIR/atf-2.bin");
			type = "firmware";
			arch = "arm64";
			os = "arm-trusted-firmware";
			compression = "none";
			load = <0x0005a000>;
			hash-1 { algo = "sha256"; };
		};
		atf-3 {
			description = "ARM Trusted Firmware";
			data = /incbin/("$ATF_DIR/atf-3.bin");
			type = "firmware";
			arch = "arm64";
			os = "arm-trusted-firmware";
			compression = "none";
			load = <0x0005c000>;
			hash-1 { algo = "sha256"; };
		};
		atf-4 {
			description = "ARM Trusted Firmware";
			data = /incbin/("$ATF_DIR/atf-4.bin");
			type = "firmware";
			arch = "arm64";
			os = "arm-trusted-firmware";
			compression = "none";
			load = <0xfdcc1000>;
			hash-1 { algo = "sha256"; };
		};
		atf-5 {
			description = "ARM Trusted Firmware";
			data = /incbin/("$ATF_DIR/atf-5.bin");
			type = "firmware";
			arch = "arm64";
			os = "arm-trusted-firmware";
			compression = "none";
			load = <0xfdcce000>;
			hash-1 { algo = "sha256"; };
		};
		atf-6 {
			description = "ARM Trusted Firmware";
			data = /incbin/("$ATF_DIR/atf-6.bin");
			type = "firmware";
			arch = "arm64";
			os = "arm-trusted-firmware";
			compression = "none";
			load = <0xfdcd0000>;
			hash-1 { algo = "sha256"; };
		};
		fdt-1 {
			description = "fdt-rockchip/rk3568-z96a-laptop-v2";
			data = /incbin/("$BASE_DTB");
			type = "flat_dt";
			compression = "none";
			hash-1 { algo = "sha256"; };
		};
	};

	configurations {
		default = "config-1";
		config-1 {
			description = "rockchip/rk3568-z96a-laptop-v2.dtb";
			firmware = "atf-1";
			loadables = "u-boot", "atf-2", "atf-3", "atf-4", "atf-5", "atf-6";
			fdt = "fdt-1";
		};
	};
};
EOF

# Match the deployed binman layout: external data aligned to 0x200.
"$MKIMAGE" -E -B 0x200 -f "$ITS" "$OUT/u-boot.bin"

"$DUMPIMAGE" -l "$OUT/u-boot.bin" > "$OUT/fit-info.txt"

# Structural comparison against the deployed FIT (ignore size/hash/timestamp)
norm() { grep -vE "Created:|Data Size:|Hash value:|description|Description" "$1"; }
norm "$ROOT/artifacts/mainline-uboot/extracted/fit-info.txt" > "$OUT/deployed.norm"
norm "$OUT/fit-info.txt" > "$OUT/new.norm"
if ! diff -u "$OUT/deployed.norm" "$OUT/new.norm" > "$OUT/structure.diff"; then
	echo "ERROR: FIT structure differs from deployed:" >&2
	cat "$OUT/structure.diff" >&2
	exit 1
fi
echo "FIT structure: identical to deployed"

# Compare the actual payload placement (external data offsets) with the
# deployed FIT.  This is what SPL reads; it must match exactly.
python3 - "$OUT/u-boot.bin" "$ROOT/artifacts/mainline-uboot/u-boot.bin" <<'PY'
import struct, subprocess, sys

def layout(path):
    d = open(path, 'rb').read()
    totalsize = struct.unpack_from('>I', d, 4)[0]
    hdr = '/tmp/fit-hdr-%d.dtb' % (hash(path) & 0xffff)
    open(hdr, 'wb').write(d[:totalsize])
    imgs = subprocess.check_output(['fdtget', '-l', hdr, '/images']).decode().split()
    out = []
    for img in imgs:
        vals = []
        for prop in ('data-offset', 'data-size', 'load', 'entry'):
            try:
                vals.append(subprocess.check_output(
                    ['fdtget', '-t', 'x', hdr, f'/images/{img}', prop],
                    stderr=subprocess.DEVNULL).decode().strip())
            except subprocess.CalledProcessError:
                vals.append('-')
        out.append((img, *vals))
    return out

new = layout(sys.argv[1])
dep = layout(sys.argv[2])
if new != dep:
    print('deployed:', dep)
    print('new     :', new)
    raise SystemExit('ERROR: FIT payload layout differs from deployed')
print('FIT payload layout: identical to deployed (offset/size/load/entry)')
PY

# Verify every declared hash against the actual payload
python3 - "$OUT/u-boot.bin" "$OUT/fit-info.txt" "$DUMPIMAGE" "$OUT" <<'PY'
import hashlib, os, re, subprocess, sys

fit, info, dumpimage, out = sys.argv[1:5]
declared = [m.group(1) for m in re.finditer(r"Hash value:\s+([0-9a-f]{64})", open(info).read())]
extracted = []
for idx in range(len(declared)):
    path = os.path.join(out, f"verify-{idx}.bin")
    subprocess.run([dumpimage, "-T", "flat_dt", "-p", str(idx), "-o", path, fit],
                   check=True, stdout=subprocess.DEVNULL)
    extracted.append(hashlib.sha256(open(path, "rb").read()).hexdigest())
    os.unlink(path)

if extracted != declared:
    print("declared:", declared)
    print("actual  :", extracted)
    raise SystemExit("ERROR: FIT hash verification failed")
print(f"FIT hashes: {len(declared)} images verified OK")
PY

(
	cd "$OUT"
	sha256sum u-boot.bin u-boot-image0-led.bin > SHA256SUMS
)
echo
cat "$OUT/SHA256SUMS"
echo
echo "Next: flash only after reading build/inplace/FLASH-PLAN.md"
