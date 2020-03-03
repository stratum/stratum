<!--
Copyright 2019-present Open Networking Foundation

SPDX-License-Identifier: Apache-2.0
-->
# Trellis+Stratum example

This directory contains scripts that use Docker and Mininet to emulate a network
of `stratum_bmv2` switches controlled by Trellis, a set of SDN applications
running on top of ONOS to provide the control plane for an IP fabric based on
MPLS segment-routing.

To learn more about Trellis: <https://docs.trellisfabric.org>

## P4 program

This example is based on `fabric.p4`, a P4 program distributed as part of ONOS,
designed to work with the Trellis apps to provide capabilities such as L2
bridging, L3 routing, MPLS segmemnt-routing, etc.

The P4 code can be found [here][fabric.p4].

## Prerequisites

To run this example you will need:

* Docker v17+ (with `docker-compose`)
* `make`
* Access to Internet (to download the necessary Docker images)

## Network topology

The file [topo/topo.py](topo/topo.py) defines a 2x2 leaf-spine topology using
the `StratumBMv2Switch` Mininet class provided by the [Stratum Mininet Docker
image][mn-stratum] (see [stratum.py][stratum.py]). It also defines a number of
hosts attached to the fabric.

The file [topo/netcfg.json](topo/netcfg.json) defines the corresponding
configuration consumed by ONOS to discover and control all 4 switch instances
(such as the address and port of the gRPC server, the pipeconf to push, etc.),
as well as the Trellis-specific fabric configuration (interface IP address, VLAN
configuration, etc.)

## Make commands

We provide a set of make-based commands to control the different aspects of this
example. These commands will be used in the next section, we provide here a
reference:

| Make command        | Description          |
|---------------------|------------------------------------------------------- |
| `make pull `        | Pull all required Docker images                        |
| `make start`        | Start containers (`mininet` and `onos`)                |
| `make stop`         | Stop and remove all containers                         |
| `make onos-cli`     | Access the ONOS CLI (password: `rocks`, Ctrl+D to exit)|
| `make onos-ui`      | Open the ONOS Web UI (user `onos` password `rocks`)    |
| `make mn-cli`       | Access the Mininet CLI (Ctrl+A Ctrl+D to exit)         |
| `make onos-log`     | Show the ONOS log    |
| `make mn-log`       | Show the Mininet log (i.e., the CLI output)            |
| `make netcfg`       | Push netcfg.json file (network config) to ONOS         |
| `make reset`        | Reset the tutorial environment                         |

## Walktrough

To run this example you will need multiple terminal windows (or tabs) to
operate with the CLI of Mininet and ONOS. We use the following convention
to distinguish between commands of different CLIs:

* `onos>` for commands in the ONOS CLI;
* `mininet>` for the Mininet CLI;
* otherwise, commands are meant to be executed in the terminal prompt.

As a first step, start the ONOS and Mininet containers:

```bash
make start
```

Show the ONOS log:

```bash
make onos-log
```

Wait for ONOS to complete boot, i.e., until the ONOS log stops showing new
messages.

On a second terminal window, push the netcfg.json file:

```bash
make netcfg
```

You should see the ONOS log updating with messages showing discovery of the 4
`stratum_bmv2` switches and links. The log will also show warn messages like:

```
Cannot translate DefaultForwardingObjective: unsupported forwarding function type 'PSEUDO_WIRE'...
```

This is expected, as not all Trellis features are supported today with
`fabric.p4`. One of such feature is pseudo-wire. You can ignore that.

### Check ONOS

Now we use the ONOS CLI to verify that everything is in working order.
To access the ONOS CLI

```bash
make onos-cli
```

When prompted, use password `rocks`.

You should now see the ONOS CLI command prompt. For a list of possible
commands that you can use here, type:

```
onos> help onos:
```

#### Check apps

Verify that all required apps are activated and running:

```
onos> apps -a -s
```

Make sure you see the following list of apps displayed:

