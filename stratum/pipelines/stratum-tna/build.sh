#!/usr/bin/env bash
# Copyright 2021-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

set -ex

if [[ -z "$SDE_INSTALL" ]]; then
    echo "SDE_INSTALL must be set!"
    exit 1
fi

BF_P4C="$SDE_INSTALL/bin/bf-p4c"

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
P4_SRC_DIR=${DIR}
output_dir="${DIR}/build"
p4c_flags=""
OTHER_PP_FLAGS=$2

rm -rf ${output_dir}
$BF_P4C --arch tna -g --create-graphs --verbose 2 \
      -o ${output_dir} -I ${P4_SRC_DIR} \
      ${OTHER_PP_FLAGS} \
      ${p4c_flags} \
      --p4runtime-files ${output_dir}/p4info.pb.txt \
      --p4runtime-force-std-externs \
      ${DIR}/stratum_tna.p4

# Generate the pipeline config binary.
bazel build //stratum/pipelines/stratum-tna:stratum_tna_device_config

