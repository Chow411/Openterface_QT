#!/bin/bash
# ============================================================================
# FFmpeg Static Build Script - MSYS2 MinGW64
# ============================================================================
# This script runs inside MSYS2 MinGW64 environment
# ============================================================================

set -e  # Exit on error
set -u  # Exit on undefined variable

# Configuration
FFMPEG_VERSION="6.1.1"
LIBJPEG_TURBO_VERSION="3.0.4"
FFMPEG_INSTALL_PREFIX="/c/ffmpeg-static"
BUILD_DIR="$(pwd)/ffmpeg-build-temp"
DOWNLOAD_URL="https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.bz2"
LIBJPEG_TURBO_URL="https://github.com/libjpeg-turbo/libjpeg-turbo/archive/refs/tags/${LIBJPEG_TURBO_VERSION}.tar.gz"

# Number of CPU cores for parallel compilation
NUM_CORES=$(nproc)

echo "============================================================================"
echo "FFmpeg Static Build - MSYS2 MinGW64"
echo "============================================================================"
echo "FFmpeg Version: ${FFMPEG_VERSION}"
echo "libjpeg-turbo Version: ${LIBJPEG_TURBO_VERSION}"
echo "Install Prefix: ${FFMPEG_INSTALL_PREFIX}"
echo "Build Directory: ${BUILD_DIR}"
echo "CPU Cores: ${NUM_CORES}"
echo "============================================================================"
echo ""

# Update MSYS2 and install required packages
echo "Step 1/8: Updating MSYS2 and installing dependencies..."
echo "This may take a while on first run..."

# Update package database
pacman -Sy --noconfirm

# Install build tools and dependencies
pacman -S --needed --noconfirm \
    mingw-w64-x86_64-gcc \
    mingw-w64-x86_64-binutils \
    mingw-w64-x86_64-nasm \
    mingw-w64-x86_64-yasm \
    mingw-w64-x86_64-pkgconf \
    mingw-w64-x86_64-cmake \
    mingw-w64-x86_64-ffnvcodec-headers \
    mingw-w64-x86_64-libmfx \
    make \
    diffutils \
    tar \
    bzip2 \
    wget \
    git

echo "✓ Dependencies installed"
echo ""

# Check for Intel Media SDK
echo "Checking for Intel Media SDK..."
INTEL_MEDIA_SDK_PATH="/c/Program Files (x86)/Intel/Media SDK"
if [ ! -d "$INTEL_MEDIA_SDK_PATH" ]; then
    echo "⚠ Intel Media SDK not found - QSV hardware acceleration will be limited"
    echo "  Download from: https://github.com/Intel-Media-SDK/MediaSDK/releases"
    echo "  Or install Intel Media Driver from: https://www.intel.com/content/www/us/en/download/19344/"
    echo ""
    echo "Note: For QSV to work, you'll need Intel integrated graphics or a discrete Intel GPU"
    echo "      Continuing with libmfx support (works with newer Intel drivers)..."
else
    echo "✓ Found Intel Media SDK at: $INTEL_MEDIA_SDK_PATH"
fi
echo ""

# Check for CUDA installation
echo "Checking for NVIDIA CUDA Toolkit..."
if [ -d "/c/Program Files/NVIDIA GPU Computing Toolkit/CUDA" ]; then
    CUDA_VERSION=$(ls "/c/Program Files/NVIDIA GPU Computing Toolkit/CUDA" | grep "^v" | sort -V | tail -n 1)
    if [ -n "$CUDA_VERSION" ]; then
        echo "✓ Found CUDA: $CUDA_VERSION"
        export CUDA_PATH="/c/Program Files/NVIDIA GPU Computing Toolkit/CUDA/$CUDA_VERSION"
        export PATH="$CUDA_PATH/bin:$PATH"
    else
        echo "⚠ CUDA Toolkit not found - GPU acceleration may not work"
        echo "  Download from: https://developer.nvidia.com/cuda-downloads"
    fi
else
    echo "⚠ CUDA Toolkit not found at standard location"
    echo "  Download from: https://developer.nvidia.com/cuda-downloads"
fi
echo ""

# Verify cross-compilation tools are available
echo "Verifying MinGW64 toolchain..."
which gcc
which nm
which ar
echo "✓ MinGW64 toolchain verified"
echo ""

# Create build directory
echo "Step 2/8: Creating build directory..."
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"
echo "✓ Build directory ready: ${BUILD_DIR}"
echo ""

