#!/usr/bin/env python2

# Copyright 2019-present Dell EMC
# Copyright 2019-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

# Translate names in tables and counters of the P4Info file to be
# the same as the alias.  Needed for NP4 Intel SDK.

import argparse
import logging
import os
import sys

from google.protobuf import text_format
from p4.config.v1 import p4info_pb2

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("p4info_tr")


def error(msg, *args, **kwargs):
    logger.error(msg, *args, **kwargs)


def warn(msg, *args, **kwargs):
    logger.warn(msg, *args, **kwargs)


def info(msg, *args, **kwargs):
    logger.info(msg, *args, **kwargs)


def debug(msg, *args, **kwargs):
    logger.debug(msg, *args, **kwargs)


def tr_p4info(platform, p4info_path, p4info_out_path):
    """
    Translate to NP4 compatible p4info
    """
    p4info = p4info_pb2.P4Info()
    with open(p4info_path, 'r') as p4info_f:
        text_format.Merge(p4info_f.read(), p4info)

    # Spin through tables
    for table in p4info.tables:
         # Look for "standard_metadata" match fields
         for mf in table.match_fields:
            if platform == "np4":
                if mf.name.startswith("standard_metadata"):
                    mf.name = mf.name.replace("standard", "intrinsic");
                    mf.bitwidth = 8;
                elif mf.name == "vlan_tag.$valid$":
                    mf.name = "vlan_tag"

    if platform == "np4":
        # Spin through actions
        for action in p4info.actions:
            # Look for "port_num" param
            for p in action.params:
                if p.name == "port_num":
                    p.bitwidth = 8

    # Add the packet in controller packet metadata
    cpm = p4info.controller_packet_metadata.add()
    cpm.preamble.id = 67146229
    cpm.preamble.name = "packet_in"
    cpm.preamble.alias = "packet_in"
    metadata = cpm.metadata.add()
    metadata.id = 1
    metadata.name = "ingress_port"
    metadata.bitwidth = 9
    metadata = cpm.metadata.add()
    metadata.id = 2
    metadata.name = "_pad"
    metadata.bitwidth = 7

    # Add the packet out controller packet metadata
    cpm = p4info.controller_packet_metadata.add()
    cpm.preamble.id = 67121543
    cpm.preamble.name = "packet_out"
    cpm.preamble.alias = "packet_out"
    metadata = cpm.metadata.add()
    metadata.id = 1
    metadata.name = "egress_port"
    metadata.bitwidth = 9
    metadata = cpm.metadata.add()
    metadata.id = 2
    metadata.name = "_pad"
    metadata.bitwidth = 7

    # Write out new p4info
    with open(p4info_out_path, 'w') as f:
        f.write(text_format.MessageToString(p4info))

    print("P4Info successfully translated to {}".format(p4info_out_path))


# noinspection PyTypeChecker
def main():
    parser = argparse.ArgumentParser(
        description="translate P4Info for NP4 Intel")
    parser.add_argument('--p4info',
                        help='p4info protobuf in text format',
                        type=str, action="store", required=False,
                        default="p4info.txt")
    parser.add_argument('--p4info-out',
                        help='translated p4info protobuf in text format',
                        type=str, action="store", required=False,
                        default=None)
    parser.add_argument('--platform',
                        help='platform to transform (bmv2/np4)',
                        type=str, action="store", required=False,
                        default="np4")
    args, unknown_args = parser.parse_known_args()

    if not os.path.exists(args.p4info):
        error("P4Info file {} not found".format(args.p4info))
        sys.exit(1)

    if args.platform == "bmv2":
        p4info_out = "p4info_bmv2.txt"

    elif args.platform == "np4" or args.platform is None:
        p4info_out = "p4info_np4.txt"

    else:
        print("Invalid platform {}\n".format(args.platform))
        return -1

    if args.p4info_out is not None:
        p4info_out = args.p4info_out

    tr_p4info(platform=args.platform, p4info_path=args.p4info,
              p4info_out_path=p4info_out)

if __name__ == '__main__':
    main()
