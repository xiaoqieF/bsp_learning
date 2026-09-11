#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
COMMON_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/../common" && pwd)
. "$COMMON_DIR/env.sh"

[ -f "$OUT_DIR/04-pl061-gpio/qemu-virt-gpio.dtb" ] || "$SCRIPT_DIR/build.sh"
exec "$COMMON_DIR/run-qemu.sh" 04-pl061-gpio \
	-dtb "$OUT_DIR/04-pl061-gpio/qemu-virt-gpio.dtb"
