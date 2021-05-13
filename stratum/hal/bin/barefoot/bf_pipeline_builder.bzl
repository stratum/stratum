# Copyright 2021-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

def packed_bf_pipeline(name, src):
    """Packs a compiled P4 TNA program into the Stratum BF pipeline format.

     The resulting file can be used in the P4Runtime device_config field when
     pushing a pipeline.
    """

    # Since the exact name of the conf file depends on the P4 program name, we
    # try to find it dynamically.
    cmds = [
        "tar -xf $< -C $(@D)",
        "CONF_FILE=`ls $(@D)/*.conf | head -1`",
        "$(location //stratum/hal/bin/barefoot:bf_pipeline_builder) -p4c_conf_file=$$CONF_FILE -bf_pipeline_config_binary_file=$@",
    ]
    native.genrule(
        name = name,
        srcs = src,
        outs = [name + ".pb.bin"],
        tools = ["//stratum/hal/bin/barefoot:bf_pipeline_builder"],
        cmd = " && ".join(cmds),
    )
