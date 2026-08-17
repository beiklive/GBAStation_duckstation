#!/usr/bin/env bash
# Local Nintendo Switch build. Run this from an MSYS2 UCRT64 shell.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build-local}"
JOBS="${JOBS:-$(nproc)}"
UAM_PREFIX="${UAM_PREFIX:-$BUILD_DIR/uam}"
TMPDIR="${TMPDIR:-$BUILD_DIR/tmp}"
PYTHON3="${PYTHON3:-}"
MESON_BIN="${MESON_BIN:-}"
CLEAN=0

# devkitPro's CMake toolchain emits POSIX paths. Use MSYS CMake/Ninja even
# when launched from UCRT64, otherwise native ninja.exe cannot resolve /e/... .
if [[ "$(uname -o 2>/dev/null || true)" == "Msys" ]]; then
  export PATH="/usr/bin:/bin:$PATH"
  if ! command -v python3 >/dev/null 2>&1; then
    for python_dir in /ucrt64/bin /mingw64/bin; do
      [[ -x "$python_dir/python3.exe" ]] || continue
      PYTHON3="$python_dir/python3.exe"
      break
    done
  fi
fi
if [[ -z "$PYTHON3" ]]; then
  PYTHON3=$(command -v python3 || true)
fi
if [[ -z "$MESON_BIN" ]]; then
  MESON_BIN=$(command -v meson || true)
fi
if [[ -z "$MESON_BIN" && "$(uname -o 2>/dev/null || true)" == "Msys" ]]; then
  for meson_candidate in /ucrt64/bin/meson.exe /mingw64/bin/meson.exe; do
    [[ -x "$meson_candidate" ]] || continue
    MESON_BIN="$meson_candidate"
    break
  done
fi

usage() {
  cat <<'EOF'
Usage: ./build_local.sh [-j JOBS] [--clean]

Requires an MSYS2 shell with devkitPro Switch tools, CMake, Ninja,
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
for tool in cmake ninja git bison flex pkg-config; do
  command -v "$tool" >/dev/null 2>&1 || { echo "Missing $tool in MSYS2." >&2; exit 1; }
done
[[ -n "$MESON_BIN" ]] || { echo "Missing Meson in MSYS2." >&2; exit 1; }
[[ -n "$PYTHON3" ]] && "$PYTHON3" -c 'import mako' >/dev/null 2>&1 || {
  echo "Missing Python Mako in MSYS2." >&2; exit 1;
}
[[ -x "$DEVKITPRO/portlibs/switch/bin/aarch64-none-elf-cmake" ]] || {
  echo "Missing devkitPro Switch CMake toolchain under $DEVKITPRO." >&2; exit 1;
}

if [[ "$CLEAN" == 1 ]]; then
  rm -rf "$BUILD_DIR"
fi

# The devkitA64 compiler uses TMPDIR for intermediate files.  Point it into
# the build tree so a normal MSYS2 user does not need write access to MSYS2's
# installation-wide tmp directory.
mkdir -p "$TMPDIR"
export TMPDIR
# devkitA64's Windows-hosted GCC consults TMP/TEMP before TMPDIR.
export TMP="$TMPDIR"
export TEMP="$TMPDIR"

if [[ ! -f "$UAM_PREFIX/lib/libuam.a" || ! -f "$UAM_PREFIX/include/uam.h" ]]; then
  UAM_SOURCE="$BUILD_DIR/_deps/uam"
  rm -rf "$UAM_SOURCE"
  mkdir -p "$(dirname "$UAM_SOURCE")" "$UAM_PREFIX/lib" "$UAM_PREFIX/include"
  git clone --depth 1 --branch library-target https://github.com/RSDuck/uam.git "$UAM_SOURCE"
  printf "%s\n" "option('build_as_library', type: 'boolean', value: false)" > "$UAM_SOURCE/meson_options.txt"
  # UAM's old Meson check imports distutils, which was removed in Python 3.12.
  # Importing Mako directly remains sufficient for its generated sources.
  sed -i '/from distutils\.version import StrictVersion/d; /assert StrictVersion(mako\.__version__)/d' "$UAM_SOURCE/meson.build"
  UAM_CROSS_FILE="$UAM_SOURCE/crossfile"
  if [[ "$(uname -o 2>/dev/null || true)" == "Msys" ]]; then
    UAM_CROSS_FILE="$UAM_SOURCE/crossfile-local"
    devkitpro_native=$(cygpath -m "$DEVKITPRO")
    sed "s#/opt/devkitpro#$devkitpro_native#g" "$UAM_SOURCE/crossfile" > "$UAM_CROSS_FILE"
  fi
  "$MESON_BIN" setup "$UAM_SOURCE/build" "$UAM_SOURCE" -Dbuild_as_library=true --cross-file="$UAM_CROSS_FILE"
  ninja -C "$UAM_SOURCE/build"
  "$DEVKITPRO/devkitA64/bin/aarch64-none-elf-ar" rcs "$UAM_PREFIX/lib/libuam.a" \
    $(find "$UAM_SOURCE/build" -name '*.o' | sort)
  install -m644 "$UAM_SOURCE/source/uam.h" "$UAM_PREFIX/include/uam.h"
fi

# Keep this path relative to CMake's build directory. This works for the
# native Windows CMake launcher and is resolved to an MSYS path in CMake.
UAM_PREFIX_CMAKE=$(realpath --relative-to="$BUILD_DIR" "$UAM_PREFIX")

"$DEVKITPRO/portlibs/switch/bin/aarch64-none-elf-cmake" -S "$ROOT" -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_NOGUI_FRONTEND=ON \
  -DBUILD_QT_FRONTEND=OFF \
  -DENABLE_OPENGL=OFF \
  -DENABLE_VULKAN=OFF \
  -DENABLE_CUBEB=OFF \
  -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON \
  -DUAM_PREFIX="$UAM_PREFIX_CMAKE"
cmake --build "$BUILD_DIR" --target GBAStationDuckStationStub.nro --parallel "$JOBS"
test -s "$BUILD_DIR/GBAStationDuckStationStub.nro"
echo "Output: $BUILD_DIR/GBAStationDuckStationStub.nro"