```
* ... org.onosproject.protocols.grpc        2.2.0    gRPC Protocol Subsystem
* ... org.onosproject.protocols.gnmi        2.2.0    gNMI Protocol Subsystem
* ... org.onosproject.route-service         2.2.0    Route Service Server
* ... org.onosproject.drivers               2.2.0    Default Drivers
* ... org.onosproject.generaldeviceprovider 2.2.0    General Device Provider
* ... org.onosproject.protocols.p4runtime   2.2.0    P4Runtime Protocol Subsystem
* ... org.onosproject.p4runtime             2.2.0    P4Runtime Provider
* ... org.onosproject.drivers.p4runtime     2.2.0    P4Runtime Drivers
* ... org.onosproject.protocols.gnoi        2.2.0    gNOI Protocol Subsystem
* ... org.onosproject.hostprovider          2.2.0    Host Location Provider
* ... org.onosproject.lldpprovider          2.2.0    LLDP Link Provider
* ... org.onosproject.drivers.gnoi          2.2.0    gNOI Drivers
* ... org.onosproject.drivers.gnmi          2.2.0    gNMI Drivers
* ... org.onosproject.pipelines.basic       2.2.0    Basic Pipelines
* ... org.onosproject.drivers.stratum       2.2.0    Stratum Drivers
* ... org.onosproject.portloadbalancer      2.2.0    Port Load Balance Service
* ... org.onosproject.mcast                 2.2.0    Multicast traffic control
* ... org.onosproject.segmentrouting        2.2.0    Segment Routing
* ... org.onosproject.pipelines.fabric      2.2.0    Fabric Pipeline
* ... org.onosproject.gui                   2.2.0    ONOS Legacy GUI
* ... org.onosproject.drivers.bmv2          2.2.0    BMv2 Drivers
```

#### Check topology

Verify that the network configuration has been applied correctly:

```
onos> netcfg
```

You should see the same content of `./topo/netcfg.json`, 4 devices and 4
interfaces configured.

Make sure that all 4 switches have been discovered and ONOS is connected to
them:

```
onos> devices -s
id=device:leaf1, available=true, role=MASTER, type=SWITCH, driver=stratum-bmv2:org.onosproject.pipelines.fabric
id=device:leaf2, available=true, role=MASTER, type=SWITCH, driver=stratum-bmv2:org.onosproject.pipelines.fabric
id=device:spine1, available=true, role=MASTER, type=SWITCH, driver=stratum-bmv2:org.onosproject.pipelines.fabric
id=device:spine2, available=true, role=MASTER, type=SWITCH, driver=stratum-bmv2:org.onosproject.pipelines.fabric
```

Show links, there should be 8 (unidirectional), automatically discovered by
means of LLDP-based packet-in/out performed by the `lldpprovider` app:

```
onos> links
src=device:leaf1/1, dst=device:spine1/1, type=DIRECT, state=ACTIVE, expected=false
src=device:leaf1/2, dst=device:spine2/1, type=DIRECT, state=ACTIVE, expected=false
src=device:leaf2/1, dst=device:spine1/2, type=DIRECT, state=ACTIVE, expected=false
src=device:leaf2/2, dst=device:spine2/2, type=DIRECT, state=ACTIVE, expected=false
src=device:spine1/1, dst=device:leaf1/1, type=DIRECT, state=ACTIVE, expected=false
src=device:spine1/2, dst=device:leaf2/1, type=DIRECT, state=ACTIVE, expected=false
src=device:spine2/1, dst=device:leaf1/2, type=DIRECT, state=ACTIVE, expected=false
src=device:spine2/2, dst=device:leaf2/2, type=DIRECT, state=ACTIVE, expected=false
```

Show port information, obtained by ONOS by querying the OpenConfig Interfaces
model of each switch using gNMI:

```
onos> ports -s
id=device:leaf1, available=true, role=MASTER, type=SWITCH, driver=stratum-bmv2:org.onosproject.pipelines.fabric
  port=[leaf1-eth1](1), state=enabled, type=copper, speed=10000 , last-changed=1562973339888950100
  port=[leaf1-eth2](2), state=enabled, type=copper, speed=10000 , last-changed=1562973339889147911
  port=[leaf1-eth3](3), state=enabled, type=copper, speed=10000 , last-changed=1562973339889292422
  port=[leaf1-eth4](4), state=enabled, type=copper, speed=10000 , last-changed=1562973339889441068
id=device:leaf2, available=true, role=MASTER, type=SWITCH, driver=stratum-bmv2:org.onosproject.pipelines.fabric
  port=[leaf2-eth1](1), state=enabled, type=copper, speed=10000 , last-changed=1562973339790191500
  port=[leaf2-eth2](2), state=enabled, type=copper, speed=10000 , last-changed=1562973339790495800
  port=[leaf2-eth3](3), state=enabled, type=copper, speed=10000 , last-changed=1562973339790662700
  port=[leaf2-eth4](4), state=enabled, type=copper, speed=10000 , last-changed=1562973339790817500
id=device:spine1, available=true, role=MASTER, type=SWITCH, driver=stratum-bmv2:org.onosproject.pipelines.fabric
  port=[spine1-eth1](1), state=enabled, type=copper, speed=10000 , last-changed=1562973339842834700
  port=[spine1-eth2](2), state=enabled, type=copper, speed=10000 , last-changed=1562973339843289992
id=device:spine2, available=true, role=MASTER, type=SWITCH, driver=stratum-bmv2:org.onosproject.pipelines.fabric
  port=[spine2-eth1](1), state=enabled, type=copper, speed=10000 , last-changed=1562973339821364600
  port=[spine2-eth2](2), state=enabled, type=copper, speed=10000 , last-changed=1562973339821586565
```

