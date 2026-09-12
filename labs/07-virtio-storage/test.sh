#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
"$SCRIPT_DIR/build.sh" >/dev/null

first_log=$(mktemp)
second_log=$(mktemp)
trap 'rm -f "$first_log" "$second_log"' EXIT

set +e
(
	sleep 2
	printf '%s\n' \
		'mount | grep " / "' \
		'cat /proc/1/cmdline' \
		'mount -o remount,rw /' \
		'echo persistent-data > /root/persistent-data' \
		'sync' \
		'echo TEST:first-boot'
) | timeout -s KILL 12s "$SCRIPT_DIR/run.sh" >"$first_log" 2>&1
first_status=$?

(
	sleep 2
	printf '%s\n' \
		'mount | grep " / "' \
		'cat /root/persistent-data' \
		'echo TEST:second-boot'
) | timeout -s KILL 12s "$SCRIPT_DIR/run.sh" >"$second_log" 2>&1
second_status=$?
set -e

grep -q 'VFS: Mounted root (ext4 filesystem)' "$first_log"
grep -q '/sbin/init' "$first_log"
grep -q 'TEST:first-boot' "$first_log"
grep -q 'persistent-data' "$second_log"
grep -q 'TEST:second-boot' "$second_log"

printf '%s\n' "lab 07: PASS (qemu statuses $first_status/$second_status)"
