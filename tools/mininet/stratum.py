#
# Copyright 2018-present Open Networking Foundation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
'''
This module contains a switch class for Mininet: StratumBmv2Switch

Prerequisites
-------------
1. Mininet
$ git clone https://github.com/mininet/mininet
$ cd mininet
$ util/install.sh -n

2. ONOS (optional; only required if you want to try the ONOS variant)
   You should run ONOS in a separate terminal window as we are invoking
   it in the foreground.
$ git clone https://github.com/opennetworkinglab/onos
$ cd onos
$ export ONOS_APPS=drivers.bmv2,proxyarp,lldpprovider,hostprovider,fwd
$ bazel run onos-local -- clean

If you need more instructions, you can find them here:
https://wiki.onosproject.org/display/ONOS/Developer+Quick+Start

3. Install BMv2 dependencies per
https://github.com/opennetworkinglab/stratum/blob/master/stratum/hal/bin/bmv2/README.md

4. Stratum with built BMv2 target
$ git clone https://github.com/opennetworkinglab/stratum
$ cd stratum
$ bazel build //stratum/hal/bin/bmv2:stratum_bmv2

Environment Variables
---------------------
STRATUM_ROOT must point to the Stratum workspace root if run outside of the Stratum workspace
ONOS_WEB_USER must be set if the default user (onos) is not available
ONOS_WEB_PASS must be set if the password has be changed from its default (rocks)

Usage
-----
You run Stratum with BMv2 using the following:
$ sudo mn --custom tools/mininet/stratum.py --switch stratum-bmv2 --controller none

There is also a variant that can be used with ONOS. It's exactly
the same, except after starting the switch it will push device
information to ONOS, so that ONOS can connect.

$ sudo mn --custom tools/mininet/stratum.py --switch onos-stratum-bmv2 --controller remote[,ip=<ONOS IP>]

If you've installed BMv2 in a custom install path (e.g. $BMV2_INSTALL), then you'll need to update the LD_LIBRARY_PATH when you run Mininet:
$ sudo env LD_LIBRARY_PATH=$BMV2_INSTALL/lib mn [... rest of the command]

Advanced Usage
--------------
You can use either of these classes in a Mininet topology script by including:

from stratum import ONOSStratumBmv2Switch
# or
from stratum import StratumBmv2Switch

You will probably need to update your Python path, unless you've installed stratum.py
sudo -E env PYTHONPATH=$PYTHONPATH:$STRATUM_ROOT/tools/mininet ./<your script>.py

Notes
-----
This code has been adapted from the ONOSBmv2Switch class defined in the ONOS project
(tools/dev/mininet/bmv2.py).

'''

import json
import multiprocessing
import os
import random
import re
import socket
import sys
import threading
import time
import urllib2
from contextlib import closing
from subprocess import call, check_output

from mininet.log import info, warn
from mininet.node import Switch

def getStratumRoot():
  if 'STRATUM_ROOT' in os.environ:
    return os.environ['STRATUM_ROOT']
  # Check to see if we are running from the stratum workspace
  NULL = open(os.devnull, 'w')
  if not call(['which', 'bazel'], stdout=NULL):
    root = check_output(["bazel", "info", "workspace"], stderr=NULL).rstrip()
    #sanity check the workspace
    if os.path.isdir(os.path.join(root, "stratum")):
      return root
  # Fallback
  print("STRATUM_ROOT is not defined")
  sys.exit(1)

STRATUM_ROOT = getStratumRoot()
STRATUM_BINARY = STRATUM_ROOT + '/bazel-bin/stratum/hal/bin/bmv2/stratum_bmv2'
INITIAL_PIPELINE = STRATUM_ROOT + '/stratum/hal/bin/bmv2/dummy.json'
SWITCH_START_TIMEOUT = 5  # seconds
DEFAULT_DEVICE_ID = 1

def pickUnusedPort():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(('localhost', 0))
    addr, port = s.getsockname()
    s.close()
    return port

