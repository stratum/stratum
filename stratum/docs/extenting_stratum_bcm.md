<!--
Copyright 2020-present Open Networking Foundation

SPDX-License-Identifier: Apache-2.0
-->
# How to Add a Dataplane Feature to Stratum (BCM)

This guide describes how to add a new feature to Stratum BCM on the example of MPLS.
Check PR [#222](https://github.com/stratum/stratum/pull/222) for the code changes.

## 0. Prepare Testbed

Stratum BCM does not support simulators for the ASIC, therefore developing on real
hardware is encouraged and recommended.

Most features require some form of traffic to test them. Having an additional
server with QSFP NICs connected to a few switch ports can be helpful.

## 1. Define Target and Scope

First, you have to define the scope of the feature you want to add. This helps
to keep track of progress and forces you to make up a use case beforehand.

For MPLS, we aim for a relatively narrow scope:

- Single label
- Encapsulation of IPv4 into MPLS packets
- Transit (LSR) of MPLS packets
- Decapsulation of MPLS into IPv4 packets
- Compatible with existing LPM IPv4 routing code

### Explore Docs

The first step is to make sure the Switch supports and the [SDKLT](https://github.com/opennetworkinglab/SDKLT) actually exposes the required functionality. The [Logical Table Guide](https://broadcom-network-switching-software.github.io/Logical_Table_Documentation_Guide) and [Feature List](https://github.com/Broadcom-Network-Switching-Software/SDKLT/wiki/Tomahawk-Feature-List) are great starting points. Other products like OF-DPA and SDK6 can also provide hints.

For MPLS, we find the following tables of interest:

- TNL_MPLS_*
- L3_EIF, field `TNL_ENCAP_ID`

### Create Solution in SDKLT CLI Script

It has proven helpful to a functioning target in place to work towards to. One example of this would be an SDKLT CLI script that configures the switch for a specific target scenario. It also makes sure one fully understands the problem space, the relations between the different tables, and which APIs of SDKLT have to be triggered.

CLI scripts are simple text files that contain a series of commands as one would type them in the CLI normally. A few examples are [included](https://github.com/opennetworkinglab/SDKLT/tree/master/examples).

A script must be self-contained and only solve one specific sub-problem. We start with the encapsulation part of MPLS:

```bash
# Setup packet IO from diag shell.
pktdev init

# Map all queues to channel 1.
pktdev chan queuemap 1 highword=0xffff lowword=0xffffffff

# Start packet watcher to capture packets at CPU.
rx watcher create

# Enable ports
lt PC_PORT traverse -u ENABLE=1

# Enable Link Manager (linkscan task).
lt LM_CONTROL insert SCAN_ENABLE=1

# Configure software Linkscan mode for ports 1-2.
lt LM_PORT_CONTROL insert PORT_ID=58 LINKSCAN_MODE=SOFTWARE
lt LM_PORT_CONTROL insert PORT_ID=62 LINKSCAN_MODE=SOFTWARE

# Allow Link Manager to collect status of the ports 1-2.
# Default Link Manager scan interval is 250ms.
sleep quiet 1

# Check the status of ports 1-2. LINK_STATE=1 indicates port is UP.
lt PC_PORT_STATUS lookup PORT_ID=58
lt PC_PORT_STATUS lookup PORT_ID=62

# Enable MAC counters for ports 1 and 2.
lt CTR_MAC insert PORT_ID=58
lt CTR_MAC insert PORT_ID=62

# Configure ports 1 and 2 in forwarding state.
lt VLAN_STG insert VLAN_STG_ID=1 STATE[58]=FORWARD STATE[62]=FORWARD

# Configure PORT_PKT_CONTROL to send ARP request, reply to CPU.
lt PORT_PKT_CONTROL insert PORT_PKT_CONTROL_ID=3 ARP_REQUEST_TO_CPU=1 \
                           ARP_REPLY_TO_CPU=1

# Add VLAN tag for untagged packets.
lt VLAN_ING_TAG_ACTION_PROFILE insert VLAN_ING_TAG_ACTION_PROFILE_ID=1 \
                                      UT_OTAG=ADD

# Create L3_IIF_PROFILE 2 and enable IPv4 routing.
lt L3_IIF_PROFILE insert L3_IIF_PROFILE_ID=2 IPV4_UC=1

# Create L3_IIF index 1 and set VRF_ID=10.
lt L3_IIF insert L3_IIF_ID=1 VRF_ID=10 L3_IIF_PROFILE_ID=2

# Create VLAN 1 and include port 1.
lt VLAN insert VLAN_ID=1 UNTAGGED_MEMBER_PORTS=0xffffffffffffffffffffffffffff \
               VLAN_STG_ID=1 L3_IIF_ID=1

# Enable IPv4 routing on Port 1.
lt PORT insert PORT_ID=58 V4L3=1 PORT_TYPE=ETHERNET MY_MODID=0 \
               PORT_PKT_CONTROL_ID=3 VLAN_ING_TAG_ACTION_PROFILE_ID=1 ING_OVID=1

# Configure a profile to classify 0x8100 at bytes 12,13 of the packet to be
# outer TPID and add 0x8100 as outgoing packets outer TPID.
lt VLAN_OUTER_TPID insert VLAN_OUTER_TPID_ID=0 ING_TPID=0x8100 EGR_TPID=0x8100

# Configure to classify packets with value 0x8100 at bytes 12,13 as
# outer VLAN tagged packet.
lt PORT_POLICY insert PORT_ID=58 PASS_ON_OUTER_TPID_MATCH[0]=1
lt PORT_POLICY insert PORT_ID=62 PASS_ON_OUTER_TPID_MATCH[0]=1

# Program L2_MY_STATION to enable routing for destination MAC 00:00:00:BB:BB:BB
# and VLAN_ID 1.
lt L2_MY_STATION insert VLAN_ID=1 VLAN_ID_MASK=0xfff MAC_ADDR=0x000000BBBBBB \
                        MAC_ADDR_MASK=0xffffffffffff IPV4_TERMINATION=1 \
                        COPY_TO_CPU=1

# Program TNL_MPLS_ENCAP to specify MPLS tunnel encapsulation parameters.
lt TNL_MPLS_ENCAP insert TNL_ENCAP_ID=4 NUM_LABELS=1 MAX_LABELS=2 \
                         LABEL[0]=100 LABEL_TTL[0]=0x40

# Create egress L3 interface (L3_EIF).
lt L3_EIF insert L3_EIF_ID=1 VLAN_ID=1 MAC_SA=0x000000AAAAAA TNL_ENCAP_ID=4 \
                 TNL_TYPE=MPLS

# Configure max MTU for L3 egress interface.
lt L3_UC_TNL_MTU insert L3_EIF_ID=1 L3_MTU=0x3fff

# Program TNL_MPLS_DST_MAC with destination MAC address.
lt TNL_MPLS_DST_MAC insert TNL_MPLS_DST_MAC_ID=1 DST_MAC=0x000000000003

# Program TNL_MPLS_ENCAP_NHOP with next-hop parameters for packets
# entering into the MPLS tunnel.
lt TNL_MPLS_ENCAP_NHOP insert NHOP_ID=2 MODPORT=62 MODID=0 L3_EIF_ID=1 \
                              TNL_MPLS_DST_MAC_ID=1 COPY_TO_CPU=0

# Create ECMP group.
lt ECMP insert ECMP_ID=1 NHOP_ID[0]=2 NUM_PATHS=1

# Create IPv4 route.
lt L3_IPV4_UC_ROUTE_VRF insert VRF_ID=10 IPV4=0x0a020000 IPV4_MASK=0xFFFF0000 \
                               ECMP_ID=1 ECMP_NHOP=1

echo "Done"
```

When pasted into the prompt of a stratum_bcm instance, it configures the switch to accept IPv4 packets to `10.2.0.0/16` on port 58 and encapsulate them in MPLS packets with label 100 and TTL 64 to port 62.

We can verify this by sending packets from the connected test host using scapy. Using PTF or testvectors to generate traffic is also possible.

```python
sendp(Ether(dst="00:00:00:bb:bb:bb")/IP(dst="10.2.0.1",ttl=10)/TCP()/'aaaaaaaaaaa', iface="ens6f0")
```

And capture the send/recieved packets:

```
3c:fd:fe:9d:f1:48 > 00:00:00:bb:bb:bb, ethertype IPv4 (0x0800), length 65: (tos 0x0, ttl 10, id 1, offset 0, flags [none], proto TCP (6), length 51)
    10.1.0.1.20 > 10.2.0.1.80: Flags [S], cksum 0x3388 (correct), seq 0:11, win 8192, length 11: HTTP
	0x0000:  0000 00bb bbbb 3cfd fe9d f148 0800 4500
	0x0010:  0033 0001 0000 0a06 9cc0 0a01 0001 0a02
	0x0020:  0001 0014 0050 0000 0000 0000 0000 5002
	0x0030:  2000 3388 0000 6161 6161 6161 6161 6161
	0x0040:  61

00:00:00:aa:aa:aa > 00:00:00:00:00:04, ethertype MPLS unicast (0x8847), length 69: MPLS (label 300, exp 7, [S], ttl 64)
	(tos 0x0, ttl 9, id 1, offset 0, flags [none], proto TCP (6), length 51)
    10.1.0.1.20 > 10.2.0.1.80: Flags [S], cksum 0x3388 (correct), seq 0:11, win 8192, length 11: HTTP
	0x0000:  0000 0000 0004 0000 00aa aaaa 8847 0012
	0x0010:  cf40 4500 0033 0001 0000 0906 9dc0 0a01
	0x0020:  0001 0a02 0001 0014 0050 0000 0000 0000
	0x0030:  0000 5002 2000 3388 0000 6161 6161 6161
	0x0040:  6161 6161 61
```

Depending on the scope and extent of the complete solution, one can either repeat this for all sub-features first or continue with one and reiterate later.

## 2. Map Feature to P4 code

Next, the feature has to be mapped to P4 code. Here we decide how it should be expressed so that the compiler can make sense of it. It's also important to make the additions integrate well in existing features, to maximize code reuse and minimize effort.

### Headers & Parser

Adding new protocols come with their own headers & types, and we have to make the compiler understand them.

```p4
const bit<16> ETHERTYPE_MPLS = 0x8847;

header mpls_t {
    bit<20> label;
    bit<3> tc;
    bit<1> bos;
    bit<8> ttl;
}

state parse_ethernet {
    pk.extract(hdr.ethernet);
    transition select(hdr.ethernet.ether_type) {
        ETHERTYPE_VLAN1: parse_vlan;
        ETHERTYPE_VLAN2: parse_vlan;
        ETHERTYPE_VLAN3: parse_vlan;
        ETHERTYPE_VLAN4: parse_vlan;
        ETHERTYPE_IPV4: parse_ipv4;
        ETHERTYPE_IPV6: parse_ipv6;
        ETHERTYPE_ARP: parse_arp;
        ETHERTYPE_MPLS: parse_mpls; // <- New transition
        default: accept;
    }
}

state parse_mpls {
    pk.extract(hdr.mpls);
    transition select(pk.lookahead<bit<4>>()) {
        4: parse_ipv4;
        6: parse_ipv6;
        default: accept;
    }
}
```

Then the SDKLT tables have to be mapped to P4 primitives like actions, tables, and table entries. Let's see how this is done for MPLS encap:

As seen in the CLI config script, the encapsulation decision is made in the `L3_IPV4_UC_ROUTE_VRF` forwarding table, by setting a `ENCAP_NHOP` instead of a `L3_UC_NHOP` one.

| TODO(max): add images of paper scribbles

## 3. Stratum Modifications

### Overview

In general, most features will require changes to multiple areas of Stratum:

- p4c-fpm Compiler

    Here you decide how your feature will be exposed in P4 code. Hide unnecessary details about implementation details in both Statum and the underlying SDK.
    - New header fields in `p4_table_defs.proto` and `p4_table_map.proto`
        - E.g. `P4FieldType::P4_FIELD_TYPE_MPLS_LABEL`
    - New Pipeline stages in `p4_annotation.proto`
        - E.g. `P4Annotation::PipelineStage::L3_MPLS`
    - New mappers (rare)
    - Extend standard parser map
        - Add new parser states

- BCM node Runtime
    - Add fields to `bcm.proto`
        - This is the internal representation for data to be fed into the BCM APIs. Most likely new `BcmField`s, `BcmFlowEntry` types or tunnels.
    - Handle new feature in `bcm_lX_manager.cc`
        - Depending on which layer the feature operates, the appropriate manager has to be modified. This involves understanding the new P4 annotations and translating them to the `bcm.proto` equivalents.
    - bcm_sdk_wrapper.cc
        - New functions that abstract the SDK APIs have to be added. The main purpose is to provide a nice and easy to use C++ interface to the underlying SDK. Avoid pulling in complex data types (protobuf messages) and favor scalar types. Ensure thread-safety, if not provided by the SDK.

- Extend `main.p4` pipeline

    Add or modify the new table(s) according to your additions to the compiler.

## 3. Testing

To ensure a feature works as expected initially and continues to be correctly
implemented, it's recommended to write dataplane tests. This could either be
done in a ad-hoc manner with something like [scapy](https://github.com/secdev/scapy),
or it could become a permanent part of the [TestVectors](https://github.com/stratum/testvectors-runner)
test [suite](https://github.com/stratum/testvectors) that is run on real devices
as part of the Continuos Certification Program.
