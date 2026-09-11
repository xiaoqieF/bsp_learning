#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
COMMON_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/../common" && pwd)
. "$COMMON_DIR/env.sh"

dtb="$OUT_DIR/01-device-tree/qemu-virt.dtb"
if [ ! -f "$dtb" ] || [ "$(wc -c < "$dtb")" -ge 65536 ]; then
	"$SCRIPT_DIR/build.sh"
fi
exec "$COMMON_DIR/run-qemu.sh" 01-device-tree \
	-dtb "$dtb"
