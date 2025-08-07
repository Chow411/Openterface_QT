#!/bin/bash

# Build script for Openterface Qt with FFmpeg camera support using qmake
# This script builds the project using qmake instead of CMake

set -e

echo "=== Openterface Qt FFmpeg Camera Build (qmake) ==="

# Check if we're in the correct directory
if [ ! -f "openterfaceQT.pro" ]; then
    echo "Error: Please run this script from the Openterface_QT root directory"
    exit 1
fi

# Check FFmpeg dependencies
echo "Checking FFmpeg dependencies..."
if ! pkg-config --exists libavformat libavcodec libavutil libswscale; then
    echo "Warning: FFmpeg development libraries not found."
    echo "FFmpeg camera support will be disabled."
    echo "To install FFmpeg libraries:"
    echo "  Ubuntu/Debian: sudo apt install libavformat-dev libavcodec-dev libavutil-dev libswscale-dev"
    echo "  Fedora/RHEL: sudo dnf install ffmpeg-devel"
    echo "  Arch: sudo pacman -S ffmpeg"
    echo ""
    echo "Continuing with Qt Multimedia backend only..."
    FFMPEG_SUPPORT="CONFIG-=ffmpeg_camera_support"
else
    echo "FFmpeg libraries found - enabling FFmpeg camera support"
    FFMPEG_SUPPORT="CONFIG+=ffmpeg_camera_support"
fi

# Check Qt installation
echo "Checking Qt installation..."
if ! command -v qmake >/dev/null 2>&1; then
    echo "Error: qmake not found. Please install Qt development packages."
    echo "  Ubuntu/Debian: sudo apt install qt6-base-dev qt6-multimedia-dev"
    echo "  Fedora/RHEL: sudo dnf install qt6-qtbase-devel qt6-qtmultimedia-devel"
    exit 1
fi

# Show Qt version
echo "Qt version: $(qmake -query QT_VERSION)"

# Create build directory
echo "Creating build directory..."
mkdir -p build-qmake
cd build-qmake

# Generate Makefile with qmake
echo "Generating Makefile with qmake..."
qmake ../openterfaceQT.pro $FFMPEG_SUPPORT || { echo "qmake failed"; exit 1; }

# Build the project
echo "Building project..."
make -j$(nproc) || { echo "Build failed"; exit 1; }

echo "=== Build completed successfully! ==="

# Check if binary was created
if [ -x "./openterfaceQT" ]; then
    echo "Binary created: $(pwd)/openterfaceQT"
    
    # Show FFmpeg support status
    if strings ./openterfaceQT | grep -q "FFMPEG_CAMERA_SUPPORT"; then
        echo "✓ FFmpeg camera support: ENABLED"
    else
        echo "✗ FFmpeg camera support: DISABLED (using Qt Multimedia only)"
    fi
    
    echo ""
    echo "To run the application:"
    echo "cd $(pwd) && ./openterfaceQT"
else
    echo "Error: Binary not found after build"
    exit 1
fi

echo ""
echo "=== Build completed! ==="
