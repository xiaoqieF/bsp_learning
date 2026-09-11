#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$SCRIPT_DIR/env.sh"

[ "$#" -eq 2 ] || die "usage: $0 MODULE_SOURCE_DIR OUTPUT_DIR"
module_dir=$(CDPATH= cd -- "$1" && pwd)
mkdir -p "$2"
output_dir=$(CDPATH= cd -- "$2" && pwd)
require_command flock

lock_file="${TMPDIR:-/tmp}/bsp-learn-kbuild.lock"
exec 9>"$lock_file"
flock 9

if [ ! -f "$KERNEL_DIR/Module.symvers" ]; then
	printf '%s\n' "Module.symvers is missing; building kernel modules once..." >&2
	make -C "$KERNEL_DIR" \
		ARCH="$ARCH" \
		CROSS_COMPILE="$CROSS_COMPILE" \
		-j"${JOBS:-$(getconf _NPROCESSORS_ONLN)}" \
		modules
fi

make -C "$KERNEL_DIR" \
	ARCH="$ARCH" \
	CROSS_COMPILE="$CROSS_COMPILE" \
	M="$module_dir" \
	modules

find "$module_dir" -maxdepth 1 -type f -name '*.ko' -exec cp -f {} "$output_dir/" \;
