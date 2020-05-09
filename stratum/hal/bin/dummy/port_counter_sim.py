#!/usr/bin/env python2.7
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

import grpc
import argparse
import sys
import time
import random

import stratum.hal.lib.dummy.dummy_test_pb2 as dummy_test_pb2
import stratum.hal.lib.dummy.dummy_test_pb2_grpc as dummy_test_pb2_grpc
import stratum.hal.lib.common.common_pb2 as common_pb2

from cli import create_port_counters_msg

DEFAULT_CNT_VALUES = [0 for _ in range(0, 14)]

parser = argparse.ArgumentParser(description='Test Service CLI')
parser.add_argument('--dry-run', help='Dry run',
                    action='store_const', const=True, default=False)
parser.add_argument('--test-service-url', help='Test service address',
                    type=str, default='localhost:28010')
parser.add_argument('--delay', help='Delay between each counter event',
                    type=int, default=1)
parser.add_argument('node', help='node id', type=int)
parser.add_argument('port', help='port id', type=int)

# (node,port) -> [in_octets, in_unicast_pkts, in_broadcast_pkts,
# in_multicast_pkts, in_discards, in_errors, in_unknown_protos,
# out_octets, out_unicast_pkts, out_broadcast_pkts, out_multicast_pkts,
# out_discards, out_errors, in_fcs_errors]
port_counters = {}

def main():
  args = parser.parse_args()
  channel = grpc.insecure_channel(args.test_service_url)
  stub = dummy_test_pb2_grpc.TestStub(channel)

  req, resp = None, None

  while True:
    counter_values = port_counters.get((args.node, args.port), DEFAULT_CNT_VALUES)
    for i in range(0, 14):
      num_to_increase = random.randint(1, 10)
      if i == 0 or i == 7:
        # make this as MB
        num_to_increase *= 10**6
      counter_values[i] += num_to_increase
    req = create_port_counters_msg(args.node, args.port, *counter_values)
    print (req)

    if req is not None and not args.dry_run:
      resp = stub.DeviceStatusUpdate(req)

    time.sleep(args.delay)


if __name__ == "__main__":
  main()