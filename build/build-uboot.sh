#!/usr/bin/env bash
# Build a flashable / RAM-testable U-Boot FIT for the Z96A without rkbin.
#
# The deployed 2026.01 FIT ships the TF-A blobs (atf-1..atf-6) extracted
# from the current image.  We only rebuild U-Boot proper + its control
# DTB and re-pack them with those unchanged TF-A blobs, so no BL31/TBL
# download is needed and the result keeps the exact same FIT layout.
#
# Outputs (in $OUT/$TAG):
#   u-boot-nodtb.bin        raw U-Boot proper
#   u-boot.dtb              U-Boot control DTB
#   u-boot-fit-dtb.bin      nodtb + dtb, for RAM chain-load via `go`
#   u-boot.bin              full FIT for eMMC `uboot` partition (8 MiB)
#   z96a_chainload.bin      RAM chain-load stub
#   SHA256SUMS
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
SRC=${SRC:-$ROOT/u-boot-mainline-src}
OUT=${OUT:-$ROOT/build/out}
ATF_DIR=${ATF_DIR:-$ROOT/build/atf}
TAG=${TAG:-$(date +%Y%m%d-%H%M%S)}
JOBS=${JOBS:-$(nproc)}
CROSS_COMPILE=${CROSS_COMPILE:-aarch64-linux-gnu-}
export CROSS_COMPILE

TOOLS=${TOOLS:-$OUT/host-tools}
MKIMAGE=${MKIMAGE:-$TOOLS/tools/mkimage}
DUMPIMAGE=${DUMPIMAGE:-$TOOLS/tools/dumpimage}
BUILD=$OUT/$TAG
ITS=$BUILD/z96a-uboot.its

echo "== source: $SRC"
echo "== build : $BUILD"

# 1. host tools (mkimage/dumpimage)
if [ ! -x "$MKIMAGE" ]; then
	mkdir -p "$TOOLS"
	make -C "$SRC" O="$TOOLS" tools-only_defconfig >/dev/null
	make -C "$SRC" O="$TOOLS" tools-only -j"$JOBS" >/dev/null
fi

mkdir -p "$BUILD"

# 2. configure and compile U-Boot proper
make -C "$SRC" O="$BUILD" z96a-rk3568-laptop-v2_defconfig >/dev/null
make -C "$SRC" O="$BUILD" -j"$JOBS" u-boot-nodtb.bin u-boot.dtb

# 3. combined image used for RAM chain-load (DTB must directly follow
#    U-Boot at _end, which is what the Makefile's u-boot-fit-dtb.bin does)
cat "$BUILD/u-boot-nodtb.bin" "$BUILD/u-boot.dtb" > "$BUILD/u-boot-fit-dtb.bin"

# 4. generate the FIT description (same layout as deployed FIT)
cat > "$ITS" <<EOF
/dts-v1/;

/ {
	description = "FIT image for U-Boot with bl31 (TF-A)";
	#address-cells = <1>;

	images {
		u-boot {
			description = "U-Boot";
			data = /incbin/("$BUILD/u-boot-nodtb.bin");
			type = "standalone";
			arch = "arm64";
			compression = "none";
			load = <0x00800000>;
			entry = <0x00800000>;
			hash-1 {
				algo = "sha256";
			};
		};

		atf-1 {
			description = "ARM Trusted Firmware";
			data = /incbin/("$ATF_DIR/atf-1.bin");
			type = "firmware";
			arch = "arm64";
			os = "arm-trusted-firmware";
			compression = "none";
			load = <0x00040000>;
			hash-1 {
				algo = "sha256";
			};
		};

		atf-2 {
			description = "ARM Trusted Firmware";
			data = /incbin/("$ATF_DIR/atf-2.bin");
			type = "firmware";
			arch = "arm64";
			os = "arm-trusted-firmware";
			compression = "none";
			load = <0x0005a000>;
			hash-1 {
				algo = "sha256";
			};
		};

		atf-3 {
			description = "ARM Trusted Firmware";
			data = /incbin/("$ATF_DIR/atf-3.bin");
			type = "firmware";
			arch = "arm64";
			os = "arm-trusted-firmware";
			compression = "none";
			load = <0x0005c000>;
			hash-1 {
				algo = "sha256";
			};
		};

		atf-4 {
			description = "ARM Trusted Firmware";
			data = /incbin/("$ATF_DIR/atf-4.bin");
			type = "firmware";
			arch = "arm64";
			os = "arm-trusted-firmware";
			compression = "none";
			load = <0xfdcc1000>;
			hash-1 {
				algo = "sha256";
			};
		};

		atf-5 {
			description = "ARM Trusted Firmware";
			data = /incbin/("$ATF_DIR/atf-5.bin");
			type = "firmware";
			arch = "arm64";
			os = "arm-trusted-firmware";
			compression = "none";
			load = <0xfdcce000>;
			hash-1 {
				algo = "sha256";
			};
		};

		atf-6 {
			description = "ARM Trusted Firmware";
			data = /incbin/("$ATF_DIR/atf-6.bin");
			type = "firmware";
			arch = "arm64";
			os = "arm-trusted-firmware";
			compression = "none";
			load = <0xfdcd0000>;
			hash-1 {
				algo = "sha256";
			};
		};

		fdt-1 {
			description = "fdt-rockchip/rk3568-z96a-laptop-v2";
			data = /incbin/("$BUILD/u-boot.dtb");
			type = "flat_dt";
			compression = "none";
			hash-1 {
				algo = "sha256";
			};
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

# 5. pack FIT
"$MKIMAGE" -f "$ITS" "$BUILD/u-boot.bin"

# 6. RAM chain-load stub
aarch64-linux-gnu-gcc -c -nostdlib -o "$BUILD/z96a_chainload.o" \
	"$ROOT/build/chainload/z96a_chainload.S"
aarch64-linux-gnu-objcopy -O binary "$BUILD/z96a_chainload.o" \
	"$BUILD/z96a_chainload.bin"

# 7. verify and record
"$DUMPIMAGE" -l "$BUILD/u-boot.bin" > "$BUILD/fit-info.txt"
(
	cd "$BUILD"
	sha256sum u-boot.bin u-boot-nodtb.bin u-boot.dtb u-boot-fit-dtb.bin \
		z96a_chainload.bin > SHA256SUMS
)
echo "== FIT images:"
grep -E "^ Image|Load Address|Data Size" "$BUILD/fit-info.txt" | head -20
echo "== hashes:"
cat "$BUILD/SHA256SUMS"
echo
echo "RAM test:"
echo "  cp u-boot-fit-dtb.bin z96a_chainload.bin to the USB stick"
echo "  U-Boot> load usb 0:1 0x00800000 u-boot-fit-dtb.bin"
echo "  U-Boot> load usb 0:1 0x10000000 z96a_chainload.bin"
echo "  U-Boot> go 0x10000000"
echo "eMMC write (only after RAM test passes and write is authorised):"
echo "  dd if=u-boot.bin of=/dev/mmcblk0 seek=16384 conv=fsync"
