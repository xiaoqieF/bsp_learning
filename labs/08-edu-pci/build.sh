#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
COMMON_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/../common" && pwd)
. "$COMMON_DIR/env.sh"

lab_out="$OUT_DIR/08-edu-pci"
module_src="$lab_out/module-src"
mkdir -p "$lab_out"
rm -rf "$module_src"
mkdir -p "$module_src"
cp -f "$SCRIPT_DIR/src/Makefile" "$SCRIPT_DIR/src"/*.c "$module_src/"
"$COMMON_DIR/build-module.sh" "$module_src" "$lab_out/modules"
mkdir -p "$lab_out/rootfs-overlay/lib/modules"
cp -f "$lab_out/modules/bsp_edu.ko" "$lab_out/rootfs-overlay/lib/modules/"
"$COMMON_DIR/pack-rootfs.sh" "$SCRIPT_DIR" \
	"$lab_out/rootfs-overlay" >/dev/null

printf '%s\n' "built: $lab_out"
