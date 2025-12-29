#!/usr/bin/env bash
set -euo pipefail

# Cross-build static FFmpeg for Windows on Ubuntu (uses mingw-w64)
# Expects:
#  PREFIX - install prefix (default: /tmp/ffmpeg-static-windows)
#  CROSS_PREFIX - cross toolchain prefix (default: x86_64-w64-mingw32-)
# Optional environment variables:
#  FFMPEG_VERSION, LIBJPEG_TURBO_VERSION

FFMPEG_VERSION="${FFMPEG_VERSION:-6.1.1}"
LIBJPEG_TURBO_VERSION="${LIBJPEG_TURBO_VERSION:-3.0.4}"
PREFIX="${PREFIX:-/tmp/ffmpeg-static-windows}"
CROSS_PREFIX="${CROSS_PREFIX:-x86_64-w64-mingw32-}"
NUM_CORES=$(nproc)

echo "============================================================================"
echo "FFmpeg cross-build (Linux -> Windows static)"
echo "FFmpeg: ${FFMPEG_VERSION}, libjpeg-turbo: ${LIBJPEG_TURBO_VERSION}"
echo "Prefix: ${PREFIX}, Cross: ${CROSS_PREFIX}, Cores: ${NUM_CORES}"
echo "============================================================================"

# Detect environment and give actionable message if run on Windows/cmd
if ! command -v apt-get >/dev/null 2>&1 && ! command -v apt >/dev/null 2>&1; then
  echo "ERROR: apt/apt-get not found in PATH. This script is intended to run on Ubuntu (CI runner, WSL, or Docker)." >&2
  echo "If you're on Windows, run via WSL or Docker. Example WSL command:" >&2
  echo "  wsl -d ubuntu-22.04 -- bash -lc 'cd /mnt/c/your/path/to/repo && PREFIX=/tmp/ffmpeg-static-windows CROSS_PREFIX=x86_64-w64-mingw32- bash build-script/build-static-ffmpeg-cross.sh'" >&2
  echo "Or run the provided helper: build-script/run-cross-build.ps1 (PowerShell) which will use WSL or Docker if available." >&2
  exit 1
fi

# Ensure toolchain present
for tool in ${CROSS_PREFIX}gcc ${CROSS_PREFIX}ar ${CROSS_PREFIX}nm ${CROSS_PREFIX}ranlib ${CROSS_PREFIX}strip; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "Error: required tool $tool not found in PATH" >&2
    exit 1
  fi
done

# Verify GCC major version if requested
if [ -n "${MINGW_GCC_MAJOR:-}" ]; then
  ver="$(${CROSS_PREFIX}gcc -dumpversion 2>/dev/null || true)"
  major="${ver%%.*}"
  echo "Cross GCC version: ${ver} (major ${major}), expected ${MINGW_GCC_MAJOR}"
  if [ "${major}" != "${MINGW_GCC_MAJOR}" ]; then
    echo "ERROR: cross GCC major ${major} does not match expected ${MINGW_GCC_MAJOR}" >&2
    exit 1
  fi
fi

mkdir -p "$PREFIX"
WORKDIR="$(pwd)/ffmpeg-cross-build"
rm -rf "$WORKDIR"
mkdir -p "$WORKDIR"
cd "$WORKDIR"

# Export cross compilers
export CC=${CROSS_PREFIX}gcc
export CXX=${CROSS_PREFIX}g++
export AR=${CROSS_PREFIX}ar
export NM=${CROSS_PREFIX}nm
export RANLIB=${CROSS_PREFIX}ranlib
export STRIP=${CROSS_PREFIX}strip

# Build libjpeg-turbo (cross)
if [ ! -f "${PREFIX}/lib/libturbojpeg.a" ]; then
  echo "Building libjpeg-turbo ${LIBJPEG_TURBO_VERSION}..."
  curl -L -o libjpeg-turbo.tar.gz "https://github.com/libjpeg-turbo/libjpeg-turbo/archive/refs/tags/${LIBJPEG_TURBO_VERSION}.tar.gz"
  tar xzf libjpeg-turbo.tar.gz
  pushd "libjpeg-turbo-${LIBJPEG_TURBO_VERSION}"
  mkdir -p build && cd build
  cmake .. \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER="$CC" \
    -DCMAKE_CXX_COMPILER="$CXX" \
    -DENABLE_SHARED=OFF \
    -DENABLE_STATIC=ON
  make -j${NUM_CORES}
  make install
  popd
else
  echo "libjpeg-turbo already installed in ${PREFIX}"
fi

# Download FFmpeg
if [ ! -d "ffmpeg-${FFMPEG_VERSION}" ]; then
  echo "Downloading FFmpeg ${FFMPEG_VERSION}..."
  curl -L -o "ffmpeg-${FFMPEG_VERSION}.tar.bz2" "https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.bz2"
  tar xf "ffmpeg-${FFMPEG_VERSION}.tar.bz2"
fi

cd "ffmpeg-${FFMPEG_VERSION}"

export PKG_CONFIG_PATH="${PREFIX}/lib/pkgconfig:${PKG_CONFIG_PATH:-}"

./configure \
  --prefix="$PREFIX" \
  --cross-prefix="${CROSS_PREFIX}" \
  --arch=x86_64 \
  --target-os=mingw32 \
  --enable-static \
  --disable-shared \
  --pkg-config-flags="--static" \
  --extra-cflags="-I${PREFIX}/include -static" \
  --extra-ldflags="-L${PREFIX}/lib -static" \
  --disable-doc \
  --disable-programs \
  --enable-gpl \
  --enable-libjpeg \
  --enable-decoder=mjpeg

make -j${NUM_CORES}
make install

# Verify installation
echo "Verifying installation in ${PREFIX}"
if [ -d "${PREFIX}/include/libavcodec" ] && [ -f "${PREFIX}/lib/libavcodec.a" ] && [ -f "${PREFIX}/lib/libturbojpeg.a" ]; then
  echo "FFmpeg cross-built and installed to ${PREFIX}"
else
  echo "ERROR: expected artifacts missing in ${PREFIX}" >&2
  ls -la "${PREFIX}"
  exit 1
fi

# Verify MJPEG decoder is present in libavcodec
if command -v nm >/dev/null 2>&1 && [ -f "${PREFIX}/lib/libavcodec.a" ]; then
  if nm -g --defined-only "${PREFIX}/lib/libavcodec.a" | grep -i "mjpeg" >/dev/null 2>&1; then
    echo "✅ MJPEG decoder symbols found in libavcodec.a"
  else
    echo "ERROR: MJPEG decoder not found in libavcodec.a" >&2
    echo "Listing libavcodec.a contents (first 200 lines):"
    nm -g --defined-only "${PREFIX}/lib/libavcodec.a" | head -n 200 || true
    exit 1
  fi
else
  echo "WARNING: 'nm' not available or libavcodec.a missing; skipping MJPEG symbol check"
fi

# Done
echo "Artifact available at ${PREFIX} (contains include/ and lib/)."

exit 0
