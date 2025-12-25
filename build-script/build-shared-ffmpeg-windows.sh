#!/bin/bash
# ============================================================================
# FFmpeg Shared Build Script - Windows (external MinGW)
# ============================================================================
# This script builds shared FFmpeg and libjpeg-turbo on Windows using an external
# MinGW toolchain (e.g., C:\mingw64) and a bash shell (Git Bash).
# ============================================================================

set -e  # Exit on error
set -u  # Exit on undefined variable

# Quick help / usage
if [ "${1:-}" = "--help" ] || [ "${1:-}" = "-h" ]; then
    cat <<'EOF'
Usage: ./build-shared-ffmpeg-windows.sh [ENV=VAL ...]

This script is intended to be run from an MSYS2 "MSYS2 MinGW 64-bit" shell.
Recommended example:
  SKIP_MSYS_MINGW=1 EXTERNAL_MINGW_MSYS=/mingw64 ./build-shared-ffmpeg-windows.sh

Environment variables supported by this script (set before running / from caller):
  SKIP_MSYS_MINGW=1          -> Skip package-managed install and prefer external MinGW (caller should set EXTERNAL_MINGW_MSYS)
  EXTERNAL_MINGW_MSYS=/c/mingw64 -> Path to external mingw in MSYS-style (set by caller wrapper when using EXTERNAL_MINGW)
  ENABLE_NVENC=1             -> Attempt to enable NVENC support (requires NVENC SDK/headers)
  NVENC_SDK_PATH=...         -> Path to NVENC SDK (optional, used when ENABLE_NVENC=1)
EOF
    exit 0
fi

# Detect MSYS2 MinGW64 and set sensible defaults
if [ -n "${MSYSTEM:-}" ] && printf '%s
' "${MSYSTEM}" | grep -qi '^MINGW64'; then
    echo "Detected MSYS2 MINGW64 (MSYSTEM=${MSYSTEM}); defaulting EXTERNAL_MINGW_MSYS=/mingw64"
    : "${EXTERNAL_MINGW_MSYS:=/mingw64}"
fi

# If EXTERNAL_MINGW_MSYS not provided, try common locations
if [ -z "${EXTERNAL_MINGW_MSYS:-}" ]; then
    if [ -d "/mingw64/bin" ]; then
        EXTERNAL_MINGW_MSYS="/mingw64"
    elif [ -d "/c/msys64/mingw64/bin" ]; then
        EXTERNAL_MINGW_MSYS="/c/msys64/mingw64"
    elif [ -d "/c/mingw64/bin" ]; then
        EXTERNAL_MINGW_MSYS="/c/mingw64"
    fi
fi

# Helpful hint when no obvious MSYS2/Mingw detected
if [ -z "${MSYSTEM:-}" ] && [ -z "${EXTERNAL_MINGW_MSYS:-}" ]; then
    cat <<'EOF'
Warning: It looks like you may be running this script outside of MSYS2 MinGW64.
For best results open "MSYS2 MinGW 64-bit" and run:
  SKIP_MSYS_MINGW=1 EXTERNAL_MINGW_MSYS=/mingw64 ./build-shared-ffmpeg-windows.sh
EOF
fi

FFMPEG_VERSION="6.1.1"
LIBJPEG_TURBO_VERSION="3.0.4"
FFMPEG_INSTALL_PREFIX="/c/ffmpeg-shared"
BUILD_DIR="$(pwd)/ffmpeg-build-temp"
DOWNLOAD_URL="https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.bz2"
LIBJPEG_TURBO_URL="https://github.com/libjpeg-turbo/libjpeg-turbo/archive/refs/tags/${LIBJPEG_TURBO_VERSION}.tar.gz"

# Number of CPU cores for parallel compilation
NUM_CORES=$(nproc)

echo "============================================================================"
echo "FFmpeg Shared Build - Windows (external MinGW)"
echo "============================================================================"
echo "FFmpeg Version: ${FFMPEG_VERSION}"
echo "libjpeg-turbo Version: ${LIBJPEG_TURBO_VERSION}"
echo "Install Prefix: ${FFMPEG_INSTALL_PREFIX}"
echo "Build Directory: ${BUILD_DIR}"
echo "CPU Cores: ${NUM_CORES}"
echo "============================================================================"
echo ""

