#!/bin/bash
#
# Copyright 2021-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0
#
set -ex

# Files in this branch that are different from main.
CHANGED_FILES=$(git diff --name-only --diff-filter=d origin/main -- 'BUILD' '*.BUILD' '*.bzl')

# List of files that are already formatted.
read -r -d '\0' KNOWN_FILES << EOF
bazel/BUILD
bazel/defs.bzl
bazel/external/bfsde.BUILD
bazel/google/BUILD
bazel/patches/BUILD
bazel/rules/BUILD
bazel/rules/build_tools.bzl
bazel/rules/lexyacc.bzl
bazel/rules/license.bzl
bazel/rules/proto_gen.bzl
bazel/rules/proto_rule.bzl
bazel/rules/test_rule.bzl
bazel/rules/yang_to_proto_rule.bzl
legal/BUILD
stratum/BUILD
stratum/glue/BUILD
stratum/glue/gtl/BUILD
stratum/glue/gtl/BUILD
stratum/glue/net_util/BUILD
stratum/glue/net_util/BUILD
stratum/glue/status/BUILD
stratum/glue/status/BUILD
stratum/hal/bin/barefoot/BUILD
stratum/hal/bin/barefoot/BUILD
stratum/hal/bin/bcm/sim/BUILD
stratum/hal/bin/bcm/sim/BUILD
stratum/hal/bin/bcm/standalone/BUILD
stratum/hal/bin/bcm/standalone/BUILD
stratum/hal/bin/bmv2/bmv2.bzl
stratum/hal/bin/np4intel/BUILD
stratum/hal/bin/np4intel/np4intel.bzl
stratum/hal/config/platform_config_test.bzl
stratum/hal/lib/barefoot/BUILD
stratum/hal/lib/dummy/BUILD
stratum/hal/lib/np4intel/BUILD
stratum/hal/lib/phal/BUILD
stratum/hal/lib/phal/onlp/onlp.bzl
stratum/hal/lib/phal/tai/BUILD
stratum/hal/lib/phal/test/BUILD
stratum/hal/lib/pi/BUILD
stratum/hal/stub/embedded/BUILD
stratum/lib/BUILD
stratum/lib/channel/BUILD
stratum/lib/security/BUILD
stratum/lib/test_utils/BUILD
stratum/p4c_backends/common/BUILD
stratum/p4c_backends/common/build_defs.bzl
stratum/p4c_backends/fpm/bcm/BUILD
stratum/p4c_backends/ir/BUILD
stratum/p4c_backends/test/BUILD
stratum/p4c_backends/test/build_defs.bzl
stratum/pipelines/loopback/BUILD
stratum/pipelines/main/BUILD
stratum/pipelines/ptf/BUILD
stratum/pipelines/ptf/ptf_exec.bzl
stratum/pipelines/ptf/scapy_exec.bzl
stratum/portage/BUILD
stratum/portage/build_defs.bzl
stratum/procmon/BUILD
stratum/public/lib/BUILD
stratum/public/model/BUILD
stratum/public/proto/BUILD
stratum/public/proto/proto2/BUILD
stratum/testing/cdlang/BUILD
stratum/testing/protos/BUILD
stratum/testing/scenarios/BUILD
stratum/testing/tests/BUILD
stratum/tools/gnmi/BUILD
stratum/tools/stratum_replay/BUILD
\0
EOF

echo -e "$KNOWN_FILES\n$CHANGED_FILES" | sort -u | xargs -t buildifier -lint=fix -mode=fix

# Report which files need to be fixed.
git update-index --refresh