# Download and build libjpeg-turbo
echo "Step 3/8: Building libjpeg-turbo ${LIBJPEG_TURBO_VERSION}..."
if [ ! -f "${FFMPEG_INSTALL_PREFIX}/lib/libturbojpeg.a" ]; then
    echo "Downloading libjpeg-turbo..."
    if [ ! -f "libjpeg-turbo.tar.gz" ]; then
        wget "${LIBJPEG_TURBO_URL}" -O "libjpeg-turbo.tar.gz"
    fi
    echo "Extracting libjpeg-turbo..."
    tar xzf libjpeg-turbo.tar.gz
    cd "libjpeg-turbo-${LIBJPEG_TURBO_VERSION}"
    mkdir -p build
    cd build
    echo "Configuring libjpeg-turbo..."
    cmake .. -G "MSYS Makefiles" -DCMAKE_INSTALL_PREFIX="${FFMPEG_INSTALL_PREFIX}" -DCMAKE_BUILD_TYPE=Release -DENABLE_STATIC=ON -DENABLE_SHARED=OFF -DWITH_JPEG8=ON -DWITH_TURBOJPEG=ON
    echo "Building libjpeg-turbo with ${NUM_CORES} cores..."
    make -j${NUM_CORES}
    echo "Installing libjpeg-turbo..."
    make install
    cd "${BUILD_DIR}"
    rm -rf "libjpeg-turbo-${LIBJPEG_TURBO_VERSION}"
    echo "✓ libjpeg-turbo built and installed"
else
    echo "✓ libjpeg-turbo already installed"
fi
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

# Configure FFmpeg
echo "Step 6/8: Configuring FFmpeg for static build..."
echo "This may take a few minutes..."
echo ""

# Set PKG_CONFIG_PATH to find libjpeg-turbo
export PKG_CONFIG_PATH="${FFMPEG_INSTALL_PREFIX}/lib/pkgconfig:${PKG_CONFIG_PATH:-}"

./configure \
    --prefix="${FFMPEG_INSTALL_PREFIX}" \
    --arch=x86_64 \
    --target-os=mingw32 \
    --disable-shared \
    --enable-static \
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
    --enable-dxva2 \
    --enable-d3d11va \
    --enable-hwaccels \
    --enable-decoder=mjpeg \
    --enable-cuda \
    --enable-cuvid \
    --enable-nvdec \
    --disable-nvenc \
    --enable-ffnvcodec \
    --enable-libmfx \
    --enable-decoder=h264_cuvid \
    --enable-decoder=hevc_cuvid \
    --enable-decoder=mjpeg_cuvid \
    --enable-decoder=mpeg1_cuvid \
    --enable-decoder=mpeg2_cuvid \
    --enable-decoder=mpeg4_cuvid \
    --enable-decoder=vc1_cuvid \
    --enable-decoder=vp8_cuvid \
    --enable-decoder=vp9_cuvid \
    --enable-decoder=mjpeg_qsv \
    --enable-decoder=h264_qsv \
    --enable-decoder=hevc_qsv \
    --enable-decoder=vp8_qsv \
    --enable-decoder=vp9_qsv \
    --enable-decoder=vc1_qsv \
    --enable-decoder=mpeg2_qsv \
    --enable-decoder=av1_qsv \
    --enable-decoder=vp9_cuvid \
    --pkg-config-flags="--static" \
    --extra-cflags="-I${FFMPEG_INSTALL_PREFIX}/include" \
    --extra-ldflags="-L${FFMPEG_INSTALL_PREFIX}/lib -static -static-libgcc -static-libstdc++"

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

# Verify installation
echo "============================================================================"
echo "Verifying installation..."
echo "============================================================================"

if [ -d "${FFMPEG_INSTALL_PREFIX}/include/libavcodec" ] && [ -f "${FFMPEG_INSTALL_PREFIX}/lib/libavcodec.a" ] && [ -f "${FFMPEG_INSTALL_PREFIX}/lib/libturbojpeg.a" ]; then
    echo "✓ FFmpeg and libjpeg-turbo static libraries installed successfully!"
    echo ""
    echo "Installed FFmpeg libraries:"
    ls -lh "${FFMPEG_INSTALL_PREFIX}/lib/"*.a | grep -E 'libav|libsw|libpostproc'
    echo ""
    echo "Installed libjpeg-turbo libraries:"
    ls -lh "${FFMPEG_INSTALL_PREFIX}/lib/"*jpeg*.a 2>/dev/null || true
    echo ""
    echo "Include directories:"
    ls -d "${FFMPEG_INSTALL_PREFIX}/include/"lib* 2>/dev/null || true
    echo ""
    echo "============================================================================"
    echo "Installation Summary"
    echo "============================================================================"
    echo "Install Path: ${FFMPEG_INSTALL_PREFIX}"
    echo "Libraries: ${FFMPEG_INSTALL_PREFIX}/lib"
    echo "Headers: ${FFMPEG_INSTALL_PREFIX}/include"
    echo "pkg-config: ${FFMPEG_INSTALL_PREFIX}/lib/pkgconfig"
    echo ""
    echo "Components installed:"
    echo "  ✓ FFmpeg ${FFMPEG_VERSION} (static)"
    echo "  ✓ libjpeg-turbo ${LIBJPEG_TURBO_VERSION} (static)"
    echo ""
    echo "============================================================================"
else
    echo "✗ Installation verification failed!"
    exit 1
fi

echo ""
echo "Build completed successfully!"
echo ""
echo "To use this FFmpeg in your CMake project:"
echo "  set FFMPEG_PREFIX=${FFMPEG_INSTALL_PREFIX}"
echo "  or pass -DFFMPEG_PREFIX=${FFMPEG_INSTALL_PREFIX} to cmake"

exit 0