# Prepare/verify required toolchain and utilities (external MinGW + bash)
# Default to SKIP_MSYS_MINGW=1 (use external MinGW) unless explicitly overridden
if [ "${SKIP_MSYS_MINGW:-1}" = "1" ]; then
    echo "Step 1/8: SKIP_MSYS_MINGW set (or default) - using external MinGW build environment"
    echo "External MinGW (msys style): ${EXTERNAL_MINGW_MSYS:-/c/mingw64}"
    # Prepend external mingw to PATH so the toolchain is used
    EXTERNAL_MINGW_BIN="${EXTERNAL_MINGW_MSYS:-/c/mingw64}/bin"
    if [ -d "${EXTERNAL_MINGW_BIN}" ]; then
        export PATH="${EXTERNAL_MINGW_BIN}:$PATH"
        echo "PATH updated to prefer external MinGW: ${EXTERNAL_MINGW_BIN}"
    else
        echo "WARNING: External MinGW bin directory not found: ${EXTERNAL_MINGW_BIN}"
        echo "Please ensure your external MinGW (gcc, make, cmake, nasm/yasm) are on PATH."
    fi

    # Sanity checks for mandatory tools (gcc, make/cmake, nasm/yasm, tar, wget/git)
    MISSING=0
    command -v gcc >/dev/null 2>&1 || { echo "ERROR: gcc not found on PATH"; MISSING=1; }
    command -v cmake >/dev/null 2>&1 || { echo "ERROR: cmake not found on PATH"; MISSING=1; }
    command -v make >/dev/null 2>&1 || command -v mingw32-make >/dev/null 2>&1 || { echo "ERROR: make (or mingw32-make) not found on PATH"; MISSING=1; }
    command -v nasm >/dev/null 2>&1 || command -v yasm >/dev/null 2>&1 || { echo "ERROR: nasm or yasm not found on PATH"; MISSING=1; }
    command -v tar >/dev/null 2>&1 || { echo "ERROR: tar not found on PATH"; MISSING=1; }
    command -v wget >/dev/null 2>&1 || command -v curl >/dev/null 2>&1 || { echo "ERROR: wget or curl not found on PATH"; MISSING=1; }

    # Additional helpful checks: cmp (diffutils) and make
    command -v cmp >/dev/null 2>&1 || { echo "ERROR: cmp not found on PATH (package: diffutils). Install via pacman: pacman -S diffutils"; MISSING=1; }
    command -v make >/dev/null 2>&1 || command -v mingw32-make >/dev/null 2>&1 || command -v gmake >/dev/null 2>&1 || { echo "ERROR: make not found on PATH. Install via pacman: pacman -S --needed mingw-w64-x86_64-make"; MISSING=1; }

    if [ "$MISSING" -eq 1 ]; then
        echo "One or more required tools are missing. Install the missing tools (gcc, cmake, make/mingw32-make, nasm/yasm, tar, wget/curl, git, bash) and ensure they are on PATH."
        exit 1
    fi
else
    echo "ERROR: SKIP_MSYS_MINGW is not set to 1. Automated package-managed installation was removed from this script."
    echo "Please run this script with SKIP_MSYS_MINGW=1 (the default) and provide an external MinGW toolchain (set EXTERNAL_MINGW or ensure /c/mingw64 exists)."
    exit 1
fi

# Choose CMake generator for external MinGW
GENERATOR="MinGW Makefiles"

# Create build directory
echo "Step 2/8: Creating build directory..."
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"
echo "✓ Build directory ready: ${BUILD_DIR}"
echo ""

