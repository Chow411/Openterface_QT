#!/usr/bin/env bash
set -euo pipefail
IFS=$'\n\t'

# Wrapper script to run our existing docker-based build scripts locally
# This uses a Docker image which contains Qt 6.6.3, FFmpeg, and GStreamer so we don't need to change CMake.
# Usage:
#   ./build-script/local-docker-build.sh --appimage
#   ./build-script/local-docker-build.sh --deb
#   ./build-script/local-docker-build.sh --all

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_ROOT}/build"

# Defaults
DOCKER_IMAGE=${DOCKER_IMAGE:-ghcr.io/chow411/shared-qtbuild-complete:sha-ca1b7d7-amd64}
OPENTERFACE_BUILD_STATIC=${OPENTERFACE_BUILD_STATIC:-OFF}
USE_GSTREAMER_STATIC_PLUGINS=${USE_GSTREAMER_STATIC_PLUGINS:-OFF}

BUILD_APPIMAGE=false
BUILD_DEB=false

show_help(){
    cat <<EOF
Usage: $0 [--appimage] [--deb] [--all] [--image <docker-image>]

Options:
  --appimage   Build AppImage inside Docker and copy output to build/
  --deb        Build .deb package inside Docker and copy output to build/
  --all        Build both AppImage and .deb
  --image      Override Docker image (default: ${DOCKER_IMAGE})

Notes:
- This script mounts the repository into the container and runs the project's docker build scripts.
- The container must include Qt 6.6.3 (and FFmpeg and GStreamer) in /opt/Qt6 or system paths inside the image.
EOF
}

if [[ $# -eq 0 ]]; then
    show_help
    exit 1
fi

# parse args
while [[ $# -gt 0 ]]; do
    case $1 in
        --appimage)
            BUILD_APPIMAGE=true; shift;;
        --deb)
            BUILD_DEB=true; shift;;
        --all)
            BUILD_APPIMAGE=true; BUILD_DEB=true; shift;;
        --image)
            if [[ -z "$2" ]]; then echo "Missing docker image argument"; exit 1; fi
            DOCKER_IMAGE="$2"; shift 2;;
        -h|--help)
            show_help; exit 0;;
        *)
            echo "Unknown option: $1"; show_help; exit 1;;
    esac
done

# Quick dependency check
if ! command -v docker >/dev/null 2>&1; then
    echo "[ERROR] docker not found. Please install Docker to run this script."
    exit 1
fi

mkdir -p "$BUILD_DIR"

# Compose the Docker command. We mount source to /workspace/src and build to /workspace/build
DOCKER_RUN=(docker run --rm -v "${PROJECT_ROOT}:/workspace/src" -v "${BUILD_DIR}:/workspace/build" -e OPENTERFACE_BUILD_STATIC=${OPENTERFACE_BUILD_STATIC} -e USE_GSTREAMER_STATIC_PLUGINS=${USE_GSTREAMER_STATIC_PLUGINS} -e DOCKER=true -w /workspace/src ${DOCKER_IMAGE} bash -lc)

if [[ "$BUILD_APPIMAGE" == "true" ]]; then
    echo "Running AppImage build inside Docker image: ${DOCKER_IMAGE}"
    # Use the docker-build-appimage.sh script inside the image
    # We also pass through any other envs the docker script expects
    "${DOCKER_RUN[@]}" "bash ./build-script/docker-build-appimage.sh" || { echo "AppImage build failed"; exit 1; }
    echo "AppImage build finished. Output can be found in build/"
fi

if [[ "$BUILD_DEB" == "true" ]]; then
    echo "Running DEB build inside Docker image: ${DOCKER_IMAGE}"
    "${DOCKER_RUN[@]}" "bash ./build-script/docker-build-deb.sh" || { echo "DEB build failed"; exit 1; }
    echo "DEB build finished. Output can be found in build/"
fi

echo "Done. Files should be in ${BUILD_DIR}"