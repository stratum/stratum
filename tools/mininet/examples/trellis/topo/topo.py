#!/usr/bin/python
# Copyright 2019-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

from mininet.cli import CLI
from mininet.log import setLogLevel
from mininet.net import Mininet
from mininet.node import Host
from mininet.topo import Topo
from stratum import StratumBmv2Switch


class RoutedHost(Host):
    """Host that can be configured with multiple IP addresses."""

    def __init__(self, name, ips, gateway, *args, **kwargs):
        super(RoutedHost, self).__init__(name, *args, **kwargs)
        self.ips = ips
        self.gateway = gateway

    def config(self, **kwargs):
        Host.config(self, **kwargs)
        self.cmd('ip -4 addr flush dev %s' % self.defaultIntf())
        for ip in self.ips:
            self.cmd('ip addr add %s dev %s' % (ip, self.defaultIntf()))
        self.cmd('ip route add default via %s' % self.gateway)
        # Disable offload
        for attr in ["rx", "tx", "sg"]:
            cmd = "/sbin/ethtool --offload %s %s off" % (self.defaultIntf(), attr)
            self.cmd(cmd)


class TaggedRoutedHost(RoutedHost):
    """Host that can be configured with multiple IP addresses."""

    def __init__(self, name, ips, gateway, vlan, *args, **kwargs):
        super(RoutedHost, self).__init__(name, *args, **kwargs)
        self.ips = ips
        self.gateway = gateway
        self.vlan = vlan
        self.vlanIntf = None

    def config(self, **kwargs):
        Host.config(self, **kwargs)
        self.vlanIntf = "%s.%s" % (self.defaultIntf(), self.vlan)
        self.cmd('ip -4 addr flush dev %s' % self.defaultIntf())
        self.cmd('ip link add link %s name %s type vlan id %s' % (
            self.defaultIntf(), self.vlanIntf, self.vlan))
        self.cmd('ip link set up %s' % self.vlanIntf)
        # Set ips and gateway
        for ip in self.ips:
            self.cmd('ip addr add %s dev %s' % (ip, self.vlanIntf))
        self.cmd('ip route add default via %s' % self.gateway)
        # Update the intf name and host's intf map
        self.defaultIntf().name = self.vlanIntf
        self.nameToIntf[self.vlanIntf] = self.defaultIntf()
        # Disable offload
        for attr in ["rx", "tx", "sg"]:
            cmd = "/sbin/ethtool --offload %s %s off" % (self.vlanIntf, attr)
            self.cmd(cmd)

    def terminate(self, **kwargs):
        self.cmd('ip link remove link %s' % self.vlanIntf)
        super(TaggedRoutedHost, self).terminate()


class LeafSpineTopo(Topo):
    "Trellis basic topology"

    def __init__(self, *args, **kwargs):
        Topo.__init__(self, *args, **kwargs)

        # Leaves
        leaf1 = self.addSwitch('leaf1', grpcPort=50001)
        leaf2 = self.addSwitch('leaf2', grpcPort=50002)

        # Spines
        spine1 = self.addSwitch('spine1', grpcPort=50003)
        spine2 = self.addSwitch('spine2', grpcPort=50004)

        # Links
        self.addLink(spine1, leaf1)
        self.addLink(spine1, leaf2)
        self.addLink(spine2, leaf1)
        self.addLink(spine2, leaf2)

        # IPv4 Hosts
        h1 = self.addHost('h1', cls=RoutedHost, mac='00:aa:00:00:00:01',
                          ips=['10.0.2.1/24'], gateway='10.0.2.254')
        h2 = self.addHost('h2', cls=TaggedRoutedHost, mac='00:aa:00:00:00:02',
                          ips=['10.0.2.2/24'], gateway='10.0.2.254', vlan=10)
        h3 = self.addHost('h3', cls=RoutedHost, mac='00:aa:00:00:00:03',
                          ips=['10.0.3.1/24'], gateway='10.0.3.254')
        h4 = self.addHost('h4', cls=TaggedRoutedHost, mac='00:aa:00:00:00:04',
                          ips=['10.0.3.2/24'], gateway='10.0.3.254', vlan=20)
        self.addLink(h1, leaf1)
        self.addLink(h2, leaf1)
        self.addLink(h3, leaf2)
        self.addLink(h4, leaf2)


def main():
    net = Mininet(
        topo=LeafSpineTopo(),
        switch=StratumBmv2Switch,
        controller=None)
    net.start()
    CLI(net)
    net.stop()


if __name__ == "__main__":
    setLogLevel('info')
    main()
