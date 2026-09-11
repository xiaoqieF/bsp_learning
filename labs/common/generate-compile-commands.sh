#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$SCRIPT_DIR/env.sh"

require_command python3
require_command make

set -- "$WORKSPACE_DIR"/labs/*/src

for module_dir do
	[ -d "$module_dir" ] || continue
	make -C "$KERNEL_DIR" \
		ARCH="$ARCH" \
		CROSS_COMPILE="$CROSS_COMPILE" \
		M="$module_dir" \
		modules
done

python3 "$KERNEL_DIR/scripts/clang-tools/gen_compile_commands.py" \
	-d "$KERNEL_DIR" \
	-o "$WORKSPACE_DIR/compile_commands.json" \
	"$@"

printf '%s\n' "Generated $WORKSPACE_DIR/compile_commands.json"
