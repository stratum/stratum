#!/bin/bash
#
# Copyright 2020-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0
#

STRATUM_BF_DIR=$( cd $(dirname "${BASH_SOURCE[0]}") >/dev/null 2>&1 && pwd )
JOBS=${JOBS:-4}

print_help() {
echo "
The script builds the BF SDE for Stratum.

Usage: $0 [<options>]

Options:

    -t, --bf-sde-tar: BF SDE tarball (Required)
    -k, --kernel-headers-tar: Linux Kernel headers tarball
    -l, --build-local-kernel-mod: Build kernel module for local kernel (Default: false)
    -j, --jobs: Number of jobs for BF SDE build (Default: 4)
    -b, --bsp-path: Path to optional BSP package directory (Default: <empty>)
    -o, --bf-sde-install-tar: BF SDE install tarball (Default: <bf sde tar name>-install.tgz)

Examples:

    $0 -t ~/bf-sde-9.3.2.tgz
    $0 -t ~/bf-sde-9.3.2.tgz -j 4
    $0 -t ~/bf-sde-9.3.2.tgz -k ~/linux-4.14.49-ONL.tar.xz
"
}

KERNEL_HEADERS_TARS=""
while (( "$#" )); do
  case "$1" in
    -t|--bf-sde-tar)
      if [ -n "$2" ] && [ ${2:0:1} != "-" ]; then
        SDE_TAR=$2
        shift 2
      else
        echo "Error: Argument for $1 is missing" >&2
        exit 1
      fi
      ;;
    -o|--bf-sde-install-tar)
      if [ -n "$2" ] && [ ${2:0:1} != "-" ]; then
        SDE_INSTALL_TAR=$2
        shift 2
      else
        echo "Error: Argument for $1 is missing" >&2
        exit 1
      fi
      ;;
    -k|--kernel-headers-tar)
      if [ -n "$2" ] && [ ${2:0:1} != "-" ]; then
        KERNEL_HEADERS_TARS="$KERNEL_HEADERS_TARS $2"
        shift 2
      else
        echo "Error: Argument for $1 is missing" >&2
        exit 1
      fi
      ;;
    -l|--build-local-kernel-mod)
      BUILD_LOCAL_KERNEL=true
      shift
      ;;
    -b|--bsp-path)
      if [ -n "$2" ] && [ ${2:0:1} != "-" ]; then
        BSP_CMD=" --bsp-path $2 "
        shift 2
      else
        echo "Error: Argument for $1 is missing" >&2
        exit 1
      fi
      ;;
    -j|--jobs)
      if [ -n "$2" ] && [ ${2:0:1} != "-" ]; then
        JOBS=$2
        shift 2
      else
        echo "Error: Argument for $1 is missing" >&2
        exit 1
      fi
      ;;
    -h|--help)
      print_help
      exit 0
      ;;
    -*|--*=) # unsupported flags
      echo "Error: Unsupported flag $1" >&2
      print_help
      exit 1
      ;;
    *) # preserve positional arguments
      echo "Error: Unknown positional argument $1" >&2
      print_help
      exit 1
      ;;
  esac
done

if [ -z "$SDE_TAR" ]; then
  echo "--bf-sde-tar is required"
  print_help
  exit 1
fi

echo "
Build variables:
  BF SDE tar: $SDE_TAR
  Kernel headers directories: ${KERNEL_HEADERS_TARS:-none}
  BSP package command: ${BSP_CMD:-none}
  Build kernel module for local kernel: ${BUILD_LOCAL_KERNEL:-false}
  Stratum scripts directory: $STRATUM_BF_DIR
  SDE build jobs: $JOBS
"

# -------------------- REAL WORK STARTS HERE --------------------
set -xe

sudo=""
# If we are not root, use "sudo"
if [[ $EUID -ne 0 ]]; then
   sudo="sudo"
fi

# Install an older version of pyresistent before running the P4 studio
# since the pip will try to install newer version of it when pip install
# the jsonschema library. And the new version of pyresistent(0.17.x) requires
# Python >= 3.5
# TODO: Remove this once we move to Python3
$sudo pip install pyrsistent==0.14.0

# Set up SDE build directory in /tmp
tmpdir="$(mktemp -d /tmp/bf_sde.XXXXXX)"
export SDE=$tmpdir
export SDE_INSTALL=$SDE/install

# Extract the SDE
tar xf $SDE_TAR -C $SDE --strip-components 1

# Get SDE version from bf-sde-[version].manifest
SDE_VERSION=$(find "$SDE" -name 'bf-sde-*.manifest' -printf '%f')
SDE_VERSION=${SDE_VERSION#bf-sde-} # Remove "bf-sde-"
SDE_VERSION=${SDE_VERSION%.manifest} # Remove ".manifest"
if [ -z "${SDE_VERSION}" ]; then
    echo "Unknown SDE version, cannot find SDE manifest file"
    exit 1
else
    echo "SDE version: ${SDE_VERSION}"
fi

# Patch stratum_profile.yaml in SDE
if [[ $SDE_VERSION == "9.7.0" ]]; then
    cp -f "$STRATUM_BF_DIR/stratum_profile_9.7.0.yaml" "$SDE/p4studio/profiles/stratum_profile.yaml"
    # Build BF SDE
    pushd "$SDE/p4studio"
    $sudo ./install-p4studio-dependencies.sh
    ./p4studio profile apply --jobs $JOBS $BSP_CMD profiles/stratum_profile.yaml
    popd
else
    cp -f "$STRATUM_BF_DIR/stratum_profile.yaml" "$SDE/p4studio_build/profiles/stratum_profile.yaml"
    # Build BF SDE
    pushd "$SDE/p4studio_build"
    ./p4studio_build.py -up stratum_profile -wk -j$JOBS -shc $BSP_CMD
    popd
fi

echo "BF SDE build complete."

# Strip shared libraries and fix permissions
find $SDE_INSTALL -name "*\.so*" -a -type f | xargs -n1 chmod u+w
find $SDE_INSTALL -name "*\.so*" -a -type f | xargs -n1 strip --strip-all
find $SDE_INSTALL -name "*\.so*" -a -type f | xargs -n1 chmod a-w

# Build BF kernel modules
if [ -n "$KERNEL_HEADERS_TARS" ]; then
  $STRATUM_BF_DIR/build-bf-kernel-mods.sh $KERNEL_HEADERS_TARS
fi
if [ -n "$BUILD_LOCAL_KERNEL" ]; then
  $STRATUM_BF_DIR/build-bf-kernel-mods.sh
fi

# Patch SDE with Stratum-required fixes
$STRATUM_BF_DIR/patch-bf-sde-install.sh

# Build the Stratum BF SDE install archive
SDE_INSTALL_TAR=${SDE_INSTALL_TAR:-"${SDE_TAR%.tgz}-install.tgz"}
tar czf $SDE_INSTALL_TAR -C $SDE_INSTALL .
rm -rf $tmpdir

set +x
echo "
BF SDE install tar: $SDE_INSTALL_TAR
"