def writeToFile(path, value):
    with open(path, "w") as f:
        f.write(str(value))

class StratumBmv2Switch(Switch):
    """Stratum BMv2 switch"""

    def __init__(self, name, json=None, loglevel="warn",
                 grpcport=None, cpuport=255, **kwargs):
        Switch.__init__(self, name, **kwargs)
        self.grpcPort = grpcport
        self.cpuPort = cpuport
        self.json = json
        self.loglevel = loglevel
        self.logfile = '/tmp/stratum-bmv2-%s.log' % self.name
        self.logfd = None
        self.bmv2popen = None
        self.stopped = False

        # Remove files from previous executions
        self.cleanupTmpFiles()

    def start(self, controllers):
        config_dir = '/tmp/stratum-bmv2-%s' % self.name

        if self.grpcPort is None:
            self.grpcPort = pickUnusedPort()

        args = [
          STRATUM_BINARY,
          '-device_id=%d' % DEFAULT_DEVICE_ID,
          '-forwarding_pipeline_configs_file=%s/config.txt' % config_dir,
          '-persistent_config_dir=' + config_dir,
          '-initial_pipeline=' + INITIAL_PIPELINE,
          '-cpu_port=%s' % self.cpuPort,
          '-external_hercules_urls=0.0.0.0:%s' % self.grpcPort,
        ]
        for port, intf in self.intfs.items():
            if not intf.IP():
                args.append('%d@%s' % (port, intf.name))

        cmdString = " ".join(args)

        info("\nStarting Stratum BMv2 target: %s\n" % cmdString)

        writeToFile("/tmp/stratum-bmv2-%s-grpc-port" % self.name, self.grpcPort)

        try:
            # Start the switch
            self.logfd = open(self.logfile, "w")
            self.bmv2popen = self.popen(cmdString,
                                        stdout=self.logfd,
                                        stderr=self.logfd)
            self.waitBmv2Start()
        except Exception:
            self.killBmv2()
            raise

    def waitBmv2Start(self):
        # Wait for switch to open gRPC port.
        # Include time-out just in case something hangs.
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        endtime = time.time() + SWITCH_START_TIMEOUT
        while True:
            result = sock.connect_ex(('127.0.0.1', self.grpcPort))
            if result == 0:
                # The port is open. Let's go! (Close socket first)
                sock.close()
                break
            # Port is not open yet. If there is time, we wait a bit.
            if endtime > time.time():
                time.sleep(0.1)
            else:
                # Time's up.
                raise Exception("Switch did not start before timeout")

    def killBmv2(self, log=False):
        if self.bmv2popen is not None:
            self.bmv2popen.kill()
        if self.logfd is not None:
            if log:
                self.logfd.write("*** PROCESS TERMINATED BY MININET ***\n")
            self.logfd.close()

    def cleanupTmpFiles(self):
        self.cmd("rm -f /tmp/stratum-bmv2-%s-*" % self.name)

    def stop(self, deleteIntfs=True):
        """Terminate switch."""
        self.stopped = True
        self.killBmv2(log=True)
        Switch.stop(self, deleteIntfs)


