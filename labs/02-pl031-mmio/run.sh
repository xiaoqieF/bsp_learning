#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
COMMON_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/../common" && pwd)
. "$COMMON_DIR/env.sh"

[ -f "$OUT_DIR/02-pl031-mmio/qemu-virt-pl031.dtb" ] || "$SCRIPT_DIR/build.sh"
exec "$COMMON_DIR/run-qemu.sh" 02-pl031-mmio \
	-dtb "$OUT_DIR/02-pl031-mmio/qemu-virt-pl031.dtb"
