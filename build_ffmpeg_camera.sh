#!/bin/bash

# FFmpeg Camera Implementation Build and Test Script
# This script helps build and test the new FFmpeg camera implementation

set -e

echo "=== FFmpeg Camera Implementation Build Script ==="

# Check if we're in the correct directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "Error: Please run this script from the Openterface_QT root directory"
    exit 1
fi

# Check FFmpeg dependencies
echo "Checking FFmpeg dependencies..."
if ! pkg-config --exists libavformat libavcodec libavutil libswscale; then
    echo "Error: FFmpeg development libraries not found. Please install:"
    echo "  Ubuntu/Debian: sudo apt install libavformat-dev libavcodec-dev libavutil-dev libswscale-dev"
    echo "  Fedora/RHEL: sudo dnf install ffmpeg-devel"
    echo "  Arch: sudo pacman -S ffmpeg"
    exit 1
fi

# Check V4L2 support
echo "Checking V4L2 support..."
if [ ! -d "/sys/class/video4linux" ]; then
    echo "Warning: No V4L2 devices found in /sys/class/video4linux"
    echo "Make sure your camera is connected and V4L2 drivers are loaded"
fi

# List available video devices
echo "Available video devices:"
ls -la /dev/video* 2>/dev/null || echo "No /dev/video* devices found"

# Create build directory
echo "Creating build directory..."
mkdir -p build
cd build

# Configure with CMake
echo "Configuring with CMake..."
cmake .. \
    -DCMAKE_BUILD_TYPE=Debug \
    -DUSE_FFMPEG_STATIC=OFF \
    || { echo "CMake configuration failed"; exit 1; }

# Build the project
echo "Building project..."
make -j$(nproc) || { echo "Build failed"; exit 1; }

echo "=== Build completed successfully! ==="

# Test camera devices
echo "Testing camera device detection..."
if [ -x "./openterfaceQT" ]; then
    echo "Binary built successfully: ./openterfaceQT"
    
    # Check for camera devices that might be Openterface
    echo "Checking for potential Openterface devices..."
    for device in /dev/video*; do
        if [ -c "$device" ]; then
            echo "Testing device: $device"
            # Use v4l2-ctl if available to get device info
            if command -v v4l2-ctl >/dev/null 2>&1; then
                v4l2-ctl --device="$device" --info 2>/dev/null || true
            fi
        fi
    done
else
    echo "Error: Binary not found after build"
    exit 1
fi

echo ""
echo "=== Next Steps ==="
echo "1. Connect your Openterface device"
echo "2. Run: ./openterfaceQT"
echo "3. The application should automatically detect and use FFmpeg backend"
echo "4. Check the debug output for FFmpeg initialization messages"
echo ""
echo "=== Debug Information ==="
echo "To enable detailed FFmpeg logging, set environment variable:"
echo "export QT_LOGGING_RULES='opf.ffmpeg.*=true'"
echo ""
echo "To manually test camera detection:"
echo "ffmpeg -f v4l2 -list_formats all -i /dev/video0"
