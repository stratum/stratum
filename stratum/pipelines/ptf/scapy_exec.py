#!/usr/bin/env python2
# Copyright 2019 NoviFlow Inc.
# SPDX-License-Identifier: Apache-2.0

import os
import argparse

EXEC_ROOT=os.getcwd()

def setup_and_launch_scapy(scapy_dir):

    EXEC_PATH_SCAPY      = EXEC_ROOT + "/py_scapy"
    EXTERNAL_PATH_SCAPY  = EXEC_ROOT + "/external/" + scapy_dir + "/" + scapy_dir[6:].replace("_","-",1).replace("_",".") + ".data/scripts"
    PY_IMPORT_PATH_SCAPY = EXEC_ROOT + "/external/" + scapy_dir

    scapy_setup_cmd = \
        "mkdir -p "  + EXEC_PATH_SCAPY + " && " +\
        "sudo mount -t tmpfs scapy_fs -o size=1M " + EXEC_PATH_SCAPY + " && " +\
        "sed '1 s/python/\/usr\/bin\/env python2/' " + EXTERNAL_PATH_SCAPY + "/scapy > " + EXEC_PATH_SCAPY + "/scapy && " +\
        "chmod +x " + EXEC_PATH_SCAPY + "/scapy"

    res = os.system(scapy_setup_cmd)
    if res == 0:
        scapy_call_cmd = \
            "sudo PYTHONPATH=" + PY_IMPORT_PATH_SCAPY + \
            " " + EXEC_PATH_SCAPY + "/scapy"
        os.system(scapy_call_cmd)

    scapy_umount_cmd = \
        "sudo umount scapy_fs"
    res = os.system(scapy_umount_cmd)
    if res != 0:
        sys.exit(1)

def main():
    parser = argparse.ArgumentParser(
        description="Start Scapy with parameters provided by Bazel")
    parser.add_argument('--scapy-dir',
                        help='Directory for ptf script in the form pypi__scapy_<version>',
                        type=str, action="store", required=True)
    args, unknown_args = parser.parse_known_args()

    setup_and_launch_scapy(scapy_dir=args.scapy_dir)

if __name__ == '__main__':
    main()
