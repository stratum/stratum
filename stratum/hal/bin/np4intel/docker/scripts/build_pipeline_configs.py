#!/usr/bin/env python

# Copyright 2019-present Dell EMC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

#
# Build a fowarding_pipeline_config.pb.txt file for
# Stratum based on the p4info file and the device configs
# found in the device_config directory
#

import os
import glob
import argparse

from p4.v1 import p4runtime_pb2
from p4.config.v1 import p4info_pb2
import forwarding_pipeline_configs_pb2
from pi.np4 import p4_device_config_pb2
from google.protobuf import text_format

parser = argparse.ArgumentParser(description='Build Fwd Pipeline Configs')
parser.add_argument('--device-config-dir', help='Device donfig directory',
                    type=str, action="store", 
                    default="device_configs")
parser.add_argument('--p4info', help='P4 Info file name',
                    type=str, action="store", default="p4info.txt")
parser.add_argument('--pipeline-config',
                    help='Forwarding Pipeline Config file name',
                    type=str, action="store", 
                    default="pipeline_config.pb.txt")

args = parser.parse_args()


def build_device_config(filename):
    """Builds P4 Device Config data"""

    dev_config = p4_device_config_pb2.P4DeviceConfig()
    with open(filename, "r") as f:
        file_content = f.read()
    text_format.Parse(file_content, dev_config)
    return dev_config


def build_p4info():
    """Builds P4 Info"""

    p4info = p4info_pb2.P4Info()
    with open(arg.p4info, "r") as f:
        text_format.Merge(f.read(), p4info)
    return p4info


def build_fwd_pipeline_config(fp_configs, filename):
    # 
    # Grab P4 config
    dev_config = build_device_config(filename)

    # Create a ForwardingPipelineConfig keyed on node_id
    fwd_config = fp_configs.node_id_to_config[dev_config.node_id]

    # Set the device config
    fwd_config.p4_device_config = dev_config.SerializeToString()

    # Build P4 Info
    with open(args.p4info, "r") as f:
        text_format.Merge(f.read(), fwd_config.p4info)


def main():
    print("Building P4 Forwarding Pipeline Configs:")

    # Build forwarding pipeline configs
    fp_configs = forwarding_pipeline_configs_pb2.ForwardingPipelineConfigs()

    # Build forwarding pipeline per device config
    for filename in glob.glob(os.path.join(args.device_config_dir, '*.pb.txt')):
        print("  %s" % filename)
        build_fwd_pipeline_config(fp_configs, filename)

    # Write out forwarding pipeline config
    with open(args.pipeline_config, 'w') as f:
        f.write(text_format.MessageToString(fp_configs))

    print("Successfully written out to %s" % args.pipeline_config)

if __name__ == '__main__':
    main()
