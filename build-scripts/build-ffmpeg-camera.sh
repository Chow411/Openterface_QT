#!/bin/bash

# Build script for Openterface QT with FFmpeg Camera Manager
# This script helps build and test the new FFmpeg-based camera implementation

set -e

echo "=== Openterface QT FFmpeg Camera Manager Build Script ==="

# Check if we're on Linux
if [[ "$OSTYPE" != "linux-gnu"* ]]; then
    echo "Warning: FFmpeg camera manager is primarily designed for Linux"
    echo "Continuing with build anyway..."
fi

# Check for required dependencies
echo "Checking dependencies..."

# Check for Qt6
if ! command -v qmake6 &> /dev/null && ! command -v qmake &> /dev/null; then
    echo "Error: Qt6 development packages not found"
    echo "Please install Qt6 development packages:"
    echo "  Ubuntu/Debian: sudo apt install qt6-base-dev qt6-multimedia-dev"
    echo "  Fedora: sudo dnf install qt6-qtbase-devel qt6-qtmultimedia-devel"
    echo "  Arch: sudo pacman -S qt6-base qt6-multimedia"
    exit 1
fi

# Check for FFmpeg development packages
if ! pkg-config --exists libavformat libavcodec libavutil libswscale; then
    echo "Error: FFmpeg development packages not found"
    echo "Please install FFmpeg development packages:"
    echo "  Ubuntu/Debian: sudo apt install libavformat-dev libavcodec-dev libavutil-dev libswscale-dev"
    echo "  Fedora: sudo dnf install ffmpeg-devel"
    echo "  Arch: sudo pacman -S ffmpeg"
    exit 1
fi

# Check for V4L2 headers
if [[ ! -f /usr/include/linux/videodev2.h ]]; then
    echo "Error: V4L2 headers not found"
    echo "Please install kernel headers:"
    echo "  Ubuntu/Debian: sudo apt install linux-libc-dev"
    echo "  Fedora: sudo dnf install kernel-headers"
    echo "  Arch: sudo pacman -S linux-headers"
    exit 1
fi

echo "All dependencies found!"

# Create build directory
BUILD_DIR="build-ffmpeg"
if [[ -d "$BUILD_DIR" ]]; then
    echo "Cleaning existing build directory..."
    rm -rf "$BUILD_DIR"
fi

mkdir "$BUILD_DIR"
cd "$BUILD_DIR"

echo "Configuring build with CMake..."

# Configure with CMake
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DUSE_FFMPEG_STATIC=OFF \
    -DCMAKE_PREFIX_PATH="$(qmake6 -query QT_INSTALL_PREFIX 2>/dev/null || qmake -query QT_INSTALL_PREFIX)" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

echo "Building project..."
make -j$(nproc)

echo "Build completed successfully!"

# Check if we can find video devices
echo ""
echo "=== System Video Device Check ==="
if ls /dev/video* >/dev/null 2>&1; then
    echo "Found video devices:"
    ls -la /dev/video*
    
    echo ""
    echo "Device capabilities:"
    for device in /dev/video*; do
        echo "--- $device ---"
        if command -v v4l2-ctl &> /dev/null; then
            v4l2-ctl --device="$device" --info 2>/dev/null || echo "Could not query device info"
        else
            echo "v4l2-ctl not available (install v4l-utils for detailed info)"
        fi
    done
else
    echo "No video devices found in /dev/"
    echo "Make sure your camera/capture card is connected"
fi

echo ""
echo "=== Build Summary ==="
echo "Executable: $PWD/openterfaceQT"
echo "FFmpeg camera manager: Enabled"
echo "Threading: Multi-threaded decoding enabled"
echo "V4L2 support: Enabled"

echo ""
echo "=== Usage Instructions ==="
echo "1. Connect your Openterface device"
echo "2. Run: ./openterfaceQT"
echo "3. The application will automatically detect and use the FFmpeg backend on Linux"
echo "4. Check the console output for FFmpeg camera manager log messages"

echo ""
echo "=== Troubleshooting ==="
echo "- If camera doesn't start, check dmesg for USB/V4L2 errors"
echo "- Ensure your user has permission to access /dev/video* devices"
echo "- Add user to 'video' group: sudo usermod -a -G video \$USER"
echo "- Log out and back in after adding to video group"

echo ""
echo "=== Configuration ==="
echo "To force use of specific camera backend, add to ~/.config/Techxartisan/Openterface.conf:"
echo "[camera]"
echo "backend=ffmpeg    # Force FFmpeg backend"
echo "backend=qt        # Force Qt backend"
echo "backend=auto      # Auto-select (default)"

echo ""
echo "Build script completed successfully!"
