#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
COMMON_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/../common" && pwd)
. "$COMMON_DIR/env.sh"

lab_out="$OUT_DIR/07-virtio-storage"
disk="$lab_out/rootfs.ext4"
mkdir -p "$lab_out"

truncate -s 128M "$disk"
mkfs.ext4 -F -L bsp-rootfs -d "$ROOTFS_DIR" "$disk" >/dev/null

printf '%s\n' "built: $disk"
