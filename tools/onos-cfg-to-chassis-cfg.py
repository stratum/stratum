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

'''
This script generates chassis configs from ONOS netcfg file.
by default, every Stratum device for ONOS has only one node with sevaral ports
'''

import re
import sys
import grpc
import json
import argparse

import common_pb2

DEFAULT_STRATUM_NODE_SLOT = 1
DEFAULT_STRATUM_NODE_INDEX = 1

parser = argparse.ArgumentParser(description='Chassis config generator')
parser.add_argument('--out-dir', help='Output directory', type=str, default='.')
parser.add_argument('onosNetcfg', help='ONOS network config file', type=str)
args = parser.parse_args()

def generateChassisConfig(deviceId, deviceCfg):
  if 'generalprovider' not in deviceCfg:
    return None
  chassisConfig = common_pb2.ChassisConfig()
  chassisConfig.description = "Stratum device %s" % deviceId
  # TODO(unknown): Sets correct platform based on device config
  chassisConfig.chassis.platform = common_pb2.PLT_P4_SOFT_SWITCH
  chassisConfig.chassis.name = deviceId
  node = chassisConfig.nodes.add()
  node.id = deviceCfg['generalprovider']['p4runtime']['deviceId']
  node.name = "%s node %d" % (deviceId, node.id)
  node.slot = DEFAULT_STRATUM_NODE_SLOT
  node.index = DEFAULT_STRATUM_NODE_INDEX

  for portId in deviceCfg['ports']:
    portCfg = deviceCfg['ports'][portId]
    portName = portCfg['name']
    # Port Name should be PortNo/Channel
    if not re.search('^\d+/\d+$', portName):
      print 'Invalid port name %s' % portName
      continue
    portNo = int(portName.split('/')[0])
    portChannel = int(portName.split('/')[1]) + 1 # convert channel to 1-base

    singletonPort = chassisConfig.singleton_ports.add()
    singletonPort.id = portCfg['number']
    singletonPort.name = portName
    singletonPort.port = portNo
    singletonPort.channel = portChannel
    singletonPort.speed_bps = portCfg['speed'] * 10**6 # Mbps based
    singletonPort.node = node.id

  return chassisConfig

def main():
  with open(args.onosNetcfg) as onosNetcfgFile:
    onosNetcfg = json.load(onosNetcfgFile)

    if 'devices' not in onosNetcfg:
      print 'Cannot find device config in file %s' % args.onosNetcfg
      return
    for deviceId in onosNetcfg['devices']:
      deviceCfg = onosNetcfg['devices'][deviceId]
      chassisCfg = generateChassisConfig(deviceId, deviceCfg)
      if not chassisCfg:
        continue
      fileToWrite = '%s/%s-chassis-cfg.pb.txt' % (args.out_dir, deviceId.replace(':', '-'))
      with open(fileToWrite, 'w') as chassisFile:
        chassisFile.write(str(chassisCfg))

if __name__ == "__main__":
  main()