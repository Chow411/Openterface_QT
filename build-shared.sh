#!/bin/bash
# Openterface QT Shared Build Script
# Uses Docker image with static Qt6 build

#!/bin/bash
DOCKER_BUILDENV_IMAGE=ghcr.io/techxartisanstudio/static-qtbuild-complete
sudo rm -rf $(pwd)/build
echo "Start to build by docker now.."
echo "Build envioronment image: $DOCKER_BUILDENV_IMAGE"

export OPENTERFACE_BUILD_STATIC=OFF
docker run --rm \
  --env OPENTERFACE_BUILD_STATIC \
  -v "$(pwd)":/workspace/src \
  -v "$(pwd)/build":/workspace/build \
  # -v "$(pwd)/build-script/docker-translation.sh":/workspace/docker-translation.sh \
  -w /workspace/src \
  "$DOCKER_BUILDENV_IMAGE" \
  bash /workspace/docker-translation.sh
  
docker run --rm --network host \
	--env OPENTERFACE_BUILD_STATIC=OFF \
    --env USE_GSTREAMER_STATIC_PLUGINS=OFF \
    --env http_proxy=http://127.0.0.1:8118 \
    --env https_proxy=http://127.0.0.1:8118 \
    -v "$(pwd)":/workspace/src \
    -v "$(pwd)/build":/workspace/build \
    -v "$(pwd)/build-script/docker-build-shared.sh":/workspace/docker-build-shared.sh \
    -w /workspace/src \
  	"$DOCKER_BUILDENV_IMAGE" \
 	bash /workspace/docker-build-shared.sh