class ONOSStratumBmv2Switch(StratumBmv2Switch):
    """Stratum BMv2 switch with ONOS support"""

    def __init__(self, name, netcfg=True, pipeconf="",
                 gnmi=False, portcfg=True, onosdevid=None, **kwargs):
        StratumBmv2Switch.__init__(self, name, **kwargs)
        self.netcfg = netcfg in (True, '1', 'true', 'True')
        self.netcfgfile = '/tmp/stratum-bmv2-%s-netcfg.json' % self.name
        self.pipeconfId = pipeconf
        self.injectPorts = portcfg in (True, '1', 'true', 'True')
        self.withGnmi = gnmi in (True, '1', 'true', 'True')
        self.longitude = kwargs['longitude'] if 'longitude' in kwargs else None
        self.latitude = kwargs['latitude'] if 'latitude' in kwargs else None
        if onosdevid is not None and len(onosdevid) > 0:
            self.onosDeviceId = onosdevid
        else:
            self.onosDeviceId = "device:stratum-bmv2:%s" % self.name


    def getDeviceConfig(self, srcIP):
        basicCfg = {
            "driver": "bmv2" # Stratum also uses the bmv2 driver
        }

        if self.longitude and self.latitude:
            basicCfg["longitude"] = self.longitude
            basicCfg["latitude"] = self.latitude

        cfgData = {
            "generalprovider": {
                "p4runtime": {
                    "ip": srcIP,
                    "port": self.grpcPort,
                    "deviceId": DEFAULT_DEVICE_ID,
                    "deviceKeyId": "p4runtime:%s" % self.onosDeviceId
                }
            },
            "piPipeconf": {
                "piPipeconfId": self.pipeconfId
            },
            "basic": basicCfg
        }

        if self.withGnmi:
            cfgData["generalprovider"]["gnmi"] = {
                "ip": srcIP,
                "port": self.grpcPort
            }

        if self.injectPorts:
            portData = {}
            portId = 1
            for intfName in self.intfNames():
                if intfName == 'lo':
                    continue
                portData[str(portId)] = {
                    "number": portId,
                    "name": intfName,
                    "enabled": True,
                    "removed": False,
                    "type": "copper",
                    "speed": 10000
                }
                portId += 1

            cfgData['ports'] = portData

        return cfgData

    def getSourceIp(self, dstIP):
        """
        Queries the Linux routing table to get the source IP that can talk with
        dstIP, and vice versa.
        """
        ipRouteOut = self.cmd('ip route get %s' % dstIP)
        r = re.search(r"src (\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})", ipRouteOut)
        return r.group(1) if r else None


    def start(self, controllers):
        """
        Starts the switch, then notifies ONOS about the new device via Netcfg.
        """
        StratumBmv2Switch.start(self, controllers)
        
        if not self.netcfg:
            # Do not push config to ONOS.
            return

        controllerIP = self.controllerIp(controllers)
        srcIP = self.getSourceIp(controllerIP)
        if not srcIP:
            warn("*** WARN: unable to get switch IP address, won't do netcfg\n")
            return

        cfgData = {
            "devices": {
                self.onosDeviceId: self.getDeviceConfig(srcIP)
            }
        }
        with open(self.netcfgfile, 'w') as fp:
            json.dump(cfgData, fp, indent=4)

        # Build netcfg URL
        url = 'http://%s:8181/onos/v1/network/configuration/' % controllerIP
        # Instantiate password manager for HTTP auth
        pm = urllib2.HTTPPasswordMgrWithDefaultRealm()
        user = os.environ['ONOS_WEB_USER'] if 'ONOS_WEB_USER' in os.environ else 'onos'
        password = os.environ['ONOS_WEB_PASS'] if 'ONOS_WEB_PASS' in os.environ else 'rocks'
        pm.add_password(None, url, user, password)
        urllib2.install_opener(urllib2.build_opener(
            urllib2.HTTPBasicAuthHandler(pm)))
        try:
            # Push config data to controller
            req = urllib2.Request(url, json.dumps(cfgData),
                              {'Content-Type': 'application/json'})
            f = urllib2.urlopen(req)
            print f.read()
            f.close()
        except urllib2.URLError as e:
            warn("*** WARN: unable to push config to ONOS (%s)\n" % e.reason)

    @staticmethod
    def controllerIp(controllers):
        try:
            clist = controllers[0].nodes()
        except AttributeError:
            clist = controllers
        assert len(clist) > 0
        return random.choice(clist).IP()

# Exports for bin/mn
switches = {
  'stratum-bmv2': StratumBmv2Switch,
  'onos-stratum-bmv2': ONOSStratumBmv2Switch
}
