#!/usr/bin/env python2

# Copyright 2018-present Barefoot Networks, Inc.
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

import argparse
from time import sleep

import grpc
from gnmi import gnmi_pb2
import google.protobuf.text_format
import struct

parser = argparse.ArgumentParser(description='Test gNMI subscription')
parser.add_argument('--grpc-addr', help='gNMI server address',
                    type=str, action="store", default='localhost:50051')
parser.add_argument('--interface', help='Interface name',
                    type=str, action="store", required=False)
parser.add_argument('path', metavar='PE', help='Path elements',
                    type=str, nargs="*", default=["..."])

args = parser.parse_args()

def main():
    channel = grpc.insecure_channel(args.grpc_addr)
    stub = gnmi_pb2.gNMIStub(channel)

    req = gnmi_pb2.GetRequest()
    req.encoding = gnmi_pb2.PROTO
    path = req.path.add()
    e = path.elem.add()
    e.name = "interfaces"
    e = path.elem.add()
    e.name = "interface"
    if args.interface:
        e.key["name"] = args.interface
    for pe in args.path:
        e = path.elem.add()
        e.name = pe
    print "***************************"
    print "REQUEST"
    print req
    print "***************************"

    response = stub.Get(req)
    print "***************************"
    print "RESPONSE"
    print response
    print "***************************"

if __name__ == '__main__':
    main()
