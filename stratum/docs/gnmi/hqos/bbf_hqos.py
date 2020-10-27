#!/usr/bin/env python2.7
# Copyright 2020-present Open Networking Foundation
# Copyright 2020-present Dell EMC
# SPDX-License-Identifier: Apache-2.0

'''
This script populates the BroadBand Forum's QoS yang data model
with an example HQoS configuration.
'''

from bbf_bindings import bbf_qos_traffic_mngt, ietf_interfaces
import json

intfs = ietf_interfaces()

# TM Profiles
tm = bbf_qos_traffic_mngt()
tmprof1 = tm.tm_profiles.tc_id_2_queue_id_mapping_profile.add("tmprof1")
tc1 = tmprof1.mapping_entry.add(1)
tc1.local_queue_id = 1

# BAC Entries
bac1 = tm.tm_profiles.bac_entry.add("bac1")
bac1.max_queue_size = 30
bac1.taildrop.max_threshold = 20
#bac1.red.min_threshold = 20
#bac1.red.max_threshold = 30
#bac1.red.max_probability = 40
#bac1.wtaildrop.color.green.max_threshold = 10
#bac1.wtaildrop.color.yellow.max_threshold = 20
#bac1.wtaildrop.color.red.max_threshold = 30
#bac1.wred.color.green.min_threshold = 10
#bac1.wred.color.green.max_threshold = 20
#bac1.wred.color.green.max_probability = 30
#bac1.wred.color.yellow.min_threshold = 20
#bac1.wred.color.yellow.max_threshold = 30
#bac1.wred.color.yellow.max_probability = 40
#bac1.wred.color.red.min_threshold = 40
#bac1.wred.color.red.max_threshold = 50
#bac1.wred.color.red.max_probability = 50

# Shaper profiles
sp1 = tm.tm_profiles.shaper_profile.add("sp1")
sp1.single_token_bucket.pir = 20
sp1.single_token_bucket.pir.pbs = 30
sp2 = tm.tm_profiles.shaper_profile.add("sp2")
sp2.single_token_bucket.pir.pir = 21
sp2.single_token_bucket.pir.pbs = 31
sp3 = tm.tm_profiles.shaper_profile.add("sp3")
sp3.single_token_bucket.pir.pir = 22
sp3.single_token_bucket.pir.pbs = 32

# Queues directly on an interface
# eth-0-1 interface
if1 = intfs.interfaces.interface.add("eth-0-1")

# Queue 1
q1 = if1.tm_root.queue.add(1)
q1.bac_name = "bac1"
q1.priority = 1
q1.weight = 10
q1.pre_emption = True

# Hierarchical Schedulers on an interface
# eth-0-2 interface
if2 = intfs.interfaces.interface.add("eth-0-2")
sched = if2.tm_root.scheduler_node.add("sched1")
sched.description = "scheduler 1"
sched.scheduling_level = 1
sched.shaper_name = "sp1"
sn = sched.child_scheduler_nodes.add("sched2")
sn.priority = 0
sn.weight = 10

sched = if2.tm_root.scheduler_node.add("sched2")
sched.description = "scheduler 2"
sched.scheduling_level = 2
sched.shaper_name = "sp2"
sn = sched.child_scheduler_nodes.add("sched3")
sn.priority = 0
sn.weight = 10

sched = if2.tm_root.scheduler_node.add("sched3")
sched.description = "scheduler 3"
sched.scheduling_level = 3
sched.shaper_name = "sp3"
sched.contains_queues = True

# Queues
# Following line causes whole scheduler_node list in tm_root to be cleared ?
#q11 = sched.queue.add(7)
#q11.bac_name = "bac11"
#q11.priority = 0
#q11.weight = 10
#q11.pre_emption = False

#q12 = sched.queue.add(2)
#q12.bac_name = "bac12"
#q12.priority = 0
#q12.weight = 20
#q12.pre_emption = False


print "tm: {}".format(json.dumps(tm.get(), indent=4))
print "intfs: {}".format(json.dumps(intfs.get(), indent=4))
