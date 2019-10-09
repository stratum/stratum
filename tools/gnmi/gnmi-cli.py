#!/usr/bin/env python2

# Copyright 2018-present Open Networking Foundation.
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

import re
import argparse
from time import sleep
import ast

import grpc
from gnmi import gnmi_pb2
from gnmi import gnmi_pb2_grpc
import threading
import Queue

# https://stackoverflow.com/questions/15008758/parsing-boolean-values-with-argparse
def str2bool(v):
    return v.lower() in ('yes', 'true', 't', 'y', '1')

parser = argparse.ArgumentParser(description='Test gNMI subscription')
parser.add_argument('--grpc-addr', help='gNMI server address',
                    type=str, action="store", default='localhost:28000')
parser.add_argument('cmd', help='gNMI command', type=str, choices=['get', 'set', 'sub-onchange', 'sub-sample', 'cap', 'del'])
parser.add_argument('path', help='gNMI Path', type=str)

# gNMI options for SetRequest
parser.add_argument('--bool-val', help='[SetRequest only] Set boolean value',
                    type=str2bool, action="store", required=False)
parser.add_argument('--int-val', help='[SetRequest only] Set int value',
                    type=int, action="store", required=False)
parser.add_argument('--uint-val', help='[SetRequest only] Set uint value',
                    type=int, action="store", required=False)
parser.add_argument('--string-val', help='[SetRequest only] Set string value',
                    type=str, action="store", required=False)
parser.add_argument('--float-val', help='[SetRequest only] Set float value',
                    type=float, action="store", required=False)
parser.add_argument('--bytes-val', help='[SetRequest only] Set bytes value',
                    type=str, action="store", required=False)
parser.add_argument('--interval', help='[Sample subscribe only] Sample subscribe poll interval in ms',
                    type=int, action="store", default=5000)
parser.add_argument('--replace', help='[SetRequest only] Use replace instead of update',
                    action="store_true", required=False)

args = parser.parse_args()

def parse_key_val(key_val_str):
    # [key1=val1,key2=val2,.....]
    key_val_str = key_val_str[1:-1]  # remove "[]"
    return [kv.split('=') for kv in key_val_str.split(',')]

# parse path_str string and add elements to path (gNMI Path class)
def build_path(path_str, path):
    if path_str == '/':
        # the root path should be an empty path
        return

    path_elem_info_list = re.findall(r'/([^/\[]+)(\[([^=]+=[^\]]+)\])?', path_str)

    for path_elem_info in path_elem_info_list:
        # [('interfaces', ''), ('interface', '[name=1/1/1]'), ...]
        pe = path.elem.add()
        pe.name = path_elem_info[0]

        if path_elem_info[1]:
            for kv in parse_key_val(path_elem_info[1]):
                # [('name', '1/1/1'), ...]
                pe.key[kv[0]] = kv[1]

def print_msg(msg, prompt):
    print("***************************")
    print(prompt)
    print(msg)
    print("***************************")

def build_gnmi_get_req():
    req = gnmi_pb2.GetRequest()
    req.encoding = gnmi_pb2.PROTO
    path = req.path.add()
    build_path(args.path, path)
    if args.path == '/':
        # Special case
        req.type = gnmi_pb2.GetRequest.CONFIG
    return req

def build_gnmi_set_req():
    req = gnmi_pb2.SetRequest()
    if (args.replace):
        update = req.replace.add()
    else:
        update = req.update.add()
    path = update.path
    if (args.path != '/'):
        build_path(args.path, path)
    if (args.bool_val is not None):
        update.val.bool_val = args.bool_val
    elif (args.int_val is not None):
        update.val.int_val = args.int_val
    elif (args.uint_val is not None):
        update.val.uint_val = args.uint_val
    elif (args.string_val is not None):
        update.val.string_val = args.string_val
    elif (args.float_val is not None):
        update.val.float_val = args.float_val
    elif (args.bytes_val is not None):
        update.val.bytes_val = ast.literal_eval("b'" + args.bytes_val + "'")
    else:
        print("No typed value set")
        return None
    return req

def build_gnmi_del_req():
    req = gnmi_pb2.SetRequest()
    delete = req.delete.add()
    build_path(args.path, delete)
    return req

# for subscrption
stream_out_q = Queue.Queue()
stream_in_q = Queue.Queue()
stream = None

def build_gnmi_sub_onchange():
    req = gnmi_pb2.SubscribeRequest()
    subList = req.subscribe
    subList.mode = gnmi_pb2.SubscriptionList.STREAM
    subList.updates_only = True
    sub = subList.subscription.add()
    sub.mode = gnmi_pb2.ON_CHANGE
    path = sub.path
    build_path(args.path, path)
    return req

def build_gnmi_sub_sample():
    req = gnmi_pb2.SubscribeRequest()
    subList = req.subscribe
    subList.mode = gnmi_pb2.SubscriptionList.STREAM
    subList.updates_only = True
    sub = subList.subscription.add()
    sub.mode = gnmi_pb2.SAMPLE
    sub.sample_interval = args.interval
    path = sub.path
    build_path(args.path, path)
    return req


def req_iterator():
    while True:
        req = stream_out_q.get()
        if req is None:
            break
        print_msg(req, "REQUEST")
        yield req

def stream_recv(stream):
    for resp in stream:
        print_msg(resp, "RESPONSE")
        stream_in_q.put(resp)

def main():
    channel = grpc.insecure_channel(args.grpc_addr)
    stub = gnmi_pb2_grpc.gNMIStub(channel)
    req = None

    if args.cmd == 'get':
        req = build_gnmi_get_req()
        print_msg(req, "REQUEST")
        resp = stub.Get(req)
        print_msg(resp, "RESPONSE")
    elif args.cmd == 'set':
        req = build_gnmi_set_req()
        print_msg(req, "REQUEST")
        resp = stub.Set(req)
        print_msg(resp, "RESPONSE")
    elif args.cmd == 'del':
        req = build_gnmi_del_req()
        print_msg(req, "REQUEST")
        resp = stub.Set(req)
        print_msg(resp, "RESPONSE")
    elif args.cmd == 'sub-onchange':
        req = build_gnmi_sub_onchange()
        stream_out_q.put(req)
        stream = stub.Subscribe(req_iterator())
        stream_recv_thread = threading.Thread(
            target=stream_recv, args=(stream,))
        stream_recv_thread.start()

        try:
            while True:
                sleep(1)
        except KeyboardInterrupt:
            stream_out_q.put(None)
            stream_recv_thread.join()
    elif args.cmd == 'sub-sample':
        req = build_gnmi_sub_sample()
        stream_out_q.put(req)
        stream = stub.Subscribe(req_iterator())
        stream_recv_thread = threading.Thread(
            target=stream_recv, args=(stream,))
        stream_recv_thread.start()

        try:
            while True:
                sleep(1)
        except KeyboardInterrupt:
            stream_out_q.put(None)
            stream_recv_thread.join()

    elif args.cmd == 'cap':
        req = gnmi_pb2.CapabilityRequest()
        print_msg(req, "REQUEST")
        resp = stub.Capabilities(req)
        print_msg(resp, "RESPONSE")
    else:
        print('Unknown command %s', args.cmd)
        return

if __name__ == '__main__':
    main()

