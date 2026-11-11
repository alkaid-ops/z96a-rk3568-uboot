#!/usr/bin/env bash
# Phase 1 read-only hardware semantics probe for the Z96A.
#
# Run as root on the running Z96A Linux (USB boot; eMMC untouched).
# Only reads registers/sysfs; the LED test briefly changes the LED
# brightness through the Linux LED class and restores it.
#
# Verified on hardware 2026-11-10 (z96a-devuan, kernel 7.2.12):
#   RK809 ON_SOURCE 0xf5:
#     0x40 cold plug-in (PLUG_IN, no PWRON)
#     0x80 power button (PWRON, no PLUG_IN)
#     0x02 warm Linux reboot (neither)
#   PMU_GRF OS registers are cleared by the warm reset (no marker).
set -u

I2CGET=$(command -v i2cget 2>/dev/null || true)
DEVMEM=$(command -v devmem 2>/dev/null || true)
[ -n "$I2CGET" ] || I2CGET="busybox i2cget"
[ -n "$DEVMEM" ] || DEVMEM="busybox devmem"

section() { printf '\n===== %s =====\n' "$*"; }
run() { printf '\n$ %s\n' "$*"; "$@" 2>&1 || echo "(failed: $?)"; }

MODEL=$(tr -d '\0' </proc/device-tree/model 2>/dev/null || echo unknown)
section "system"
echo "date: $(date -Is)"
echo "host: $(hostname)"
echo "model: $MODEL"
echo "kernel: $(uname -r)"
echo "boot id: $(cat /proc/sys/kernel/random/boot_id)"
echo "uptime: $(uptime -p)"
echo "cmdline: $(tr -d '\0' </proc/cmdline)"

section "i2c buses"
"$I2CGET" -l 2>/dev/null || i2cdetect -l 2>/dev/null || ls /dev/i2c-*

# Prefer the actual driver nodes over probing (kernel drivers own the
# addresses, so a plain i2cget needs -f to force access).
PMIC_BUS=""
for d in /sys/bus/i2c/devices/*-0020; do
	[ -e "$d/name" ] || continue
	PMIC_BUS=$(basename "$d" | cut -d- -f1)
done
echo "PMIC i2c bus: ${PMIC_BUS:-not-found}"

if [ -n "$PMIC_BUS" ]; then
	section "RK809 identity and power-on reason"
	for r in ed ee f5 f6 f8 f9 f4 ae af; do
		run $I2CGET -f -y "$PMIC_BUS" 0x20 0x$r
	done
	cat <<'EOF'

Decode 0xf5 ON_SOURCE (verified):
  0x40 = cold charger plug-in  -> charge mode
  0x80 = power button          -> normal boot
  0x02 = warm Linux reboot     -> normal boot
  0xf9 INT_MSK0 bit0/bit1 = 0 means PWRON fall/rise are unmasked.
EOF
fi

section "PMU_GRF capture registers"
for reg in fdc20200 fdc20204 fdc20208 fdc2020c fdc2021c; do
	printf '%s: ' "$reg"
	$DEVMEM 0x$reg 32 2>/dev/null || echo "(devmem unavailable)"
done
cat <<'EOF'

0x200 = reboot-mode register (U-Boot clears it to 0x5242C300)
0x208/0x20c = DRAM size tag (used by U-Boot dmc driver)
0x21c = cleared across warm reset (do not use as a reboot marker)
EOF

section "SC8886 charger status"
CHG_BUS=""
for d in /sys/bus/i2c/devices/*-006b; do
	[ -e "$d/name" ] || continue
	CHG_BUS=$(basename "$d" | cut -d- -f1)
done
echo "charger i2c bus: ${CHG_BUS:-not-found}"
if [ -n "$CHG_BUS" ]; then
	for r in 20 21 26 32 36 37; do
		run $I2CGET -f -y "$CHG_BUS" 0x6b 0x$r w
	done
fi

section "power supplies"
for p in /sys/class/power_supply/*; do
	echo "--- ${p##*/}"
	for f in online status present capacity current_now voltage_now; do
		[ -r "$p/$f" ] && printf '  %s = %s\n' "$f" "$(cat "$p/$f")"
	done
done

section "LED nodes and triggers"
for d in /sys/class/leds/*; do
	case "${d##*/}" in
	white:*|red:*|green:*)
		echo "--- ${d##*/}"
		printf '  brightness=%s\n' "$(cat "$d/brightness" 2>/dev/null)"
		printf '  active trigger: '
		grep -o '\[[a-z0-9:_-]*\]' "$d/trigger" 2>/dev/null || echo none
		;;
	esac
done

section "boot timing (dmesg)"
dmesg -T 2>/dev/null | head -3
dmesg -T 2>/dev/null | grep -Ei "drm|edp|panel|backlight|bq257|sc8886|rk8xx|rk809|leds|gpio-leds|tcpci|husb" | head -80

section "done"
