#!/bin/bash

set -euo pipefail
IFS=$'\n\t'

# Local build script for AppImage and DEB packages using Docker
# This script builds Openterface QT using Docker containers with proper Qt6, FFmpeg, and GStreamer versions

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Docker image with Qt6, FFmpeg, and GStreamer
DOCKER_IMAGE="ghcr.io/chow411/shared-qtbuild-complete:sha-ca1b7d7-amd64"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if Docker is available
check_docker() {
    if ! command -v docker >/dev/null 2>&1; then
        log_error "Docker is not installed or not in PATH"
        log_error "Please install Docker first"
        exit 1
    fi

    if ! docker info >/dev/null 2>&1; then
        log_error "Docker daemon is not running"
        log_error "Please start Docker service"
        exit 1
    fi

    log_success "Docker is available"
}

# Build the application using Docker
build_application_docker() {
    log_info "Building Openterface QT application using Docker..."

    docker run --rm \
        -v "$(pwd):/workspace/src" \
        -v "$(pwd)/build:/workspace/build" \
        -e OPENTERFACE_BUILD_STATIC=OFF \
        -e USE_GSTREAMER_STATIC_PLUGINS=OFF \
        "$DOCKER_IMAGE" \
        bash -lc '
            cd /workspace/build && \
            cmake -DCMAKE_BUILD_TYPE=Release \
                  -DOPENTERFACE_BUILD_STATIC=${OPENTERFACE_BUILD_STATIC} \
                  -DUSE_GSTREAMER_STATIC_PLUGINS=${USE_GSTREAMER_STATIC_PLUGINS} \
                  -DCMAKE_PREFIX_PATH="/opt/Qt6" \
                  -DQt6_DIR="/opt/Qt6/lib/cmake/Qt6" \
                  /workspace/src && \
            make -j4
        '

    log_success "Application built successfully"
}

# Build DEB package using Docker
build_deb_docker() {
    log_info "Building DEB package using Docker..."

    docker run --rm \
        -v "$(pwd):/workspace/src" \
        -v "$(pwd)/build:/workspace/build" \
        -e OPENTERFACE_BUILD_STATIC=OFF \
        -e USE_GSTREAMER_STATIC_PLUGINS=OFF \
        "$DOCKER_IMAGE" \
        bash -lc 'cd /workspace/src && bash ./build-script/docker-build-deb.sh'

    log_success "DEB package built successfully"
}

# Build AppImage package using Docker
build_appimage_docker() {
    log_info "Building AppImage using Docker..."

    docker run --rm \
        -v "$(pwd):/workspace/src" \
        -v "$(pwd)/build:/workspace/build" \
        -e OPENTERFACE_BUILD_STATIC=OFF \
        -e USE_GSTREAMER_STATIC_PLUGINS=OFF \
        "$DOCKER_IMAGE" \
        bash -c 'cd /workspace/src && ./build-script/docker-build-appimage.sh'

    log_success "AppImage created successfully"
}

# Main function
main() {
    # Parse arguments
    BUILD_APPIMAGE=false
    BUILD_DEB=false

    while [[ $# -gt 0 ]]; do
        case $1 in
            --appimage)
                BUILD_APPIMAGE=true
                shift
                ;;
            --deb)
                BUILD_DEB=true
                shift
                ;;
            --all)
                BUILD_APPIMAGE=true
                BUILD_DEB=true
                shift
                ;;
            --image)
                if [[ -z "$2" ]]; then log_error "--image requires an argument"; exit 1; fi
                DOCKER_IMAGE="$2"
                shift 2
                ;;
            *)
                log_error "Unknown option: $1"
                echo "Usage: $0 [--appimage] [--deb] [--all]"
                echo ""
                echo "This script uses Docker to build packages with proper Qt6, FFmpeg, and GStreamer versions."
                echo "Make sure Docker is installed and running."
                exit 1
                ;;
        esac
    done

    if [[ "$BUILD_APPIMAGE" == "false" && "$BUILD_DEB" == "false" ]]; then
        log_error "Please specify --appimage, --deb, or --all"
        echo "Usage: $0 [--appimage] [--deb] [--all]"
        exit 1
    fi

    log_info "Starting Docker-based build..."
    log_info "Build AppImage: $BUILD_APPIMAGE"
    log_info "Build DEB: $BUILD_DEB"
    log_info "Using Docker image: $DOCKER_IMAGE"

    check_docker

    # Clean previous builds
    rm -rf "$PROJECT_ROOT/build"

    # Build the application first
    build_application_docker

    if [[ "$BUILD_APPIMAGE" == "true" ]]; then
        build_appimage_docker
    fi

    if [[ "$BUILD_DEB" == "true" ]]; then
        build_deb_docker
    fi

    log_success "Build completed successfully!"

    # Show results
    echo ""
    log_info "Build results:"
    if [[ "$BUILD_APPIMAGE" == "true" ]]; then
        echo "  AppImage: $(ls -la "$PROJECT_ROOT/build/"*.AppImage 2>/dev/null || echo "Not found")"
    fi
    if [[ "$BUILD_DEB" == "true" ]]; then
        echo "  DEB: $(ls -la "$PROJECT_ROOT/build/"*.deb 2>/dev/null || echo "Not found")"
    fi
}

main "$@"