# Download and build libjpeg-turbo (prefer shared libs for a shared build)
echo "Step 3/8: Building libjpeg-turbo ${LIBJPEG_TURBO_VERSION} (shared)..."
if [ ! -f "${FFMPEG_INSTALL_PREFIX}/bin/libturbojpeg.dll" ] && [ ! -f "${FFMPEG_INSTALL_PREFIX}/lib/libturbojpeg.dll" ]; then
    echo "Downloading libjpeg-turbo..."
    if [ ! -f "libjpeg-turbo.tar.gz" ]; then
        wget "${LIBJPEG_TURBO_URL}" -O "libjpeg-turbo.tar.gz"
    fi
    echo "Extracting libjpeg-turbo..."
    tar xzf libjpeg-turbo.tar.gz
    cd "libjpeg-turbo-${LIBJPEG_TURBO_VERSION}"
    mkdir -p build
    cd build
    echo "Configuring libjpeg-turbo (shared): (generator: ${GENERATOR})"
    cmake .. -G "${GENERATOR}" -DCMAKE_INSTALL_PREFIX="${FFMPEG_INSTALL_PREFIX}" -DCMAKE_BUILD_TYPE=Release -DENABLE_SHARED=ON -DENABLE_STATIC=OFF -DWITH_JPEG8=ON -DWITH_TURBOJPEG=ON -DWITH_ZLIB=ON
    echo "Building libjpeg-turbo with ${NUM_CORES} cores..."
    if [ "${GENERATOR}" = "MinGW Makefiles" ]; then
        mingw32-make -j${NUM_CORES} || make -j${NUM_CORES}
        if [ $? -ne 0 ]; then
            echo "Build failed with mingw32-make, trying make..."
            make -j${NUM_CORES}
        fi
    else
        make -j${NUM_CORES}
    fi
    echo "Installing libjpeg-turbo..."
    if [ "${GENERATOR}" = "MinGW Makefiles" ]; then
        mingw32-make install || make install
    else
        make install
    fi
    cd "${BUILD_DIR}"
    rm -rf "libjpeg-turbo-${LIBJPEG_TURBO_VERSION}"
    echo "✓ libjpeg-turbo built and installed (shared)"
else
    echo "✓ libjpeg-turbo (shared) already installed"
fi

# Determine optional flags for QSV (libmfx) and NVENC
# For a shared build we avoid forcing static linking flags
ENABLE_LIBMFX=""
EXTRA_CFLAGS="-I${FFMPEG_INSTALL_PREFIX}/include"
EXTRA_LDFLAGS="-L${FFMPEG_INSTALL_PREFIX}/lib -lz -lbz2 -llzma -lwinpthread"

# libmfx (QSV): Only enable if user explicitly requests it via ENABLE_LIBMFX=1 and pkg-config can find it.
if [ "${ENABLE_LIBMFX:-0}" = "1" ]; then
    if pkg-config --exists libmfx; then
        echo "libmfx found via pkg-config; enabling QSV (libmfx)"
        ENABLE_LIBMFX="--enable-libmfx"
    else
        echo "ERROR: ENABLE_LIBMFX=1 but libmfx not found via pkg-config. Install headers/libs or unset ENABLE_LIBMFX."
        exit 1
    fi
else
    echo "libmfx not enabled; QSV support will be attempted at runtime if available via drivers/SDK (dynamic loading by FFmpeg)."
fi

# CUDA/NVENC: Auto-detect and enable when possible
# Default: disabled (avoid accidental configure failures)
NVENC_ARG="--disable-nvenc"
CUDA_FLAGS=""

# Detection strategy (in order):
# 1) pkg-config ffnvcodec (packaged headers/libs)
# 2) NVENC SDK present (nvEncodeAPI.h)
# If user explicitly sets ENABLE_NVENC=1 we require headers or pkg-config and will fail if missing.
if pkg-config --exists ffnvcodec >/dev/null 2>&1; then
    echo "ffnvcodec detected via pkg-config; enabling NVENC/ffnvcodec support"
    NVENC_ARG="--enable-nvenc"
    # Use pkg-config to get cflags/libs if available
    EXTRA_CFLAGS="${EXTRA_CFLAGS} $(pkg-config --cflags ffnvcodec 2>/dev/null || true)"
    EXTRA_LDFLAGS="${EXTRA_LDFLAGS} $(pkg-config --libs-only-L ffnvcodec 2>/dev/null || true) $(pkg-config --libs-only-l ffnvcodec 2>/dev/null || true)"
    CUDA_FLAGS="--enable-cuda --enable-cuvid --enable-nvdec --enable-ffnvcodec --enable-decoder=h264_cuvid --enable-decoder=hevc_cuvid --enable-decoder=mjpeg_cuvid"