Show port counters, also obtained by querying the OpenConfig Interfaces model
via gNMI:

```
onos> portstats
deviceId=device:leaf1
   port=[leaf1-eth1](1), pktRx=202, pktTx=202, bytesRx=25048, bytesTx=24846, pktRxDrp=0, pktTxDrp=0, Dur=311
   port=[leaf1-eth2](2), pktRx=202, pktTx=202, bytesRx=25048, bytesTx=24846, pktRxDrp=0, pktTxDrp=0, Dur=311
   port=[leaf1-eth3](3), pktRx=13, pktTx=204, bytesRx=1038, bytesTx=24986, pktRxDrp=0, pktTxDrp=0, Dur=311
   port=[leaf1-eth4](4), pktRx=20, pktTx=203, bytesRx=1544, bytesTx=24920, pktRxDrp=0, pktTxDrp=0, Dur=311
deviceId=device:leaf2
   port=[leaf2-eth1](1), pktRx=202, pktTx=202, bytesRx=25048, bytesTx=24846, pktRxDrp=0, pktTxDrp=0, Dur=311
   port=[leaf2-eth2](2), pktRx=202, pktTx=202, bytesRx=25048, bytesTx=24846, pktRxDrp=0, pktTxDrp=0, Dur=311
   port=[leaf2-eth3](3), pktRx=11, pktTx=203, bytesRx=858, bytesTx=24916, pktRxDrp=0, pktTxDrp=0, Dur=311
   port=[leaf2-eth4](4), pktRx=22, pktTx=203, bytesRx=1724, bytesTx=24920, pktRxDrp=0, pktTxDrp=0, Dur=311
deviceId=device:spine1
   port=[spine1-eth1](1), pktRx=204, pktTx=204, bytesRx=25092, bytesTx=25296, pktRxDrp=0, pktTxDrp=0, Dur=312
   port=[spine1-eth2](2), pktRx=204, pktTx=204, bytesRx=25092, bytesTx=25296, pktRxDrp=0, pktTxDrp=0, Dur=312
deviceId=device:spine2
   port=[spine2-eth1](1), pktRx=204, pktTx=204, bytesRx=25092, bytesTx=25296, pktRxDrp=0, pktTxDrp=0, Dur=313
   port=[spine2-eth2](2), pktRx=204, pktTx=204, bytesRx=25092, bytesTx=25296, pktRxDrp=0, pktTxDrp=0, Dur=313
```

Verify that the interface configuration has been applied correctly:

```
onos> interfaces
h1: port=device:leaf1/3 ips=[10.0.2.254/24] mac=00:00:00:00:01:00 vlanUntagged=10
h2: port=device:leaf1/4 ips=[10.0.2.254/24] mac=00:00:00:00:01:00 vlanTagged=[10]
h3: port=device:leaf2/3 ips=[10.0.3.254/24] mac=00:00:00:00:02:00 vlanUntagged=20
h4: port=device:leaf2/4 ips=[10.0.3.254/24] mac=00:00:00:00:02:00 vlanTagged=[20]
```

You should see 4 interfaces configured with the same information contained in
the netcfg file.

#### Check flows and groups

Check the flow rules inserted by the ONOS apps. To check just the count for
each switch:

```
onos> flows -c
deviceId=device:leaf1, flowRuleCount=32
deviceId=device:spine1, flowRuleCount=28
deviceId=device:spine2, flowRuleCount=28
deviceId=device:leaf2, flowRuleCount=32
```

You can also dump all flows for a given switch:

```
onos> flows -s any device:spine1
```

These flows are generated and inserted by the Trellis apps (`segmentrouting`
among all), in response to the network config and the topology discovered by
ONOS.

Similarly, you can check the groups installed for a given switch:

```
onos> groups any device:leaf1
deviceId=device:leaf1, groupCount=2
   id=0x2, state=ADDED, type=ALL, bytes=0, packets=0, appId=org.onosproject.segmentrouting, referenceCount=0
       id=0x2, bucket=1, bytes=0, packets=0, weight=-1, actions=[OUTPUT:3]
       id=0x2, bucket=2, bytes=0, packets=0, weight=-1, actions=[OUTPUT:4]
       id=0x2, bucket=3, bytes=0, packets=0, weight=-1, actions=[OUTPUT:CONTROLLER]
   id=0x8, state=ADDED, type=SELECT, bytes=0, packets=0, appId=org.onosproject.segmentrouting, referenceCount=0
       id=0x8, bucket=1, bytes=0, packets=0, weight=1, actions=[FabricIngress.next.mpls_routing_hashed(dmac=0x110, port_num=0x1, smac=0x100, label=0xc8)]
       id=0x8, bucket=2, bytes=0, packets=0, weight=1, actions=[FabricIngress.next.mpls_routing_hashed(dmac=0x210, port_num=0x2, smac=0x100, label=0xc8)]
```

