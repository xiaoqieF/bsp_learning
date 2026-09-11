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
		'insmod /lib/modules/bsp_gpio_consumer.ko' \
		'device=' \
		'for candidate in /sys/bus/platform/devices/*; do [ -e "$candidate/led" ] || continue; device=$candidate; break; done' \
		'[ -n "$device" ] || { echo TEST:missing-device; exit 1; }' \
		'echo 1 > "$device/led"' \
		'value=$(cat "$device/led")' \
		'echo TEST:led=$value' \
		'[ "$value" = 1 ] || exit 1' \
		'echo TEST:ok'
) | timeout -s KILL 12s "$SCRIPT_DIR/run.sh" >"$log" 2>&1
set -e

grep -q 'probe OK' "$log"
grep -q 'TEST:led=1' "$log"
grep -q 'TEST:ok' "$log"
printf '%s\n' 'lab 04: PASS'
