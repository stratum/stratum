#!/usr/bin/env python2
#
# Copyright 2019 NoviFlow Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import os
import sys
import argparse

def setup_and_launch_ptf_runner(device, pipeline_name, protobuf_dir, ptf_dir, scapy_dir, skip_test=False, skip_bmv2_start=False):

    if 'STRATUM_ROOT' not in os.environ:
        print("Ptf_exec.py error: STRATUM_ROOT is not defined.")
        sys.exit(1)

    if 'LD_LIBRARY_PATH' not in os.environ:
        print("Ptf_exec.py error: LD_LIBRARY_PATH is not defined.")
        sys.exit(1)

    STRATUM_ROOT=os.environ['STRATUM_ROOT']
    LD_LIBRARY_PATH=os.environ['LD_LIBRARY_PATH']
    EXEC_ROOT=os.getcwd()

    SKIP_TEST = ""
    SKIP_BMV2_START = ""

    EXEC_PATH_PTF = EXEC_ROOT + "/py_ptf"

    EXTERNAL_PATH_GOOGLE_PROTOBUF = EXEC_ROOT + "/external/" + protobuf_dir + "/google/protobuf"
    EXTERNAL_PATH_GOOGLE_RPC      = EXEC_ROOT + "/external/com_google_googleapis/google/rpc"
    EXTERNAL_PATH_PTF             = EXEC_ROOT + "/external/" + ptf_dir + "/" + ptf_dir[6:].replace("_","-",1).replace("_",".") + ".data/scripts"
    EXTERNAL_PATH_P4_FABRIC_TEST  = EXEC_ROOT + "/external/com_github_opennetworkinglab_fabric_p4test/tests/ptf"

    PY_IMPORT_PATH_GOOGLE    = EXEC_PATH_PTF
    PY_IMPORT_PATH_P4RUNTIME = EXEC_ROOT + "/external/com_github_p4lang_p4runtime"
    PY_IMPORT_PATH_PTF       = EXEC_ROOT + "/external/" + ptf_dir
    PY_IMPORT_PATH_SCAPY     = EXEC_ROOT + "/external/" + scapy_dir

    PIPELINE_PATH_BINARY = "stratum/pipelines/" + pipeline_name + "/p4c-out/bmv2/"
    PIPELINE_PATH_PTF    = "stratum/pipelines/" + pipeline_name + "/ptf"

    if skip_test == True:
        SKIP_TEST = " --skip-test"
    if skip_bmv2_start == True:
        SKIP_BMV2_START = " --skip-bmv2-start"

    # Make ptf python script executable and setup paths ./google/protobuf
    # and ./google/rpc as Python searchable paths in ./py_ptf directory.

    ptf_setup_cmd = \
        "mkdir -p "  + EXEC_PATH_PTF + " && " +\
        "sudo mount -t tmpfs ptf_fs -o size=10M " + EXEC_PATH_PTF + " && " +\
        "sed '1 s/python/\/usr\/bin\/env python2/' " + EXTERNAL_PATH_PTF + "/ptf > " + EXEC_PATH_PTF + "/ptf && " +\
        "chmod +x " + EXEC_PATH_PTF + "/ptf && " +\
        "mkdir -p " + EXEC_PATH_PTF + "/google && " +\
        "touch "    + EXEC_PATH_PTF + "/google/__init__.py && " +\
        "cp -r "    + EXTERNAL_PATH_GOOGLE_PROTOBUF + " " + EXEC_PATH_PTF + "/google && " +\
        "cp -r "    + EXTERNAL_PATH_GOOGLE_RPC + " " + EXEC_PATH_PTF + "/google "

    res = os.system(ptf_setup_cmd)
    if res == 0:
        # Launch ptf_runner.py which ends up calling the ptf script
        # mentioned above.  ptf_runner.py assumes the python script
        # is reachable through $PATH.
        call_ptf_runner_cmd = \
            "sudo STRATUM_ROOT=" + STRATUM_ROOT +\
            " PATH=" + EXEC_PATH_PTF + ":$PATH" +\
            " LD_LIBRARY_PATH=" + LD_LIBRARY_PATH +\
            " PYTHONPATH=" + EXEC_PATH_PTF + ":" + PY_IMPORT_PATH_P4RUNTIME + ":" + PY_IMPORT_PATH_PTF + ":" + PY_IMPORT_PATH_SCAPY + \
            " " + EXTERNAL_PATH_P4_FABRIC_TEST + "/ptf_runner.py" +\
            " --device-id 1" +\
            " --device " + device +\
            " --port-map " + EXTERNAL_PATH_P4_FABRIC_TEST + "/port_map.veth.json" +\
            " --grpc-addr localhost:28000" +\
            " --p4info " + PIPELINE_PATH_BINARY + pipeline_name + ".p4info" +\
            " --bmv2-json " + PIPELINE_PATH_BINARY + pipeline_name + ".json" +\
            " --cpu-port 64" +\
            " --ptf-dir " + PIPELINE_PATH_PTF +\
            SKIP_TEST + SKIP_BMV2_START
        os.system(call_ptf_runner_cmd)

    else:
        print("Ptf_exec.py error: ptf_setup_cmd error.")

    # Unmount PTF disk

    ptf_umount_cmd = \
        "sudo umount ptf_fs"
    os.system(ptf_umount_cmd)

def main():
    parser = argparse.ArgumentParser(
        description="Start ptf_runner with parameters provided by Bazel")
    parser.add_argument('--device',
                        help='Target device',
                        type=str, action="store", required=True,
                        choices=['tofino', 'bmv2', 'stratum-bmv2'])
    parser.add_argument('--pipeline-name',
                        help='Name of pipeline',
                        type=str, action="store", required=True)
    parser.add_argument('--protobuf-dir',
                        help='Directory for protobufs in the form pypi__protobuf_<version>',
                        type=str, action="store", required=True)
    parser.add_argument('--ptf-dir',
                        help='Directory for ptf script in the form pypi__ptf_<version>',
                        type=str, action="store", required=True)
    parser.add_argument('--scapy-dir',
                        help='Directory for ptf script in the form pypi__scapy_<version>',
                        type=str, action="store", required=True)
    parser.add_argument('--skip-test',
                        help='Skip test execution (useful to perform only pipeline configuration)',
                        action="store_true", default=False)
    parser.add_argument('--skip-bmv2-start',
                        help='Skip switch start (requires that the switch be started manually \
                        beforehand, only applies to bmv2 and bmv2-stratum targets)',
                        action="store_true", default=False)
    args, unknown_args = parser.parse_known_args()

    setup_and_launch_ptf_runner(device=args.device,
                                pipeline_name=args.pipeline_name,
                                protobuf_dir=args.protobuf_dir,
                                ptf_dir=args.ptf_dir,
                                scapy_dir=args.scapy_dir,
                                skip_test=args.skip_test,
                                skip_bmv2_start=args.skip_bmv2_start)

if __name__ == '__main__':
    main()