else
    # Try NVENC SDK headers if provided by user or found in common locations
    NVENC_SDK_PATH="${NVENC_SDK_PATH:-/c/Program Files/NVIDIA Video Codec SDK}"
    if [ -f "${NVENC_SDK_PATH}/include/nvEncodeAPI.h" ]; then
        echo "NVENC SDK headers found: ${NVENC_SDK_PATH}/include"
        NVENC_ARG="--enable-nvenc"
        EXTRA_CFLAGS="${EXTRA_CFLAGS} -I${NVENC_SDK_PATH}/include"
        # If ffnvcodec pkg-config is absent we still attempt to enable CUDA decoders, but configure may fail if libraries are missing
        if pkg-config --exists ffnvcodec >/dev/null 2>&1; then
            CUDA_FLAGS="--enable-cuda --enable-cuvid --enable-nvdec --enable-ffnvcodec --enable-decoder=h264_cuvid --enable-decoder=hevc_cuvid --enable-decoder=mjpeg_cuvid"
        else
            echo "Warning: ffnvcodec pkg-config not found. Enabling CUDA/NVENC may still fail if runtime libraries are missing."
            CUDA_FLAGS="--enable-cuda --enable-cuvid --enable-nvdec --enable-decoder=h264_cuvid --enable-decoder=hevc_cuvid --enable-decoder=mjpeg_cuvid"
        fi
    else
        if [ "${ENABLE_NVENC:-0}" = "1" ]; then
            echo "ERROR: ENABLE_NVENC=1 but neither ffnvcodec pkg-config nor NVENC SDK headers found (looked at ${NVENC_SDK_PATH})."
            echo "To build with NVENC, install ffnvcodec dev package or set NVENC_SDK_PATH to the NVIDIA Video Codec SDK root containing include/nvEncodeAPI.h."
            exit 1
        else
            echo "NVENC/ CUDA not detected; build will proceed without NVENC/CUDA support (dynamic runtime detection at app level remains possible)."
            NVENC_ARG="--disable-nvenc"
            CUDA_FLAGS=""
        fi
    fi
fi

# Print detection summary for the user
echo "NVENC detection: NVENC_ARG='${NVENC_ARG}' CUDA_FLAGS='${CUDA_FLAGS}' EXTRA_CFLAGS='${EXTRA_CFLAGS}' EXTRA_LDFLAGS='${EXTRA_LDFLAGS}'"

echo ""

# Download FFmpeg source
echo "Step 4/8: Downloading FFmpeg ${FFMPEG_VERSION}..."
if [ ! -f "ffmpeg-${FFMPEG_VERSION}.tar.bz2" ]; then
    wget "${DOWNLOAD_URL}" -O "ffmpeg-${FFMPEG_VERSION}.tar.bz2"
    echo "✓ Downloaded FFmpeg source"
else
    echo "✓ FFmpeg source already downloaded"
fi

echo ""

# Extract source
echo "Step 5/8: Extracting source code..."
if [ ! -d "ffmpeg-${FFMPEG_VERSION}" ]; then
    tar -xf "ffmpeg-${FFMPEG_VERSION}.tar.bz2"
    echo "✓ Source extracted"
else
    echo "✓ Source already extracted"
fi
cd "ffmpeg-${FFMPEG_VERSION}"
echo ""

# Configure FFmpeg for shared build
echo "Step 6/8: Configuring FFmpeg for shared build..."
echo "This may take a few minutes..."
echo ""

# Set PKG_CONFIG_PATH to find libjpeg-turbo
export PKG_CONFIG_PATH="${FFMPEG_INSTALL_PREFIX}/lib/pkgconfig:${PKG_CONFIG_PATH:-}"

# Print effective options for debugging
echo "CONFIGURE OPTIONS: NVENC_ARG='${NVENC_ARG}' CUDA_FLAGS='${CUDA_FLAGS}' ENABLE_LIBMFX='${ENABLE_LIBMFX}' EXTRA_CFLAGS='${EXTRA_CFLAGS}' EXTRA_LDFLAGS='${EXTRA_LDFLAGS}'"

echo "Running: ./configure --prefix='${FFMPEG_INSTALL_PREFIX}' --enable-shared --disable-static ..."

