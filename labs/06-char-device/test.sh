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
		'insmod /lib/modules/bsp_rtc_chrdev.ko' \
		'[ -c /dev/bsp_rtc0 ] || { echo TEST:missing-device; exit 1; }' \
		'/bin/bsp_rtc_demo /dev/bsp_rtc0 1' \
		'echo TEST:ok'
) | timeout -s KILL 12s "$SCRIPT_DIR/run.sh" >"$log" 2>&1
set -e
grep -q 'probe OK: /dev/bsp_rtc0' "$log"
grep -q 'select: ready' "$log"
grep -q 'read: sequence=' "$log"
grep -q 'ioctl: count=' "$log"
grep -q 'TEST:ok' "$log"
printf '%s\n' 'lab 06 char device: PASS'
