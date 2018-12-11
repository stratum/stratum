#!/usr/bin/env python2.7
# Copyright 2018-present Open Networking Foundation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http:#www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import grpc
import argparse

import stratum.hal.lib.dummy.dummy_test_pb2 as dummy_test_pb2
import stratum.hal.lib.dummy.dummy_test_pb2_grpc as dummy_test_pb2_grpc
import stratum.hal.lib.common.common_pb2 as common_pb2

node_port_states = ['oper_status', 'admin_status', 'mac_address', 'port_speed',
                  'negotiated_port_speed', 'lacp_router_mac',
                  'lacp_system_priority', 'port_counters',
                  'forwarding_viability', 'health_indicator']
node_states = ['node_packetio_debug_info']
chassis_states = ['memory_error_alarm', 'flow_programming_exception_alarm']
port_queue_states = ['port_qos_counters']

def mac_to_int(mac_address):
  hex_digits = mac_address.split(':')
  int_mac = 0
  for digit in hex_digits:
    int_mac += int(digit, 16)
    int_mac <<= 8
  int_mac >>= 8
  return int_mac

def node_port_msg(func):
  def node_port_func(node_id, port_id, *args):
    req = dummy_test_pb2.DeviceStatusUpdateRequest()
    req.source.port.node_id = int(node_id)
    req.source.port.port_id = int(port_id)
    return func(req, *args)
  return node_port_func

def chassis_msg(func):
  def chassis_func(*args):
    req = dummy_test_pb2.DeviceStatusUpdateRequest()
    req.source.chassis.ParseFromString('')
    return func(req, *args)
  return chassis_func

def port_queue_msg(func):
  def port_queue_func(node_id, port_id, queue_id, *args):
    req = dummy_test_pb2.DeviceStatusUpdateRequest()
    req.source.port_queue.node_id = int(node_id)
    req.source.port_queue.port_id = int(port_id)
    req.source.port_queue.queue_id = int(queue_id)
    return func(req, *args)
  return port_queue_func

def node_msg(func):
  def node_fnuc(node_id, *args):
    req = dummy_test_pb2.DeviceStatusUpdateRequest()
    req.source.node.node_id = int(node_id)
    return func(req, *args)
  return node_fnuc

@node_port_msg
def create_oper_status_msg(req, *args):
  req.state_update.oper_status.state = common_pb2.PortState.Value(args[0])
  return req

@node_port_msg
def create_negotiated_port_speed_msg(req, *args):
  req.state_update.negotiated_port_speed.speed_bps = int(args[0])
  return req

@node_port_msg
def create_admin_status_msg(req, *args):
  req.state_update.admin_status.state = common_pb2.AdminState.Value(args[0])
  return req

@node_port_msg
def create_mac_address_msg(req, *args):
  req.state_update.mac_address.mac_address = mac_to_int(args[0])
  return req

@node_port_msg
def create_port_speed_msg(req, *args):
  req.state_update.port_speed.speed_bps = int(args[0])
  return req

@node_port_msg
def create_lacp_router_mac_msg(req, *args):
  req.state_update.lacp_router_mac.mac_address = mac_to_int(args[0])
  return req

@node_port_msg
def create_lacp_system_priority_msg(req, *args):
  req.state_update.lacp_system_priority.priority = int(args[0])
  return req

@node_port_msg
def create_port_counters_msg(req, *args):
  req.state_update.port_counters.in_octets = int(args[0])
  req.state_update.port_counters.in_unicast_pkts = int(args[1])
  req.state_update.port_counters.in_broadcast_pkts = int(args[2])
  req.state_update.port_counters.in_multicast_pkts = int(args[3])
  req.state_update.port_counters.in_discards = int(args[4])
  req.state_update.port_counters.in_errors = int(args[5])
  req.state_update.port_counters.in_unknown_protos = int(args[6])
  req.state_update.port_counters.out_octets = int(args[7])
  req.state_update.port_counters.out_unicast_pkts = int(args[8])
  req.state_update.port_counters.out_broadcast_pkts  = int(args[9])
  req.state_update.port_counters.out_multicast_pkts  = int(args[10])
  req.state_update.port_counters.out_discards = int(args[11])
  req.state_update.port_counters.out_errors = int(args[12])
  req.state_update.port_counters.in_fcs_errors = int(args[13])
  return req

