#!/usr/bin/python
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

from __future__ import print_function
from datetime import datetime

import grpc
import time

from github.com.openconfig.gnoi.system import system_pb2
from github.com.openconfig.gnoi.system import system_pb2_grpc
from google.protobuf import text_format

grpc_server = 'localhost:50051'


def Time():
    # NOTE(gRPC Python Team): .close() is possible on a channel and should be
    # used in circumstances in which the with statement does not fit the needs
    # of the code.
    with grpc.insecure_channel(grpc_server) as channel:
        stub = system_pb2_grpc.SystemStub(channel)

        response = stub.Time(system_pb2.TimeRequest())
        t = time.time()

    print("server time nanosec: " + str(response.time))
    print("client time nanosec: " + str(int(t * 1000000000)))
    print("-------------------")
    print("server time sec: " + str(response.time / 1000000000))
    print("client time sec: " + str(t))


def Ping():
    with grpc.insecure_channel(grpc_server) as channel:
        stub = system_pb2_grpc.SystemStub(channel)
        request = system_pb2.PingRequest(destination="google.com", count=2,
                                         size=512,
                                         interval=3000000000, wait=3000000000,
                                         source="0.0.0.0", do_not_resolve=True)
        print("Ping request :\t",
              text_format.MessageToString(request, as_one_line=True))
        ping_response = stub.Ping(request)

        resp = next(ping_response, None)
        while resp:
            print("Ping response :\t",
                  text_format.MessageToString(resp, as_one_line=True))
            resp = next(ping_response, None)


def Reboot():
    with grpc.insecure_channel(grpc_server) as channel:
        stub = system_pb2_grpc.SystemStub(channel)
        reboot = system_pb2.RebootRequest()
        reboot.method = system_pb2.RebootMethod.Value('COLD')
        reboot.message = "need reboot"
        stub.Reboot(reboot)
    print("Reboot request was sent.\n")


def RebootStatus():
    with grpc.insecure_channel(grpc_server) as channel:
        stub = system_pb2_grpc.SystemStub(channel)
        resp = stub.RebootStatus(system_pb2.RebootStatusRequest())
        print("RebootStatus response :\t",
              text_format.MessageToString(resp, as_one_line=True))


def SwitchControlProcessor():
    with grpc.insecure_channel(grpc_server) as channel:
        stub = system_pb2_grpc.SystemStub(channel)

        switch_req = system_pb2.SwitchControlProcessorRequest()

        for elem in "/tmp".split('/'):
            if elem:
                switch_req.control_processor.elem.add().name = elem

        resp = stub.SwitchControlProcessor(switch_req)
        print("SwitchControlProcessor response :\t",
              text_format.MessageToString(resp, as_one_line=False))


if __name__ == '__main__':
    Time()
    Ping()
    Reboot()
    RebootStatus()
    SwitchControlProcessor()
