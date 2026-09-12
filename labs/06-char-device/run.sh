#!/bin/sh

set -eu
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
COMMON_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/../common" && pwd)
. "$COMMON_DIR/env.sh"
[ -f "$OUT_DIR/06-char-device/qemu-virt-rtc-char.dtb" ] || "$SCRIPT_DIR/build.sh"
exec "$COMMON_DIR/run-qemu.sh" 06-char-device -dtb "$OUT_DIR/06-char-device/qemu-virt-rtc-char.dtb"