@chassis_msg
def create_memory_error_alarm_msg(req, *args):
  req.state_update.memory_error_alarm.time_created = int(args[0])
  req.state_update.memory_error_alarm.description = args[1]
  req.state_update.memory_error_alarm.severity = common_pb2.Alarm.Severity.Value(args[2])
  req.state_update.memory_error_alarm.status = bool(args[3])
  return req

@chassis_msg
def create_flow_programming_exception_alarm_msg(req, *args):
  req.state_update.flow_programming_exception_alarm.time_created = int(args[0])
  req.state_update.flow_programming_exception_alarm.description = args[1]
  req.state_update.flow_programming_exception_alarm.severity = common_pb2.Alarm.Severity.Value(args[2])
  req.state_update.flow_programming_exception_alarm.status = bool(args[3])
  return req

@port_queue_msg
def create_port_qos_counters_msg(req, *args):
  req.state_update.port_qos_counters.queue_id = int(args[0])
  req.state_update.port_qos_counters.out_octets = int(args[1])
  req.state_update.port_qos_counters.out_pkts = int(args[2])
  req.state_update.port_qos_counters.out_dropped_pkts = int(args[3])
  return req

@node_msg
def create_node_packetio_debug_info_msg(req, *args):
  req.state_update.node_packetio_debug_info.debug_string = args[0]
  return req

@node_port_msg
def create_forwarding_viability_msg(req, *args):
  req.state_update.forwarding_viability.state = common_pb2.TrunkMemberBlockState.Value(args[0])
  return req

@node_port_msg
def create_health_indicator_msg(req, *args):
  req.state_update.health_indicator.state = common_pb2.HealthState.Value(args[0])
  return req

def init_args_parser():
  parser = argparse.ArgumentParser(description='Test Service CLI')
  parser.add_argument('--dry-run', help='Dry run',
                    action='store_const', const=True, default=False)
  parser.add_argument('--test-service-url', help='Test service address',
                      type=str, default='localhost:28010')
  cmd_parser = parser.add_subparsers(help='State to update', dest='state_to_update')

  for sub_cmd in node_port_states:
    sub_parser = cmd_parser.add_parser(sub_cmd, help='Change value of %s' % sub_cmd)
    sub_parser.add_argument('node', type=int, help='node id')
    sub_parser.add_argument('port', type=int, help='port id')
    sub_parser.add_argument('values', type=str, help='values to set', nargs='+')

  for sub_cmd in node_states:
    sub_parser = cmd_parser.add_parser(sub_cmd, help='Change value of %s' % sub_cmd)
    sub_parser.add_argument('node', type=int, help='node id')
    sub_parser.add_argument('values', type=str, help='values to set', nargs='+')

  for sub_cmd in chassis_states:
    sub_parser = cmd_parser.add_parser(sub_cmd, help='Change value of %s' % sub_cmd)
    sub_parser.add_argument('values', type=str, help='values to set', nargs='+')

  for sub_cmd in port_queue_states:
    sub_parser = cmd_parser.add_parser(sub_cmd, help='Change value of %s' % sub_cmd)
    sub_parser.add_argument('node', type=int, help='node id')
    sub_parser.add_argument('port', type=int, help='port id')
    sub_parser.add_argument('queue', type=int, help='queue id')
    sub_parser.add_argument('values', type=str, help='values to set', nargs='+')

  return parser.parse_args()

def main():
  args = init_args_parser()
  channel = grpc.insecure_channel(args.test_service_url)
  stub = dummy_test_pb2_grpc.TestStub(channel)

  req, resp = None, None

  func_name = 'create_%s_msg' % args.state_to_update

  func = globals().get(func_name, None)

  if not func:
    print 'Cannot find build function for %s update message' % args.state_to_update
    return

  if args.state_to_update in node_port_states:
    req = func(args.node, args.port, *args.values)

  if args.state_to_update in node_states:
    req = func(args.node, *args.values)

  if args.state_to_update in chassis_states:
    req = func(*args.values)

  if args.state_to_update in port_queue_states:
    req = func(args.node, args.port, args.queue, *args.values)

  print(req)
  if req is not None and not args.dry_run:
    resp = stub.DeviceStatusUpdate(req)
    print(resp)

if __name__ == '__main__':
  main()
