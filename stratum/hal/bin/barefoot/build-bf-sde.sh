#!/bin/bash
#
# Copyright 2020-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0
#

STRATUM_BF_DIR=$( cd $(dirname "${BASH_SOURCE[0]}") >/dev/null 2>&1 && pwd )
JOBS=${JOBS:-4}
# target-syslibs commit is based on submodule in target-utils/third-party/target-syslibs
TARGET_SYSLIBS_COMMIT=95dca9002890418614be7f76da6012b4670fb2b5
TARGET_UTILS_COMMIT=1f6fbc3387b9605ecaa4faefdc038a039e5451b2

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

    $0 -t ~/bf-sde-9.7.2.tgz
    $0 -t ~/bf-sde-9.7.2.tgz -j 4
    $0 -t ~/bf-sde-9.7.2.tgz -k ~/linux-4.14.49-ONL.tar.xz
"
}

function numeric_version() {
  # Get numeric version, for example 9.7.2 will become 90702.
  sem_ver=$1
  ver_arr=()
  IFS='.' read -raver_arr<<<"$sem_ver"
  echo $((ver_arr[0] * 10000 + ver_arr[1] * 100 + ver_arr[2]))
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

# SDE verison >= 9.7.0
pushd "$SDE/p4studio"
$sudo ./install-p4studio-dependencies.sh
./p4studio packages extract
# Patch SDE to build without kernel driver
sed -i 's/add_subdirectory(kdrv)/#add_subdirectory(kdrv)/g' $SDE/pkgsrc/bf-drivers/CMakeLists.txt
# Build BF SDE
./p4studio dependencies install --source-packages bridge,libcli,thrift --jobs $JOBS
./p4studio configure 'bfrt' 'tofino' 'asic' '^pi' '^thrift-driver' '^p4rt' '^tofino2m' '^tofino2' '^grpc' $BSP_CMD
./p4studio build --jobs $JOBS
popd

echo "BF SDE build complete."

if [[ $(numeric_version "$SDE_VERSION") -ge $(numeric_version "9.9.0") ]]; then
    echo "Build and install target-syslibs and target-utils."
    TARGET_SYSLIBS_TMP=$(mktemp -d)
    git clone https://github.com/p4lang/target-syslibs.git "$TARGET_SYSLIBS_TMP"
    pushd "$TARGET_SYSLIBS_TMP"
    git checkout "$TARGET_SYSLIBS_COMMIT"
    git submodule update --init
    mkdir -p build
    cd build
    cmake -DCMAKE_INSTALL_PREFIX="$SDE_INSTALL" ..
    make install -j "$JOBS"
    popd
    rm -rf "$TARGET_SYSLIBS_TMP"

    TARGET_UTILS_TMP=$(mktemp -d)
    git clone https://github.com/p4lang/target-utils.git "$TARGET_UTILS_TMP"
    pushd "$TARGET_UTILS_TMP"
    git checkout ${TARGET_UTILS_COMMIT}
    git submodule update --init --recursive
    mkdir -p build
    cd build
    cmake -DCMAKE_INSTALL_PREFIX="$SDE_INSTALL" -DSTANDALONE=1 ..
    make install -j "$JOBS"
    popd
    rm -rf "$TARGET_UTILS_TMP"
fi

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
