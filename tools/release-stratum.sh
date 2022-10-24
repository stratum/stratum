#!/usr/bin/env bash
# Copyright 2021-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0
set -e

if [[ $EUID -eq 0 ]]; then
  echo "This script should not be run as root, run it as the user who owns the Stratum source directory"
  exit 1
fi

function numeric_version() {
  # Get numeric version, for example 9.7.2 will become 90502.
  sem_ver=$1
  ver_arr=()
  IFS='.' read -raver_arr<<<"$sem_ver"
  echo $((ver_arr[0] * 10000 + ver_arr[1] * 100 + ver_arr[2]))
}

# ---------- User Credentials -------------
# DOCKER_USER=<FILL IN>
# DOCKER_PASSWORD=<FILL IN>
# GITHUB_TOKEN=<FILL IN>

# ---------- Release Variables -------------
VERSION=${VERSION:-$(date +%y.%m)}  # 21.03
VERSION_LONG=${VERSION_LONG:-$(date +%Y-%m-%d)}  # 2021-03-31
STRATUM_DIR=${STRATUM_DIR:-$HOME/stratum-$(date +%Y-%m-%d-%H-%M-%SZ)}
BCM_TARGETS=(stratum_bcm_opennsa stratum_bcm_sdklt)
BF_TARGETS=(stratum_bfrt)
BF_SDE_VERSIONS=(9.7.0 9.7.1 9.7.2 9.8.0 9.9.0 9.10.0)

# ---------- Build Variables -------------
JOBS=30
BAZEL_CACHE=$HOME/.cache
BF_SDE_INSTALL_TAR_PATH=$HOME/sde
RELEASE_DIR=$HOME/stratum-release-pkgs

# Clean up and recreate the release package directory
rm -rfv $RELEASE_DIR
mkdir -p $RELEASE_DIR

echo "
Building Stratum release $VERSION ($VERSION_LONG)

Stratum directory: $STRATUM_DIR
BCM Targets: ${BCM_TARGETS[*]}
BF Targets: ${BF_TARGETS[*]}
BF SDE Versions: ${BF_SDE_VERSIONS[*]}
BF SDE install tarball path: $BF_SDE_INSTALL_TAR_PATH
Release artifact directory: $RELEASE_DIR
Bazel cache directory: $BAZEL_CACHE
Jobs: $JOBS
"

# ---------- Prerequisites -------------

# Log in to Docker Hub
echo "$DOCKER_PASSWORD" | docker login -u "$DOCKER_USER" --password-stdin

# Log in to Github CLI
if ! which gh >/dev/null; then
  echo "Could not find GitHub CLI; installing..."
  wget https://github.com/cli/cli/releases/download/v1.4.0/gh_1.4.0_linux_amd64.deb
  sudo apt install ./gh_1.4.0_linux_amd64.deb
  rm gh_1.4.0_linux_amd64.deb
fi

# gh will use GITHUB_TOKEN to login automatically
gh auth status

# Verify that all BF SDE install packages exist
missing=0
for sde_version in ${BF_SDE_VERSIONS[@]}; do
  file=$BF_SDE_INSTALL_TAR_PATH/bf-sde-$sde_version-install.tgz
  if [ ! -f $file ]; then
    echo "Missing $file"
    missing+=1
  fi
done
if [ $missing -gt 0 ]; then
  echo "ERROR: Missing BF SDE install packages." \
    "Build the missing files and try again."
  exit 1
fi

# ---------- git tag the release -------------
if [ ! -d $STRATUM_DIR ]; then
  git clone https://github.com/stratum/stratum.git $STRATUM_DIR
  TEMP_STRATUM_DIR=1
fi
cd $STRATUM_DIR
git tag $VERSION_LONG

# ---------- Build release builder container -------------
set -x
docker build \
  -t stratumproject/build:build \
  -f Dockerfile.build .
docker tag stratumproject/build:build stratumproject/build:${VERSION}
docker push stratumproject/build:build
docker push stratumproject/build:${VERSION}

IMAGE_NAME=stratum-release-builder
eval docker build \
  -t $IMAGE_NAME \
  --build-arg USER_NAME="$USER" --build-arg USER_ID="$UID" \
  - < Dockerfile.dev
set +x

