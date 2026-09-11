#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$SCRIPT_DIR/env.sh"

for command_name in qemu-system-aarch64 dtc fdtget fdtput cpio gzip mkfs.ext4 timeout flock; do
	require_command "$command_name"
done

printf '%s\n' "toolchain and base artifacts: OK"