./configure \
    --prefix="${FFMPEG_INSTALL_PREFIX}" \
    --arch=x86_64 \
    --target-os=mingw32 \
    --enable-shared \
    --disable-static \
    --enable-gpl \
    --enable-version3 \
    --enable-nonfree \
    --disable-debug \
    --disable-programs \
    --disable-doc \
    --disable-htmlpages \
    --disable-manpages \
    --disable-podpages \
    --disable-txtpages \
    --disable-outdevs \
    --enable-avcodec \
    --enable-avformat \
    --enable-avutil \
    --enable-swresample \
    --enable-swscale \
    --enable-avdevice \
    --enable-avfilter \
    --enable-postproc \
    --enable-network \
    --enable-runtime-cpudetect \
    --enable-pthreads \
    --disable-w32threads \
    --enable-zlib \
    --enable-bzlib \
    --enable-lzma \
    ${ENABLE_LIBMFX} \
    --enable-dxva2 \
    --enable-d3d11va \
    --enable-hwaccels \
    --enable-decoder=mjpeg \
    ${NVENC_ARG} ${CUDA_FLAGS} \
    --pkg-config-flags="" \
    --extra-cflags="${EXTRA_CFLAGS}" \
    --extra-ldflags="${EXTRA_LDFLAGS}"

echo "✓ Configuration complete"
echo ""

# Build FFmpeg
echo "Step 7/8: Building FFmpeg..."
echo "This will take 30-60 minutes depending on your CPU..."
echo "Using ${NUM_CORES} CPU cores for compilation"
echo ""

make -j${NUM_CORES}

echo "✓ Build complete"
echo ""

# Install FFmpeg
echo "Step 8/8: Installing FFmpeg to ${FFMPEG_INSTALL_PREFIX}..."
make install

echo "✓ Installation complete"
echo ""

# Verify installation (check for DLLs for a shared build)
echo "============================================================================"
echo "Verifying installation (shared)..."
echo "============================================================================"

if ls "${FFMPEG_INSTALL_PREFIX}/bin/"*.dll >/dev/null 2>&1 || ls "${FFMPEG_INSTALL_PREFIX}/lib/"*.dll >/dev/null 2>&1; then
    echo "✓ FFmpeg shared libraries installed successfully!"
    echo ""
    echo "Installed FFmpeg libraries (DLLs):"
    ls -lh "${FFMPEG_INSTALL_PREFIX}/bin/"*.dll 2>/dev/null || true
    ls -lh "${FFMPEG_INSTALL_PREFIX}/lib/"*.dll 2>/dev/null || true
    echo ""
    echo "Installed libjpeg-turbo libraries (DLLs):"
    ls -lh "${FFMPEG_INSTALL_PREFIX}/bin/"*jpeg*.dll 2>/dev/null || ls -lh "${FFMPEG_INSTALL_PREFIX}/lib/"*jpeg*.dll 2>/dev/null || true
    echo ""
    echo "Include directories:"
    ls -d "${FFMPEG_INSTALL_PREFIX}/include/"lib* 2>/dev/null || true
    echo ""
    echo "============================================================================"
    echo "Installation Summary"
    echo "============================================================================"
    echo "Install Path: ${FFMPEG_INSTALL_PREFIX}"
    echo "Libraries (DLLs): ${FFMPEG_INSTALL_PREFIX}/bin or ${FFMPEG_INSTALL_PREFIX}/lib"
    echo "Headers: ${FFMPEG_INSTALL_PREFIX}/include"
    echo "pkg-config: ${FFMPEG_INSTALL_PREFIX}/lib/pkgconfig"
    echo ""
    echo "Components installed (shared):"
    echo "  ✓ FFmpeg ${FFMPEG_VERSION} (shared)"
    echo "  ✓ libjpeg-turbo ${LIBJPEG_TURBO_VERSION} (shared)"
    echo ""
    echo "============================================================================"
else
    echo "✗ Installation verification failed (no DLLs found)!"
    exit 1
fi

echo ""
echo "Build completed successfully!"
echo ""
echo "To use this FFmpeg in your CMake project (shared):"
echo "  set FFMPEG_PREFIX=${FFMPEG_INSTALL_PREFIX}"
echo "  or pass -DFFMPEG_PREFIX=${FFMPEG_INSTALL_PREFIX} to cmake"
echo ""
echo "> Note: For runtime, ensure ${FFMPEG_INSTALL_PREFIX}/bin is in your PATH or bundle the DLLs with your application."

echo ""
exit 0
