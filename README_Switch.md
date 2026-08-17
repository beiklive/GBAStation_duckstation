## Installation

This branch builds the GPL Switch port with the Tico overlay enabled for the nogui frontend.

To use it put the switch folder from the 7z file onto the root of your SD card, so that there is a `tico-duckstation` folder inside the `switch` folder.

Alternatively you can also put the tico-duckstation folder wherever you like.

## Local Switch build

Local builds run directly in an MSYS2 UCRT64 shell; Docker is only used by
GitHub Actions. Install devkitPro's Switch toolchain and portlibs, then install
CMake, Ninja, Git, Bison, Flex, pkg-config, Meson, Python 3 and Python Mako in
MSYS2.

```bash
./build_local.sh -j "$(nproc)"
```

The script downloads and builds the required UAM library on its first run. The
result is `build-local/GBAStationDuckStationStub.nro`. Use `--clean` for a
fresh build, or set `BUILD_DIR` to choose another output directory.
