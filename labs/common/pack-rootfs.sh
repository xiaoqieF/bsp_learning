#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$SCRIPT_DIR/env.sh"

[ "$#" -ge 1 ] || die "usage: $0 LAB_DIR [OVERLAY_DIR]"
lab_dir=$(CDPATH= cd -- "$1" && pwd)
lab_name=$(basename "$lab_dir")
lab_out="$OUT_DIR/$lab_name"
staging="$lab_out/rootfs"
overlay_dir=${2:-$lab_dir/rootfs-overlay}

rm -rf "$staging"
mkdir -p "$staging"
cp -a "$ROOTFS_DIR/." "$staging/"
mkdir -p "$staging/lib/modules" "$staging/tmp"

if [ -d "$overlay_dir" ]; then
	cp -a "$overlay_dir/." "$staging/"
fi

mkdir -p "$lab_out"
(
	cd "$staging"
	find . -print0 | cpio --null -o -H newc 2>/dev/null | gzip -9 > "$lab_out/rootfs.cpio.gz"
)

printf '%s\n' "$lab_out/rootfs.cpio.gz"
