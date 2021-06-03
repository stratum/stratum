#!/usr/bin/env bash
# Copyright 2021-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0
set -e

if [[ $EUID -eq 0 ]]; then
  echo "This script should not be run as root, run it as the user who owns the Stratum source directory"
  exit 1
fi

# ---------- User Credentials -------------
# DOCKER_USER=<FILL IN>
# DOCKER_PASSWORD=<FILL IN>
# GITHUB_TOKEN=<FILL IN>

# ---------- Release Variables -------------
VERSION=${VERSION:-$(date +%y.%m)}  # 21.03
VERSION_LONG=${VERSION_LONG:-$(date +%Y-%m-%d)}  # 2021-03-31
STRATUM_DIR=${STRATUM_DIR:-$HOME/stratum-$(date +%Y-%m-%d-%H-%M-%SZ)}
BCM_TARGETS=(stratum_bcm_opennsa stratum_bcm_sdklt)
BF_TARGETS=(stratum_bf stratum_bfrt)
BF_SDE_VERSIONS=(9.2.0 9.3.1 9.3.2 9.4.0 9.5.0)

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
echo "$GITHUB_TOKEN" | gh auth login -h github.com --with-token

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
# This container is currently only used for the BF and BCM builds
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
# TODO(bocon): Investigate using a shared Bazel cache
set -x
docker build \
  -t opennetworking/mn-stratum:latest \
  -f tools/mininet/Dockerfile .
docker tag opennetworking/mn-stratum:latest opennetworking/mn-stratum:${VERSION}
docker push opennetworking/mn-stratum:${VERSION}
docker push opennetworking/mn-stratum:latest
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
  docker tag stratumproject/stratum-bcm:$target_short stratumproject/stratum-bcm:${VERSION}-$target_short
  docker push stratumproject/stratum-bcm:${VERSION}-$target_short
  docker push stratumproject/stratum-bcm:$target_short
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
    docker tag stratumproject/$target_dash:$sde_version stratumproject/$target_dash:${VERSION}-$sde_version
    docker push stratumproject/$target_dash:${VERSION}-$sde_version
    docker push stratumproject/$target_dash:$sde_version
    clean_up_after_build
    set +x
  done
done

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
gh auth logout -h github.com