# Remove debs and docker.tar.gz files after build
function clean_up_after_build() {
  set +x
  local suffix="deb$\|docker\.tar\.gz"
  local files
  # Untracked files
  files+=$(git ls-files . --exclude-standard --others | grep $suffix || echo "")
  # Ignored files
  files+=$(git ls-files . --exclude-standard --others --ignored | grep $suffix || echo "")
  set -x
  rm -f $files
}

# ---------- Build: BMv2 -------------
set -x
RELEASE_BUILD=true \
  JOBS=${JOBS} \
  BAZEL_CACHE=${BAZEL_CACHE} \
  DOCKER_IMG=${IMAGE_NAME} \
  tools/mininet/build-stratum-bmv2-container.sh
docker tag opennetworking/mn-stratum:latest opennetworking/mn-stratum:${VERSION}
docker push opennetworking/mn-stratum:${VERSION}
docker push opennetworking/mn-stratum:latest
mv -f ./stratum_bmv2_deb.deb $RELEASE_DIR/stratum-bmv2-${VERSION}-amd64.deb
set +x

# ---------- Build: Broadcom -------------
for target in ${BCM_TARGETS[@]}; do
  target_short=${target##*_}
  echo "Building $target ($target_short)..."
  set -x
  RELEASE_BUILD=true \
    STRATUM_TARGET=$target \
    JOBS=${JOBS} \
    BAZEL_CACHE=${BAZEL_CACHE} \
    DOCKER_IMG=${IMAGE_NAME} \
    stratum/hal/bin/bcm/standalone/docker/build-stratum-bcm-container.sh
  mv -f stratum_bcm_${target_short}_deb.deb $RELEASE_DIR/stratum-bcm-${VERSION}-$target_short-amd64.deb
  docker tag stratumproject/stratum-bcm_$target_short:latest stratumproject/stratum-bcm:${VERSION}-$target_short
  docker tag stratumproject/stratum-bcm_$target_short:latest stratumproject/stratum-bcm:latest-$target_short
  docker tag stratumproject/stratum-bcm_$target_short:latest stratumproject/stratum-bcm:$target_short
  docker push stratumproject/stratum-bcm:${VERSION}-$target_short
  docker push stratumproject/stratum-bcm:latest-$target_short
  clean_up_after_build
  set +x
done

# ---------- Build: Tofino -------------
for sde_version in ${BF_SDE_VERSIONS[@]}; do
  for target in ${BF_TARGETS[@]}; do
    target_dash=${target/_/-}
    echo "Building $target ($target_dash) with BF SDE $sde_version..."
    set -x
    RELEASE_BUILD=true \
      STRATUM_TARGET=$target \
      JOBS=${JOBS} \
      BAZEL_CACHE=${BAZEL_CACHE} \
      DOCKER_IMG=${IMAGE_NAME} \
      SDE_INSTALL_TAR=$BF_SDE_INSTALL_TAR_PATH/bf-sde-$sde_version-install.tgz \
      stratum/hal/bin/barefoot/docker/build-stratum-bf-container.sh
    mv -f ${target}_deb.deb $RELEASE_DIR/$target_dash-${VERSION}-$sde_version-amd64.deb
    docker tag stratumproject/$target_dash:latest-$sde_version stratumproject/$target_dash:${VERSION}-$sde_version
    docker push stratumproject/$target_dash:${VERSION}-$sde_version
    docker push stratumproject/$target_dash:latest-$sde_version
    clean_up_after_build
    set +x
  done
done

# ---------- Build p4c-fpm and stratum-tools -------------

DOCKER_EXTRA_RUN_OPTS=""
if [ -t 0 ]; then
  # Running in a TTY, so run interactively (i.e. make Ctrl-C work)
  DOCKER_EXTRA_RUN_OPTS+="-it "
fi

# Set build options for Stratum build
DOCKER_OPTS=""

# Build optimized and stripped binaries
BAZEL_OPTS="--config release "

# Build with Bazel cache
if [ -n "$BAZEL_CACHE" ]; then
  DOCKER_OPTS+="-v $BAZEL_CACHE:/home/$USER/.cache "
  DOCKER_OPTS+="--user $USER "
fi

set -x
# Build p4c-fpm in Docker
docker run --rm \
  $DOCKER_OPTS \
  $DOCKER_EXTRA_RUN_OPTS \
  -v $STRATUM_DIR:/stratum \
  -v $RELEASE_DIR:/output \
  -w /stratum \
  --entrypoint bash \
  $IMAGE_NAME -c \
    "bazel build //stratum/p4c_backends/fpm:p4c_fpm_deb \
       $BAZEL_OPTS \
       --jobs $JOBS && \
     cp -f /stratum/bazel-bin/stratum/p4c_backends/fpm/p4c_fpm_deb.deb /output/"

# Build stratum-tools in Docker
docker run --rm \
  $DOCKER_OPTS \
  $DOCKER_EXTRA_RUN_OPTS \
  -v $STRATUM_DIR:/stratum \
  -v $RELEASE_DIR:/output \
  -w /stratum \
  --entrypoint bash \
  $IMAGE_NAME -c \
    "bazel build //stratum/tools:stratum_tools_deb \
       $BAZEL_OPTS \
       --jobs $JOBS && \
     cp -f /stratum/bazel-bin/stratum/tools/stratum_tools_deb.deb /output/"
set +x

# Compute labels for Docker containers
DOCKER_BUILD_OPTS=""
if [ "$(docker version -f '{{.Server.Experimental}}')" = "true" ]; then
  DOCKER_BUILD_OPTS+="--squash "
fi
DOCKER_BUILD_OPTS+="--label build-timestamp=$(date +%FT%T%z) "
DOCKER_BUILD_OPTS+="--label build-machine=$(hostname) "

GIT_URL=${GIT_URL:-$(git config --get remote.origin.url)}
GIT_REF=$(git describe --tags --no-match --always --abbrev=40 --dirty | sed -E 's/^.*-g([0-9a-f]{40}-?.*)$/\1/')
GIT_SHA=$(git describe --tags --match XXXXXXX --always --abbrev=40 --dirty)
DOCKER_BUILD_OPTS+="--label org.opencontainers.image.source=$GIT_URL "
DOCKER_BUILD_OPTS+="--label org.opencontainers.image.version=$GIT_REF "
DOCKER_BUILD_OPTS+="--label org.opencontainers.image.revision=$GIT_SHA "

set -x
# Build stratum-tools Docker image
docker build \
  -f stratum/tools/Dockerfile.stratum_tools \
  -t stratumproject/stratum-tools \
  $DOCKER_BUILD_OPTS \
  --label stratum-target=stratum-tools \
  $RELEASE_DIR
docker tag stratumproject/stratum-tools stratumproject/stratum-tools:${VERSION}
docker push stratumproject/stratum-tools:${VERSION}
docker push stratumproject/stratum-tools

# Build p4c-fpm Docker image
docker build \
  -f stratum/p4c_backends/fpm/Dockerfile \
  -t stratumproject/p4c-fpm \
  $DOCKER_BUILD_OPTS \
  $RELEASE_DIR
docker tag stratumproject/p4c-fpm stratumproject/p4c-fpm:${VERSION}
docker push stratumproject/p4c-fpm:${VERSION}
docker push stratumproject/p4c-fpm
set +x

# Rename p4c-fpm and stratum-tools packages
pushd $RELEASE_DIR
mv -f p4c_fpm_deb.deb p4c-fpm-${VERSION}-amd64.deb
mv -f stratum_tools_deb.deb stratum-tools-${VERSION}-amd64.deb
popd

# ---------- Push tag to Github -------------
set -x
if [ -n "$TEMP_STRATUM_DIR" ]; then
  git config url."https://$GITHUB_TOKEN@github.com/".insteadOf "https://github.com/"
fi
git push origin ${VERSION_LONG}
set +x

# ---------- Upload release artifacts to Github -------------
# Generate change list
PREV_TAG=$(git describe --tags --abbrev=0 HEAD~1)
echo "## Changelist" > $STRATUM_DIR/release-notes.txt
git log --oneline --no-decorate $PREV_TAG..HEAD >> $STRATUM_DIR/release-notes.txt
cd $RELEASE_DIR/
# Print release artifacts (debian packages)
ls -lh
set -x
# Create release and upload artifacts
gh release create -R stratum/stratum ${VERSION_LONG} \
  -d -t ${VERSION_LONG} -F $STRATUM_DIR/release-notes.txt
gh release upload -R stratum/stratum ${VERSION_LONG} *
rm $STRATUM_DIR/release-notes.txt

# ---------- Misc -------------
gh issue create -R stratum/stratum \
  --title "Update dependency versions" \
  --label "Infra" \
  --body "Now that Stratum **${VERSION}** is released, update Stratum upstream dependencies to the latest version."

# ---------- Cleanup -------------
docker logout
