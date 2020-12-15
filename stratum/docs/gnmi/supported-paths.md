<!--
Copyright 2019-present Open Networking Foundation

SPDX-License-Identifier: Apache-2.0
-->
Supported gNMI paths
====

### Wildcard paths:

`/`

 - Subscription mode: ONCE, POLL
 - Get type: ALL, STATE
 - Set mode: REPLACE

`/interfaces/interface/...`

 - Subscription mode: ONCE, POLL
 - Get type: ALL, STATE
 - Set mode: Not valid

`/interfaces/interface[name=*]/state/ifindex`

 - Subscription mode: ONCE, POLL
 - Get type: ALL, STATE
 - Set mode: Not valid

`/interfaces/interface[name=*]/state/name`

 - Subscription mode: ONCE, POLL
 - Get type: ALL, STATE
 - Set mode: Not valid

`/interfaces/interface[name=*]/state/counters`

 - Subscription mode: ONCE, POLL, SAMPLE
 - Get type: ALL, STATE
 - Set mode: Not valid

### Alarms for chassis:


`/components/component[name=chassis name]/chassis/alarms/flow-programming-exception`

 - Subscription mode: ONCE, POLL, ON_CHANGE, SAMPLE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/components/component[name=chassis name]/chassis/alarms/flow-programming-exception/info`

 - Subscription mode: ONCE, POLL, ON_CHANGE, SAMPLE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/components/component[name=chassis name]/chassis/alarms/flow-programming-exception/severity`

 - Subscription mode: ONCE, POLL, ON_CHANGE, SAMPLE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/components/component[name=chassis name]/chassis/alarms/flow-programming-exception/status`

 - Subscription mode: ONCE, POLL, ON_CHANGE, SAMPLE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/components/component[name=chassis name]/chassis/alarms/flow-programming-exception/time-created`

 - Subscription mode: ONCE, POLL, ON_CHANGE, SAMPLE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/components/component[name=chassis name]/chassis/alarms/memory-error`

 - Subscription mode: ONCE, POLL, ON_CHANGE, SAMPLE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/components/component[name=chassis name]/chassis/alarms/memory-error/info`

 - Subscription mode: ONCE, POLL, ON_CHANGE, SAMPLE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/components/component[name=chassis name]/chassis/alarms/memory-error/severity`

 - Subscription mode: ONCE, POLL, ON_CHANGE, SAMPLE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/components/component[name=chassis name]/chassis/alarms/memory-error/status`

 - Subscription mode: ONCE, POLL, ON_CHANGE, SAMPLE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/components/component[name=chassis name]/chassis/alarms/memory-error/time-created`

 - Subscription mode: ONCE, POLL, ON_CHANGE, SAMPLE
 - Get type: ALL, STATE
 - Set mode: Not valid

### Node:

`/debug/nodes/node[name=node name]/packet-io/debug-string`

 - Subscription mode: ONCE, POLL
 - Get type: ALL, STATE
 - Set mode: Not valid

### Interface config:

`/interfaces/interface[name=port name]/config/enabled`

 - Subscription mode: ONCE, POLL, SAMPLE
 - Get type: ALL, STATE
 - Set mode: REPLACE, UPDATE

`/interfaces/interface[name=port name]/config/health-indicator`

 - Subscription mode: ONCE, POLL, SAMPLE
 - Get type: ALL, STATE
 - Set mode: REPLACE, UPDATE

`/interfaces/interface[name=port name]/config/loopback-mode`

 - Subscription mode: ONCE, POLL, SAMPLE
 - Get type: ALL, STATE
 - Set mode: REPLACE, UPDATE

`/interfaces/interface[name=port name]/ethernet/config/forwarding-viable`

 - Subscription mode: ONCE, POLL, SAMPLE
 - Get type: ALL, STATE
 - Set mode: REPLACE, UPDATE

`/interfaces/interface[name=port name]/ethernet/config/mac-address`

 - Subscription mode: ONCE, POLL, SAMPLE
 - Get type: ALL, STATE
 - Set mode: REPLACE, UPDATE

`/interfaces/interface[name=port name]/ethernet/config/port-speed`

 - Subscription mode: ONCE, POLL, SAMPLE
 - Get type: ALL, STATE
 - Set mode: REPLACE, UPDATE

`/interfaces/interface[name=port name]/ethernet/config/auto-negotiate`

 - Subscription mode: ONCE, POLL, SAMPLE
 - Get type: ALL, STATE
 - Set mode: REPLACE, UPDATE

### Interface state:

`/interfaces/interface[name=port name]/ethernet/state/auto-negotiate`

 - Subscription mode: ONCE, POLL, SAMPLE, ON_CHANGE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/interfaces/interface[name=port name]/ethernet/state/forwarding-viable`

 - Subscription mode: ONCE, POLL, SAMPLE, ON_CHANGE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/interfaces/interface[name=port name]/ethernet/state/mac-address`

 - Subscription mode: ONCE, POLL, SAMPLE, ON_CHANGE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/interfaces/interface[name=port name]/ethernet/state/negotiated-port-speed`

 - Subscription mode: ONCE, POLL, SAMPLE, ON_CHANGE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/interfaces/interface[name=port name]/ethernet/state/port-speed`

 - Subscription mode: ONCE, POLL, SAMPLE, ON_CHANGE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/interfaces/interface[name=port name]/state/admin-status`

 - Subscription mode: ONCE, POLL, SAMPLE, ON_CHANGE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/interfaces/interface[name=port name]/state/health-indicator`

 - Subscription mode: ONCE, POLL, SAMPLE, ON_CHANGE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/interfaces/interface[name=port name]/state/loopback-mode`

 - Subscription mode: ONCE, POLL, SAMPLE, ON_CHANGE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/interfaces/interface[name=port name]/state/ifindex`

 - Subscription mode: ONCE, POLL, SAMPLE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/interfaces/interface[name=port name]/state/last-change`

 - Subscription mode: ONCE, POLL, SAMPLE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/interfaces/interface[name=port name]/state/name`

 - Subscription mode: ONCE, POLL, SAMPLE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/interfaces/interface[name=port name]/state/oper-status`

 - Subscription mode: ONCE, POLL, SAMPLE, ON_CHANGE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/interfaces/interface[name=port name]/state/hardware-port`

 - Subscription mode: ONCE, POLL, SAMPLE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/interfaces/interface[name=port name]/state/counters`

 - Subscription mode: ONCE, POLL, SAMPLE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/interfaces/interface[name=port name]/state/counters/in-broadcast-pkts`

 - Subscription mode: ONCE, POLL, SAMPLE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/interfaces/interface[name=port name]/state/counters/in-discards`

 - Subscription mode: ONCE, POLL, SAMPLE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/interfaces/interface[name=port name]/state/counters/in-errors`

 - Subscription mode: ONCE, POLL, SAMPLE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/interfaces/interface[name=port name]/state/counters/in-fcs-errors`

 - Subscription mode: ONCE, POLL, SAMPLE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/interfaces/interface[name=port name]/state/counters/in-multicast-pkts`

 - Subscription mode: ONCE, POLL, SAMPLE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/interfaces/interface[name=port name]/state/counters/in-octets`

 - Subscription mode: ONCE, POLL, SAMPLE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/interfaces/interface[name=port name]/state/counters/in-unicast-pkts`

 - Subscription mode: ONCE, POLL, SAMPLE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/interfaces/interface[name=port name]/state/counters/in-unknown-protos`

 - Subscription mode: ONCE, POLL, SAMPLE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/interfaces/interface[name=port name]/state/counters/out-broadcast-pkts`

 - Subscription mode: ONCE, POLL, SAMPLE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/interfaces/interface[name=port name]/state/counters/out-discards`

 - Subscription mode: ONCE, POLL, SAMPLE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/interfaces/interface[name=port name]/state/counters/out-errors`

 - Subscription mode: ONCE, POLL, SAMPLE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/interfaces/interface[name=port name]/state/counters/out-multicast-pkts`

 - Subscription mode: ONCE, POLL, SAMPLE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/interfaces/interface[name=port name]/state/counters/out-octets`

 - Subscription mode: ONCE, POLL, SAMPLE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/interfaces/interface[name=port name]/state/counters/out-unicast-pkts`

 - Subscription mode: ONCE, POLL, SAMPLE
 - Get type: ALL, STATE
 - Set mode: Not valid

