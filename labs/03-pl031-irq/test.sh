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
		'for candidate in /sys/bus/platform/devices/*; do [ -e "$candidate/alarm_seconds" ] || continue; device=$candidate; break; done' \
		'[ -n "$device" ] || { echo TEST:missing-device; exit 1; }' \
		'echo 2 > "$device/alarm_seconds"' \
		'sleep 3' \
		'count=$(cat "$device/irq_count")' \
		'echo TEST:irq_count=$count' \
		'[ "$count" -ge 1 ] || exit 1' \
		'echo TEST:ok'
) | timeout -s KILL 12s "$SCRIPT_DIR/run.sh" >"$log" 2>&1
set -e

grep -q 'TEST:irq_count=1' "$log"
grep -q 'alarm IRQ handled' "$log"
grep -q 'TEST:ok' "$log"
printf '%s\n' 'lab 03: PASS'
