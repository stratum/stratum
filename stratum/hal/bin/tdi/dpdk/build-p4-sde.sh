#!/bin/bash
# Copyright 2022-present Intel Corporation
# SPDX-License-Identifier: Apache-2.0
set -ex

THIS_DIR=$( cd $(dirname "${BASH_SOURCE[0]}") >/dev/null 2>&1 && pwd )
STRATUM_ROOT=${STRATUM_ROOT:-"$( cd "$THIS_DIR/../../../../.." >/dev/null 2>&1 && pwd )"}
JOBS=${JOBS:-4}

print_help() {
echo "
The script builds the P4 DPDK backend and creates a tar archive for later
consumption in Stratum.
Usage: $0

Example:
    $0
    JOBS=16 $0

Additional environment variables:
    JOBS: The number of jobs to run simultaneously while building the base container. (Default: 4)
    STRATUM_ROOT: The root directory of Stratum.
"
}

if [ "$EUID" -ne 0 ]
  then echo "Please run as root"
  exit
fi

# Dependency setup
echo "wireshark-common wireshark-common/install-setuid boolean true" | debconf-set-selections
apt update
DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    wireshark tshark htop vim git sudo python3-pip wget python2 \
    autoconf-archive libffi-dev ninja-build
ln -s -f /usr/bin/python2 /usr/bin/python
pip3 install distro meson

# Build the P4 TDI SDE
tmpdir="$(mktemp -d /tmp/p4_sde.XXXXXX)"
pushd $tmpdir
# TODO: needs commit hash pinning
git clone --depth=1 --recursive https://github.com/p4lang/target-utils utils
git clone --depth=1 --recursive https://github.com/p4lang/target-syslibs syslibs
git clone --depth=1 --recursive https://github.com/p4lang/p4-dpdk-target p4-dpdk-target
git clone --depth=1 --recursive https://github.com/ipdk-io/networking-recipe ipdk-dpdk

source $tmpdir/p4-dpdk-target/tools/setup/p4sde_env_setup.sh $tmpdir
python3 $tmpdir/p4-dpdk-target/tools/setup/install_dep.py

cd $SDE/p4-dpdk-target
git submodule update --init --recursive --force
git apply --ignore-whitespace "$THIS_DIR"/patch/01-enable-generic-isa.patch
./autogen.sh
autoreconf
./configure --prefix=$SDE_INSTALL
make -j$JOBS
make install

# Build the Stratum BF SDE install archive
SDE_INSTALL_TAR=${SDE_INSTALL_TAR:-"p4-sde-install.tgz"}
tar czf $STRATUM_ROOT/$SDE_INSTALL_TAR -C $SDE_INSTALL .
rm -rf $tmpdir
popd

set +x
echo "
P4 SDE install tar: $SDE_INSTALL_TAR
"
