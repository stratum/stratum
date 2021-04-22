# Copyright 2021-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

def packed_bf_pipeline(name, conf):
    """Packs a compiled P4 TNA program into the Stratum BF pipeline format.

     The resulting file can be used in the P4Runtime device_config field when
     pushing a pipeline.
    """
    cmd = "$(location //stratum/hal/bin/barefoot:bf_pipeline_builder) " + \
          " -p4c_conf_file=$<" + \
          " -bf_pipeline_config_binary_file=$@"
    native.genrule(
        name = name,
        srcs = [conf],
        outs = [name + ".pb.bin"],
        tools = ["//stratum/hal/bin/barefoot:bf_pipeline_builder"],
        cmd = cmd,
    )
