#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
COMMON_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/../common" && pwd)
. "$COMMON_DIR/env.sh"

"$COMMON_DIR/pack-rootfs.sh" "$SCRIPT_DIR" >/dev/null
exec "$COMMON_DIR/run-qemu.sh" 00-quickstart
