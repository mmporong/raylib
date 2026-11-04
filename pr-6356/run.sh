#!/bin/bash
# Build the harness against two raylib builds and write the before/after PNGs.
#
#   ./run.sh <raylib master src dir> <raylib PR src dir> <output dir>
#
# Each src dir must contain raylib.h and a static libraylib.a built from that commit,
# for example with: make -C src PLATFORM=PLATFORM_DESKTOP RAYLIB_LIBTYPE=STATIC
set -euo pipefail

HERE=$(cd "$(dirname "$0")" && pwd)
MASTER_SRC=$1
PR_SRC=$2
OUT=$3
LIBS="-lGL -lm -lpthread -ldl -lrt -lX11"
CFLAGS="-std=c99 -O2 -Wall -Wextra"
SCALE=24

mkdir -p "$OUT/raw" "$OUT/bin"

cc $CFLAGS "$HERE/rotate_check.c" -I"$MASTER_SRC" "$MASTER_SRC/libraylib.a" $LIBS -o "$OUT/bin/rotate_check_master"
cc $CFLAGS "$HERE/rotate_check.c" -I"$PR_SRC" "$PR_SRC/libraylib.a" $LIBS -o "$OUT/bin/rotate_check_pr"
cc $CFLAGS "$HERE/compose.c" -I"$PR_SRC" "$PR_SRC/libraylib.a" $LIBS -o "$OUT/bin/compose"

"$OUT/bin/rotate_check_master" master "$OUT/raw"
"$OUT/bin/rotate_check_pr" pr "$OUT/raw"

R="$OUT/raw"
C="$OUT/bin/compose"

# Source image
"$C" "$OUT/source.png" $SCALE "$R/pr_edge_source.png"

# Right angles: master | this PR | exact reference (ImageRotateCW / ImageRotateCCW)
"$C" "$OUT/rotate_090.png" $SCALE "$R/master_edge_rot090.png" "$R/pr_edge_rot090.png" "$R/pr_edge_ref090.png"
"$C" "$OUT/rotate_180.png" $SCALE "$R/master_edge_rot180.png" "$R/pr_edge_rot180.png" "$R/pr_edge_ref180.png"
"$C" "$OUT/rotate_m090.png" $SCALE "$R/master_edge_rotm090.png" "$R/pr_edge_rotm090.png" "$R/pr_edge_refm090.png"

# Other angles: master | this PR
"$C" "$OUT/rotate_045.png" $SCALE "$R/master_edge_rot045.png" "$R/pr_edge_rot045.png"
"$C" "$OUT/rotate_030.png" $SCALE "$R/master_edge_rot030.png" "$R/pr_edge_rot030.png"

ls -l "$OUT"/*.png
