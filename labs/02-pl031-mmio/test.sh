#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
"$SCRIPT_DIR/build.sh" >/dev/null

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
		'echo TEST:reg_data=$(cat "$device/reg_data")' \
		'echo TEST:raw_status=$(cat "$device/raw_status")' \
		'rmmod bsp_pl031' \
		'echo TEST:ok'
) | timeout -s KILL 12s "$SCRIPT_DIR/run.sh" >"$log" 2>&1
set -e

grep -q 'TEST:reg_data=0x' "$log"
grep -q 'probe OK' "$log"
grep -q 'remove OK' "$log"
grep -q 'TEST:ok' "$log"
printf '%s\n' 'lab 02: PASS'
