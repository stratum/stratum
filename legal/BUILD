# Copyright 2021-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

# The Stratum source code is licensed using the Apache 2.0 license. You can see
# a full copy of this license in the LICENSE file in the root of this
# repository. If you are including Stratum code in a derivative work, you should
# include the text from the NOTICE file in the root directory of this repository
# in your NOTICE file when distributing your derivative work.
#
# This directory contains NOTICE files and additional license terms that apply
# to binary distributions of Stratum. Generally, the Apache 2.0 license applies
# to Stratum binaries, but may be superseded by vendor-specific licenses or
# terms in specific Stratum packages. You should read the LICENSE and NOTICE
# file present in the Stratum binary distributions if you plan to use or
# restribute them.
#
# -----------------------------------------------------------------------------
#
# The NOTICE files were generated manually after using Bazel to identify dependencies.
#
# NOTICE.common.txt contains the baseline Stratum dependencies
#
#   bazel cquery "filter(@, kind(cc_.*, deps(//stratum/hal/bin/dummy:stratum_dummy) except deps(@local_config_cc//:*)))" | cut -d/ -f1 | sort -u
#   bazel cquery "filter(@, kind(cc_.*, deps(//stratum/tools:stratum_tools_deb) except deps(@local_config_cc//:*)))" | cut -d/ -f1 | sort -u
#
#   @boringssl
#   @com_github_cares_cares
#   @com_github_gflags_gflags
#   @com_github_google_glog
#   @com_github_google_re2
#   @com_github_grpc_grpc
#   @com_github_openconfig_gnmi_proto
#   @com_github_openconfig_gnoi
#   @com_github_openconfig_hercules
#   @com_github_p4lang_p4runtime
#   @com_google_absl
#   @com_google_googleapis
#   @com_google_googletest
#   @com_google_protobuf
#   @local_onlp_bin
#   @upb
#   @zlib
#
# NOTICE.pi.txt contains the dependies for PI and judy
# NOTICE.bmv2.txt contains the dependencies for BMv2
#
#   bazel cquery "filter(@, kind(cc_.*, deps(//stratum/hal/bin/bmv2:stratum_bmv2) except deps(//stratum/hal/bin/dummy:stratum_dummy)))" | cut -d/ -f1 | sort -u
#
#   @com_github_p4lang_PI
#   @judy
#   @local_bmv2_bin
#
# NOTICE.bmv2.txt contains the dependencies for nlohmann/json
# NOTICE.intel-sde.txt contains the dependencies for the Intel SDE
#
#   bazel cquery "filter(@, kind(cc_.*, deps(//stratum/hal/bin/barefoot:stratum_bfrt) except deps(//stratum/hal/bin/dummy:stratum_dummy)))" | cut -d/ -f1 | sort -u
#
#   @com_github_nlohmann_json
#   @com_github_p4lang_PI_bf_9_5_0
#   @judy
#   @local_barefoot_bin
#
# NOTICE.opennsa.txt contains the dependencies for Broadcom OpenNSA
#
#   bazel cquery "filter(@, kind(cc_.*, deps(//stratum/hal/bin/bcm/standalone:stratum_bcm_opennsa) except deps(//stratum/hal/bin/dummy:stratum_dummy)))" | cut -d/ -f1 | sort -u
#
#   @com_github_broadcom_opennsa
#   @com_google_absl
#
# NOTICE.yaml.txt contains the dependencies for yaml-cpp
# NOTICE.sdklt.txt contains the dependencies for Broadcom SDKLT
#
#   bazel cquery "filter(@, kind(cc_.*, deps(//stratum/hal/bin/bcm/standalone:stratum_bcm_sdklt) except deps(//stratum/hal/bin/dummy:stratum_dummy)))" | cut -d/ -f1 | sort -u
#
#   @com_github_jbeder_yaml_cpp
#   @com_github_opennetworkinglab_sdklt
#   @com_google_absl
#
# NOTICE.p4c.txt contains the dependencies for the P4 compiler
#
#   bazel cquery "filter(@, kind(cc_.*, deps(//stratum/p4c_backends/fpm:p4c_fpm_deb) except deps(//stratum/hal/bin/dummy:stratum_dummy)))" | cut -d/ -f1 | sort -u
#
#   @boost
#   @com_github_facebook_zstd
#   @com_github_p4lang_p4c
#   @org_bzip_bzip2
#   @org_lzma_lzma

package(
    default_visibility = ["//visibility:public"],
)

exports_files(glob([
    "LICENSE*",
    "NOTICE*",
]))