ONOS groups are used to abstract P4Runtime action profile groups, multicast
groups, and clone session groups.

#### Use ONOS web UI

Open a browser to <http://localhost:8181/onos/ui/> (or use `make onos-ui`). When
asked, enter the username `onos` and password `rocks`.

While here, feel free to interact with and discover the ONOS UI. For more
information on how to use the ONOS web UI please refer to this guide:
<https://wiki.onosproject.org/x/OYMg>

To show or hide switch labels, press `L` on your keyboard.

To show or hide link stats, press `A` on your keyboard multiple times until you see
(pkt/second or bit/second).

#### Check forwarding

It is finally time to test connectivity between the hosts of our Mininet
network. To access the Mininet CLI (Ctrl-A Ctrl-D to exit):

```bash
make mn-cli
```

On the Mininet prompt, start a ping between `h1` and `h2`:

```
mininet> h1 ping h2
PING 10.0.2.2 (10.0.2.2) 56(84) bytes of data.
64 bytes from 10.0.2.2: icmp_seq=1 ttl=63 time=7.81 ms
64 bytes from 10.0.2.2: icmp_seq=2 ttl=63 time=3.66 ms
64 bytes from 10.0.2.2: icmp_seq=3 ttl=63 time=3.44 ms
...
```

Ping should work! If you examine the ONOS log you should notice messages about
the discovery of these two hosts. This is achieved by cloning ARP requests to
the control plane by means of P4Runtime packet-in.

Execute the following ONOS command to verify that hosts are discovered:

```
onos> hosts -s
```

`h1` and `h2` are connnected to the same leaf and they belong to the same
subnet. For this reason their packets are bridged. Let's now try to ping hosts
on different leaves, to see how packets are routed across the spines. For
example, let's ping `h3` from `h1`:

```
mininet> h1 ping h3
```

The **ping should NOT work**, and the reason is that ONOS doesn't know the
location of `h3`, and as such it has not installed the necessary rules to
forward packets. In a more complicated Trellis setup where the DHCP Relay app is
in use, ONOS can learn host information when the host is requesting an IP
address using DHCP. However, in this simple topology, ONOS only learns host
information from ARP requests intercepted in the network. Indeed, while ONOS
just learned the location of `h1` and `h2` because of the ARP packets exchanged
between these two, `h3` is on a different subnet, hence no ARP exchange happens
between `h1` and `h3`.

To have ONOS discover the hosts, we can generate ARP packets by pinging the
fabric interface gateway IP address from each host. By using the IP address
obtained from `onos> interfaces`, let's start a ping from `h3`:

```
mininet> h3 ping 10.0.3.254
PING 10.0.3.254 (10.0.3.254) 56(84) bytes of data.
64 bytes from 10.0.3.254: icmp_seq=1 ttl=64 time=73.0 ms
64 bytes from 10.0.3.254: icmp_seq=2 ttl=64 time=18.4 ms
64 bytes from 10.0.3.254: icmp_seq=3 ttl=64 time=17.7 ms
...
```

This is the IP address associated to the leaf switch interface attached to `h3`.
ICMP Echo Request packets are sent to ONOS as packet-ins, which in turn sends
ICMP Echo Replies as packet-out. This is equivalent to pinging the interface of
a traditional router, but now handled in an SDN way.

In the ONOS log, you should see messages showing that the location of `h3` has
been discovered. Let's try again pinging from `h1`:

```
mininet> h1 ping h3
PING 10.0.3.1 (10.0.3.1) 56(84) bytes of data.
64 bytes from 10.0.3.1: icmp_seq=1 ttl=62 time=8.87 ms
64 bytes from 10.0.3.1: icmp_seq=2 ttl=62 time=8.60 ms
64 bytes from 10.0.3.1: icmp_seq=3 ttl=62 time=8.45 ms
^C
```

### Congratulations!

You have completed all the steps of this example.

### Troubleshooting and stratum_bmv2 logs

If ping doesn't work, we reccommend checking the ONOS log as well as the log of
the `stratum_bmv2` instances running in the Mininet container. These log
files can be found under `./tmp` in this directory. For example, the log of
switch instance `leaf1` will be at `./tmp/leaf1/stratum_bmv2.log`

For more information on all files found under `./tmp` refers to [mn-stratum
documentation][mn-stratum-tmp].

[mn-stratum]: ../../README.md
[stratum.py]: ../../stratum.py
[mn-stratum-tmp]: ../../README.md#logs-and-other-temporary-files
[fabric.p4]: https://github.com/opennetworkinglab/onos/tree/master/pipelines/fabric/src/main/resources
