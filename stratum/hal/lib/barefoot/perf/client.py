from stratum.hal.lib.barefoot.perf import bfruntime_pb2_grpc as bfrt_grpc
import grpc
import signal
import sys
import time

import platform
print(platform.python_version())

from google.protobuf.internal.api_implementation import Version, Type
print(Version(), Type())

count = 0
start = 0

def signal_handler(sig, frame):
    end = time.time()
    print('Read %d in %fs' % (count, end - start))
    print('100k in %fs' % (100000.0 / count * (end - start)))
    sys.exit(0)
signal.signal(signal.SIGINT, signal_handler)

with grpc.insecure_channel('localhost:50051') as channel:
    stub = bfrt_grpc.BfRuntimeStub(channel)
    responses = stub.StreamChannel(iter([]))
    i = 0
    start = time.time()
    for resp in responses:
        i += 1
        if i % 100 == 0:
            count = i
