#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
COMMON_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/../common" && pwd)
. "$COMMON_DIR/env.sh"

: "${U_BOOT_DIR:?请设置 U_BOOT_DIR，例如 U_BOOT_DIR=/path/to/u-boot}"
[ -d "$U_BOOT_DIR" ] || die "U_BOOT_DIR does not exist: $U_BOOT_DIR"
require_command mkfs.ext4

make -C "$U_BOOT_DIR" qemu_arm64_defconfig
make -C "$U_BOOT_DIR" ARCH=arm64 CROSS_COMPILE="$CROSS_COMPILE" -j"${JOBS:-$(getconf _NPROCESSORS_ONLN)}"

lab_out="$OUT_DIR/08-u-boot"
staging="$lab_out/staging"
mkdir -p "$staging"

"$COMMON_DIR/pack-rootfs.sh" "$SCRIPT_DIR" >/dev/null
cp -f "$KERNEL_DIR/arch/arm64/boot/Image" "$staging/Image"
dtc -I dts -O dtb -o "$staging/qemu-virt.dtb" \
	"$WORKSPACE_DIR/docs/qemu-virt.dts"
cp -f "$lab_out/rootfs.cpio.gz" "$staging/rootfs.cpio.gz"
truncate -s 128M "$lab_out/boot.ext4"
mkfs.ext4 -F -L bsp-boot -d "$staging" "$lab_out/boot.ext4" >/dev/null
cp -f "$U_BOOT_DIR/u-boot.bin" "$lab_out/u-boot.bin"

printf '%s\n' "built: $lab_out"
