#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
LAB02_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/../02-pl031-mmio" && pwd)
"$LAB02_DIR/build.sh" >/dev/null

log=$(mktemp)
trap 'rm -f "$log"' EXIT
set +e
(
	sleep 2
	printf '%s\n' \
		'insmod /lib/modules/bsp_pl031.ko' \
		'device=' \
		'for candidate in /sys/bus/platform/devices/*; do [ -e "$candidate/reg_data" ] || continue; device=$candidate; break; done' \
		'[ -n "$device" ] || { echo TEST:missing-device; exit 1; }' \
		'driver=/sys/bus/platform/drivers/bsp-pl031' \
		'name=$(basename "$device")' \
		'echo "$name" > "$driver/unbind"' \
		'[ ! -e "$device/reg_data" ] || exit 1' \
		'echo "$name" > "$driver/bind"' \
		'[ -e "$device/reg_data" ] || exit 1' \
		'echo TEST:ok'
) | timeout -s KILL 12s "$LAB02_DIR/run.sh" >"$log" 2>&1
set -e

grep -q 'remove OK' "$log"
grep -q 'probe OK' "$log"
grep -q 'TEST:ok' "$log"
printf '%s\n' 'lab 05: PASS'
