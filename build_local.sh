#!/usr/bin/env bash
# Local Nintendo Switch build. Run this from an MSYS2 UCRT64 shell.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build-local}"
JOBS="${JOBS:-$(nproc)}"
UAM_PREFIX="${UAM_PREFIX:-$BUILD_DIR/uam}"
CLEAN=0

usage() {
  cat <<'EOF'
Usage: ./build_local.sh [-j JOBS] [--clean]

Requires an MSYS2 UCRT64 shell with devkitPro Switch tools, CMake, Ninja,
Meson, Git, Bison, Flex, pkg-config and python-mako installed.
Output: build-local/GBAStationDuckStationStub.nro (or $BUILD_DIR when set).
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -j|--jobs) JOBS="${2:?missing job count}"; shift 2 ;;
    --clean) CLEAN=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown argument: $1" >&2; usage >&2; exit 2 ;;
  esac
done

export DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
for tool in cmake ninja meson git bison flex pkg-config; do
  command -v "$tool" >/dev/null 2>&1 || { echo "Missing $tool in MSYS2 UCRT64." >&2; exit 1; }
done
[[ -x "$DEVKITPRO/portlibs/switch/bin/aarch64-none-elf-cmake" ]] || {
  echo "Missing devkitPro Switch CMake toolchain under $DEVKITPRO." >&2; exit 1;
}

if [[ "$CLEAN" == 1 ]]; then
  rm -rf "$BUILD_DIR"
fi

if [[ ! -f "$UAM_PREFIX/lib/libuam.a" || ! -f "$UAM_PREFIX/include/uam.h" ]]; then
  UAM_SOURCE="$BUILD_DIR/_deps/uam"
  rm -rf "$UAM_SOURCE"
  mkdir -p "$(dirname "$UAM_SOURCE")" "$UAM_PREFIX/lib" "$UAM_PREFIX/include"
  git clone --depth 1 --branch library-target https://github.com/RSDuck/uam.git "$UAM_SOURCE"
  printf "%s\n" "option('build_as_library', type: 'boolean', value: false)" > "$UAM_SOURCE/meson_options.txt"
  meson setup "$UAM_SOURCE/build" -Dbuild_as_library=true --cross-file="$UAM_SOURCE/crossfile"
  ninja -C "$UAM_SOURCE/build"
  "$DEVKITPRO/devkitA64/bin/aarch64-none-elf-ar" rcs "$UAM_PREFIX/lib/libuam.a" \
    $(find "$UAM_SOURCE/build" -name '*.o' | sort)
  install -m644 "$UAM_SOURCE/source/uam.h" "$UAM_PREFIX/include/uam.h"
fi

"$DEVKITPRO/portlibs/switch/bin/aarch64-none-elf-cmake" -S "$ROOT" -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_NOGUI_FRONTEND=ON \
  -DBUILD_QT_FRONTEND=OFF \
  -DENABLE_OPENGL=OFF \
  -DENABLE_VULKAN=OFF \
  -DENABLE_CUBEB=OFF \
  -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON \
  -DUAM_PREFIX="$UAM_PREFIX"
cmake --build "$BUILD_DIR" --target GBAStationDuckStationStub.nro --parallel "$JOBS"
test -s "$BUILD_DIR/GBAStationDuckStationStub.nro"
echo "Output: $BUILD_DIR/GBAStationDuckStationStub.nro"
