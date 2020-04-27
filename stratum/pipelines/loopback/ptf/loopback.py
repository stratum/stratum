#!/usr/bin/env python2
# Copyright 2019 NoviFlow Inc.
# SPDX-License-Identifier: Apache-2.0

import ptf
import os
from ptf import config
import ptf.testutils as testutils

from base_test import P4RuntimeTest, autocleanup, stringify, ipv4_to_binary

class LoopbackTest(P4RuntimeTest):
    pass

class LoopbackAllPortsTest(LoopbackTest):
    @autocleanup
    def runTest(self):
        port_list = [0, 1, 2, 3, 4, 5 ,6 ,7]
        dst_addr = '00:11:22:33:44:55'
        src_addr = '00:aa:bb:cc:dd:ee'

        # Create test packet.
        pkt = testutils.simple_tcp_packet(eth_dst=dst_addr, eth_src=src_addr)

        # Looped back packet has swapped ethernet addresses.
        pkt_check = testutils.simple_tcp_packet(eth_dst=src_addr, eth_src=dst_addr)

        for port in port_list:

            ig_port = self.swports(port)
            eg_port = self.swports(port)
            # port is 9-bit in v1model, i.e. 2 bytes
            ig_port_str = stringify(ig_port, 2)
            eg_port_str = stringify(eg_port, 2)

            # check that the entry is hit and that no other packets are received
            testutils.send_packet(self, ig_port, pkt)
            testutils.verify_packets(self, pkt_check, [eg_port])


