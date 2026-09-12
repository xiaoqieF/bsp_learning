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
		'insmod /lib/modules/bsp_edu.ko' \
		'device=' \
		'for candidate in /sys/bus/pci/devices/*; do [ -e "$candidate/id" ] || continue; device=$candidate; break; done' \
		'[ -n "$device" ] || { echo TEST:missing-device; exit 1; }' \
		'echo TEST:id=$(cat "$device/id")' \
		'echo 1 > "$device/raise_irq"' \
		'sleep 1' \
		'count=$(cat "$device/irq_count")' \
		'echo TEST:irq_count=$count' \
		'[ "$count" -ge 1 ] || exit 1' \
		'echo TEST:ok'
) | timeout -s KILL 12s "$SCRIPT_DIR/run.sh" >"$log" 2>&1
set -e

grep -q 'probe OK' "$log"
grep -q 'TEST:id=0x010000ed' "$log"
grep -q 'TEST:irq_count=1' "$log"
grep -q 'IRQ handled' "$log"
grep -q 'TEST:ok' "$log"
printf '%s\n' 'lab 08: PASS'