### LACP state:

`/lacp/interfaces/interface[name=port name]/state/system-id-mac`

 - Subscription mode: ONCE, POLL, SAMPLE, ON_CHANGE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/lacp/interfaces/interface[name=port name]/state/system-priority`

 - Subscription mode: ONCE, POLL, SAMPLE, ON_CHANGE
 - Get type: ALL, STATE
 - Set mode: Not valid

### QoS state:

`/qos/interfaces/interface[name=port name]/output/queues/queue[name=queue name]/state/dropped-pkts`

 - Subscription mode: ONCE, POLL, SAMPLE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/qos/interfaces/interface[name=port name]/output/queues/queue[name=queue name]/state/id`

 - Subscription mode: ONCE, POLL, SAMPLE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/qos/interfaces/interface[name=port name]/output/queues/queue[name=queue name]/state/name`

 - Subscription mode: ONCE, POLL, SAMPLE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/qos/interfaces/interface[name=port name]/output/queues/queue[name=queue name]/state/transmit-octets`

 - Subscription mode: ONCE, POLL, SAMPLE
 - Get type: ALL, STATE
 - Set mode: Not valid

`/qos/interfaces/interface[name=port name]/output/queues/queue[name=queue name]/state/transmit-pkts`

 - Subscription mode: ONCE, POLL, SAMPLE
 - Get type: ALL, STATE
 - Set mode: Not valid
