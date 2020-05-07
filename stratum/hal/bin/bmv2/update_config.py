#!/usr/bin/env python2

# Copyright 2013-present Barefoot Networks, Inc.
# SPDX-License-Identifier: Apache-2.0

import argparse
from time import sleep

import grpc
from p4.v1 import p4runtime_pb2
from p4.config.v1 import p4info_pb2
from p4.tmp import p4config_pb2
import google.protobuf.text_format
import struct
import threading
import time
import Queue

parser = argparse.ArgumentParser(description='Mininet demo')
parser.add_argument('--device-id', help='Device id of switch',
                    type=int, action="store", default=1)
parser.add_argument('--p4info', help='text p4info proto',
                    type=str, action="store", required=True)
parser.add_argument('--json', help='context json',
                    type=str, action="store", required=True)
parser.add_argument('--grpc-addr', help='P4Runtime gRPC server address',
                    type=str, action="store", default='localhost:50051')
parser.add_argument('--loopback',
                    help='Provide this flag if you are using the loopback '
                    'P4 program and we will test Packet IO',
                    action="store_true", default=False)

args = parser.parse_args()

def build_bmv2_config(bmv2_json_path):
    """
    Builds the device config for BMv2
    """
    device_config = p4config_pb2.P4DeviceConfig()
    device_config.reassign = True
    with open(bmv2_json_path) as f:
        device_config.device_data = f.read()
    return device_config

class Test:
    def __init__(self):
        self.device_id = args.device_id

        self.channel = grpc.insecure_channel(args.grpc_addr)
        self.stub = p4runtime_pb2.P4RuntimeStub(self.channel)

    def update_config(self):
        request = p4runtime_pb2.SetForwardingPipelineConfigRequest()
        request.device_id = self.device_id
        request.election_id.high = 0
        request.election_id.low = 1
        config = request.config
        with open(args.p4info, 'r') as p4info_f:
            google.protobuf.text_format.Merge(p4info_f.read(), config.p4info)
        device_config = build_bmv2_config(args.json)
        config.p4_device_config = device_config.SerializeToString()
        request.action = p4runtime_pb2.SetForwardingPipelineConfigRequest.VERIFY_AND_COMMIT
        try:
            self.stub.SetForwardingPipelineConfig(request)
        except Exception as e:
            print "Error during SetForwardingPipelineConfig"
            print str(e)
            return False
        return True

    def set_up_stream(self):
        self.stream_out_q = Queue.Queue()
        self.stream_in_q = Queue.Queue()
        def stream_req_iterator():
            while True:
                p = self.stream_out_q.get()
                if p is None:
                    break
                yield p

        def stream_recv(stream):
            for p in stream:
                self.stream_in_q.put(p)

        self.stream = self.stub.StreamChannel(stream_req_iterator())
        self.stream_recv_thread = threading.Thread(
            target=stream_recv, args=(self.stream,))
        self.stream_recv_thread.start()

        self.handshake()

    def handshake(self):
        req = p4runtime_pb2.StreamMessageRequest()
        arbitration = req.arbitration
        arbitration.device_id = self.device_id
        election_id = arbitration.election_id
        election_id.high = 0
        election_id.low = 1
        self.stream_out_q.put(req)

        rep = self.get_stream_packet("arbitration", timeout=3)
        if rep is None:
            print "Failed to establish handshake"

    def tear_down_stream(self):
        self.stream_out_q.put(None)
        self.stream_recv_thread.join()

    def get_packet_in(self, timeout=3):
        msg = self.get_stream_packet("packet", timeout)
        if msg is None:
            print "Packet in not received"
        else:
            return msg.packet

    def get_stream_packet(self, type_, timeout=1):
        start = time.time()
        try:
            while True:
                remaining = timeout - (time.time() - start)
                if remaining < 0:
                    break
                msg = self.stream_in_q.get(timeout=remaining)
                if not msg.HasField(type_):
                    continue
                return msg
        except:  # timeout expired
            pass
        return None

    def send_packet_out(self, packet):
        packet_out_req = p4runtime_pb2.StreamMessageRequest()
        packet_out_req.packet.CopyFrom(packet)
        self.stream_out_q.put(packet_out_req)

if __name__ == '__main__':
    test = Test()
    test.set_up_stream()
    test.update_config()
    if args.loopback:
        packet_out = p4runtime_pb2.PacketOut()
        packet_out.payload = "\xab" * 100
        test.send_packet_out(packet_out)
        test.get_packet_in()
    test.tear_down_stream()
