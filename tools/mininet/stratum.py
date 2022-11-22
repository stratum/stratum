# coding=utf-8
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

'''
This module contains two switch classes for Mininet: StratumBmv2Switch and StratumDpdkSwitch

Prerequisites
-------------
1. Docker- mininet+stratum_bmv2+stratum_dpdk image:
$ cd stratum
$ bazel build //stratum/hal/bin/bmv2:stratum_bmv2_deb
$ bazel build //stratum/hal/bin/tdi/dpdk:stratum_dpdk_deb
$ cp -f bazel-bin/stratum/hal/bin/bmv2/stratum_bmv2_deb.deb ./
$ cp -f bazel-bin/stratum/hal/bin/tdi/dpdk/stratum_dpdk_deb.deb ./
$ docker build -t opennetworking/mn-stratum -f tools/mininet/Dockerfile .

Usage
-----
From withing the Docker container, you can run Mininet using the following:
$ mn --custom /root/stratum.py --switch stratum-bmv2 --controller none
$ mn --custom /root/stratum.py --switch stratum-dpdk --controller none

Advanced Usage
--------------
You can use this class in a Mininet topology script by including:

from stratum import ONOSStratumBmv2Switch

You will probably need to update your Python path. From within the Docker image:

PYTHONPATH=$PYTHONPATH:/root ./<your script>.py

Notes
-----
This code has been adapted from the ONOSBmv2Switch class defined in the ONOS project
(tools/dev/mininet/bmv2.py).

'''

import json
import multiprocessing
import os
import socket
import sys
import threading
import time

from mininet.log import warn
from mininet.node import Switch, Host

DEFAULT_NODE_ID = 1
DEFAULT_CPU_PORT = 255
DEFAULT_PIPECONF = "org.onosproject.pipelines.basic"
STRATUM_BMV2 = 'stratum_bmv2'
STRATUM_DPDK = 'start-stratum.sh'
STRATUM_INIT_PIPELINE = '/root/dummy.json'
MAX_CONTROLLERS_PER_NODE = 10
BMV2_LOG_LINES = 5
# Incremental default gRPC port.
nextGrpcPort = 50001


def writeToFile(path, value):
    with open(path, "w") as f:
        f.write(str(value))


def pickUnusedPort():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(('localhost', 0))
    addr, port = s.getsockname()
    s.close()
    return port


def isPowerOfTwo(n):
    return (n != 0) and (n & (n-1) == 0)


def bmv2Watchdog(sw):
    try:
        writeToFile(sw.keepaliveFile,
                    "Remove this file to terminate %s" % sw.name)
        while True:
            if StratumBmv2Switch.mininet_exception == 1 \
                    or not os.path.isfile(sw.keepaliveFile):
                sw.stop()
                return
            if sw.stopped:
                return
            if sw.bmv2popen.poll() is None:
                # All good, no return code, still running.
                time.sleep(1)
            else:
                warn("\n*** WARN: switch %s died ☠️ \n" % sw.name)
                sw.printLog()
                print("-" * 80) + "\n"
                # Close log file, set as stopped etc.
                sw.stop()
                return
    except Exception as e:
        warn("*** ERROR: " + e.message)
        sw.stop()


def dpdkWatchdog(sw):
    try:
        writeToFile(sw.keepaliveFile,
                    "Remove this file to terminate %s" % sw.name)
        while True:
            if StratumDpdkSwitch.mininet_exception == 1 \
                    or not os.path.isfile(sw.keepaliveFile):
                sw.stop()
                return
            if sw.stopped:
                return
            if sw.dpdkpopen.poll() is None:
                # All good, no return code, still running.
                time.sleep(1)
            else:
                warn("\n*** WARN: switch %s died ☠️ \n" % sw.name)
                sw.printLog()
                print("-" * 80) + "\n"
                # Close log file, set as stopped etc.
                sw.stop()
                return
    except Exception as e:
        warn("*** ERROR: " + e.message)
        sw.stop()

class StratumBmv2Switch(Switch):
    # Shared value used to notify to all instances of this class that a Mininet
    # exception occurred. Mininet exception handling doesn't call the stop()
    # method, so the mn process would hang after clean-up since Bmv2 would still
    # be running.
    mininet_exception = multiprocessing.Value('i', 0)

    def __init__(self, name, json=STRATUM_INIT_PIPELINE, loglevel="warn",
                 cpuport=DEFAULT_CPU_PORT, pipeconf=DEFAULT_PIPECONF,
                 onosdevid=None, adminstate=True, grpcPort=None,
                 **kwargs):
        Switch.__init__(self, name, **kwargs)
        if grpcPort is not None:
          self.grpcPort = grpcPort
        else:
          global nextGrpcPort
          self.grpcPort = nextGrpcPort
          nextGrpcPort += 1
        self.cpuPort = cpuport
        self.json = json
        self.loglevel = loglevel
        self.tmpDir = '/tmp/%s' % self.name
        self.logfile = '%s/stratum_bmv2.log' % self.tmpDir
        self.netcfgFile = '%s/onos-netcfg.json' % self.tmpDir
        self.chassisConfigFile = '%s/chassis-config.txt' % self.tmpDir
        self.pipeconfId = pipeconf
        self.longitude = kwargs['longitude'] if 'longitude' in kwargs else None
        self.latitude = kwargs['latitude'] if 'latitude' in kwargs else None
        if onosdevid is not None and len(onosdevid) > 0:
            self.onosDeviceId = onosdevid
        else:
            # The "device:" prefix is required by ONOS.
            self.onosDeviceId = "device:%s" % self.name
        self.nodeId = DEFAULT_NODE_ID
        self.logfd = None
        self.bmv2popen = None
        self.stopped = True
        # In case of exceptions, mininet removes *.out files from /tmp. We use
        # this as a signal to terminate the switch instance (if active).
        self.keepaliveFile = '/tmp/%s-watchdog.out' % self.name
        self.adminState = "ENABLED" if adminstate else "DISABLED"

        # Remove files from previous executions
        self.cleanupTmpFiles()
        os.mkdir(self.tmpDir)

    def getOnosNetcfg(self):
        basicCfg = {
            "managementAddress": "grpc://localhost:%d?device_id=%d" % (
                self.grpcPort, self.nodeId),
            "driver": "stratum-bmv2",
            "pipeconf": self.pipeconfId
        }

        if self.longitude and self.latitude:
            basicCfg["longitude"] = self.longitude
            basicCfg["latitude"] = self.latitude

        netcfg = {
            "devices": {
                self.onosDeviceId: {
                    "basic": basicCfg
                }
            }
        }

        return netcfg

    def getChassisConfig(self):
        config = """description: "stratum_bmv2 {name}"
chassis {{
  platform: PLT_P4_SOFT_SWITCH
  name: "{name}"
}}
nodes {{
  id: {nodeId}
  name: "{name} node {nodeId}"
  slot: 1
  index: 1
}}\n""".format(name=self.name, nodeId=self.nodeId)

        for port_num, intf in sorted(self.intfs.items()):
            if intf.name == "lo":
                continue
            config = config + """singleton_ports {{
  id: {intfNumber}
  name: "{intfName}"
  slot: 1
  port: {intfNumber}
  channel: 1
  speed_bps: 10000000000
  config_params {{
    admin_state: ADMIN_STATE_{adminState}
  }}
  node: {nodeId}
}}\n""".format(intfName=intf.name, intfNumber=port_num, nodeId=self.nodeId, adminState=self.adminState)

        return config

    def start(self, controllers):

        if not self.stopped:
            warn("*** %s is already running!\n" % self.name)
            return

        writeToFile("%s/grpc-port.txt" % self.tmpDir, self.grpcPort)
        with open(self.chassisConfigFile, 'w') as fp:
            fp.write(self.getChassisConfig())
        with open(self.netcfgFile, 'w') as fp:
            json.dump(self.getOnosNetcfg(), fp, indent=2)

        args = [
            STRATUM_BMV2,
            '-device_id=%d' % self.nodeId,
            '-chassis_config_file=%s' % self.chassisConfigFile,
            '-forwarding_pipeline_configs_file=%s/pipe.txt' % self.tmpDir,
            '-persistent_config_dir=%s' % self.tmpDir,
            '-initial_pipeline=%s' % self.json,
            '-cpu_port=%s' % self.cpuPort,
            '-external_stratum_urls=0.0.0.0:%d' % self.grpcPort,
            '-local_stratum_url=localhost:%d' % pickUnusedPort(),
            '-max_num_controllers_per_node=%d' % MAX_CONTROLLERS_PER_NODE,
            '-write_req_log_file=%s/write-reqs.txt' % self.tmpDir,
            '-bmv2_log_level=%s' % self.loglevel,
        ]

        cmd_string = " ".join(args)

        try:
            # Write cmd_string to log for debugging.
            self.logfd = open(self.logfile, "w")
            self.logfd.write(cmd_string + "\n\n" + "-" * 80 + "\n\n")
            self.logfd.flush()

            self.bmv2popen = self.popen(cmd_string, stdout=self.logfd, stderr=self.logfd)
            print("⚡️ %s @ %d" % (STRATUM_BMV2, self.grpcPort))

            # We want to be notified if stratum_bmv2 quits prematurely...
            self.stopped = False
            threading.Thread(target=bmv2Watchdog, args=[self]).start()

        except Exception:
            StratumBmv2Switch.mininet_exception = 1
            self.stop()
            self.printLog()
            raise

    def printLog(self):
        if os.path.isfile(self.logfile):
            print("-" * 80)
            print("%s log (from %s):" % (self.name, self.logfile))
            with open(self.logfile, 'r') as f:
                lines = f.readlines()
                if len(lines) > BMV2_LOG_LINES:
                    print("...")
                for line in lines[-BMV2_LOG_LINES:]:
                    print(line.rstrip())

    def cleanupTmpFiles(self):
        self.cmd("rm -rf %s" % self.tmpDir)

    def stop(self, deleteIntfs=True):
        """Terminate switch."""
        self.stopped = True
        if self.bmv2popen is not None:
            if self.bmv2popen.poll() is None:
                self.bmv2popen.terminate()
                self.bmv2popen.wait()
            self.bmv2popen = None
        if self.logfd is not None:
            self.logfd.close()
            self.logfd = None
        Switch.stop(self, deleteIntfs)

class StratumDpdkSwitch(Switch):
    # Shared value used to notify to all instances of this class that a Mininet
    # exception occurred. Mininet exception handling doesn't call the stop()
    # method, so the mn process would hang after clean-up since dpdk would still
    # be running.
    mininet_exception = multiprocessing.Value('i', 0)

    def __init__(self, name, json=STRATUM_INIT_PIPELINE, loglevel="warn",
                 cpuport=DEFAULT_CPU_PORT, pipeconf=DEFAULT_PIPECONF,
                 onosdevid=None, adminstate=True, grpcPort=None,
                 **kwargs):
        Switch.__init__(self, name, **kwargs)
        if grpcPort is not None:
          self.grpcPort = grpcPort
        else:
          global nextGrpcPort
          self.grpcPort = nextGrpcPort
          nextGrpcPort += 1
        self.cpuPort = cpuport
        self.json = json
        self.loglevel = loglevel
        self.tmpDir = '/tmp/%s' % self.name
        self.logfile = '%s/stratum_dpdk.log' % self.tmpDir
        self.netcfgFile = '%s/onos-netcfg.json' % self.tmpDir
        self.chassisConfigFile = '%s/chassis-config.txt' % self.tmpDir
        self.pipeconfId = pipeconf
        self.longitude = kwargs['longitude'] if 'longitude' in kwargs else None
        self.latitude = kwargs['latitude'] if 'latitude' in kwargs else None
        if onosdevid is not None and len(onosdevid) > 0:
            self.onosDeviceId = onosdevid
        else:
            # The "device:" prefix is required by ONOS.
            self.onosDeviceId = "device:%s" % self.name
        self.nodeId = DEFAULT_NODE_ID
        self.logfd = None
        self.dpdkpopen = None
        self.stopped = True
        # In case of exceptions, mininet removes *.out files from /tmp. We use
        # this as a signal to terminate the switch instance (if active).
        self.keepaliveFile = '/tmp/%s-watchdog.out' % self.name
        self.adminState = "ENABLED" if adminstate else "DISABLED"
        self.socatpopen = []

        # Remove files from previous executions
        self.cleanupTmpFiles()
        os.mkdir(self.tmpDir)

    def getOnosNetcfg(self):
        basicCfg = {
            "managementAddress": "grpc://localhost:%d?device_id=%d" % (
                self.grpcPort, self.nodeId),
            "driver": "stratum-dpdk",
            "pipeconf": self.pipeconfId
        }

        if self.longitude and self.latitude:
            basicCfg["longitude"] = self.longitude
            basicCfg["latitude"] = self.latitude

        netcfg = {
            "devices": {
                self.onosDeviceId: {
                    "basic": basicCfg
                }
            }
        }

        return netcfg

    def getChassisConfig(self):
        config = """description: "stratum_dpdk {name}"
chassis {{
  platform: PLT_P4_DPDK_SOFTWARE_SWITCH
  name: "{name}"
}}
nodes {{
  id: {nodeId}
  name: "{name} node {nodeId}"
  slot: 1
  index: 1
}}\n""".format(name=self.name, nodeId=self.nodeId)
        print(sorted(self.intfs.items()))

        # Singleton port IDs in Stratum-DPDK start at 0 and are incremental.
        port_id = 0
        for port_num, intf in sorted(self.intfs.items()):
            if intf.name == "lo":
                continue
            config = config + """singleton_ports {{
  id: {intfNumber}
  name: "TAP{intfName}"
  slot: 1
  port: {port}
  speed_bps: 10000000000
  config_params {{
    admin_state: ADMIN_STATE_{adminState}
    pipeline_name: "pipe"
    port_type: PORT_TYPE_TAP
  }}
  node: {nodeId}
}}\n""".format(intfName=intf.name, intfNumber=port_id, port=port_id+1, nodeId=self.nodeId, adminState=self.adminState)
            port_id += 1

        if not isPowerOfTwo(port_id):
            print("Configured %s singleton ports, but Stratum-dpdk requires the"
                  " number of ports to be a power of two!" % port_id)
            sys.exit(1)

        return config

    def start(self, controllers):

        if not self.stopped:
            warn("*** %s is already running!\n" % self.name)
            return

        writeToFile("%s/grpc-port.txt" % self.tmpDir, self.grpcPort)
        with open(self.chassisConfigFile, 'w') as fp:
            fp.write(self.getChassisConfig())
        with open(self.netcfgFile, 'w') as fp:
            json.dump(self.getOnosNetcfg(), fp, indent=2)

        args = [
            STRATUM_DPDK,
            '-chassis_config_file=%s' % self.chassisConfigFile,
            '-forwarding_pipeline_configs_file=%s/pipe.txt' % self.tmpDir,
            '-persistent_config_dir=%s' % self.tmpDir,
            '-dpdk_switchd_cfg=/usr/share/stratum/dpdk_skip_p4.conf',
            '-dpdk_switchd_background=true',
            '-external_stratum_urls=0.0.0.0:%d' % self.grpcPort,
            '-local_stratum_url=localhost:%d' % pickUnusedPort(),
            '-max_num_controllers_per_node=%d' % MAX_CONTROLLERS_PER_NODE,
            '-write_req_log_file=%s/write-reqs.txt' % self.tmpDir,
        ]

        cmd_string = " ".join(args)

        try:
            # Write cmd_string to log for debugging.
            self.logfd = open(self.logfile, "w")
            self.logfd.write(cmd_string + "\n\n" + "-" * 80 + "\n\n")
            self.logfd.flush()

            self.dpdkpopen = self.popen(cmd_string, stdout=self.logfd, stderr=self.logfd)
            print("⚡️ %s @ %d" % ("stratum_dpdk", self.grpcPort))

            # Enable the DPDK TAP interfaces.
            time.sleep(1)
            print(sorted(self.intfs.items()))
            for port_num, intf in sorted(self.intfs.items()):
              if intf.name == "lo":
                  continue
              self.cmd("/sbin/ip l set up dev TAP%s" % intf.name)

              # Connect TAP and veth interfaces with socat.
              # We use larger buffers to improve throughput.
              socat_buf_size = 1<<20
              socat_buf_string = "rcvbuf=%i,sndbuf=%i" % (socat_buf_size, socat_buf_size)
              cmd = "/usr/bin/socat interface:%s,%s interface:TAP%s,%s" % (intf.name, socat_buf_string, intf.name, socat_buf_string)
              # print("running command: %s" % cmd)
              self.socatpopen.append(self.popen(cmd, stdout=self.logfd, stderr=self.logfd))

            # We want to be notified if stratum_dpdk quits prematurely...
            self.stopped = False
            threading.Thread(target=dpdkWatchdog, args=[self]).start()

        except Exception:
            StratumDpdkSwitch.mininet_exception = 1
            self.stop()
            self.printLog()
            raise

    def printLog(self):
        if os.path.isfile(self.logfile):
            print("-" * 80)
            print("%s log (from %s):" % (self.name, self.logfile))
            with open(self.logfile, 'r') as f:
                lines = f.readlines()
                if len(lines) > BMV2_LOG_LINES:
                    print("...")
                for line in lines[-BMV2_LOG_LINES:]:
                    print(line.rstrip())

    def cleanupTmpFiles(self):
        self.cmd("rm -rf %s" % self.tmpDir)

    def stop(self, deleteIntfs=True):
        """Terminate switch."""
        self.stopped = True
        if self.dpdkpopen is not None:
            if self.dpdkpopen.poll() is None:
                self.dpdkpopen.terminate()
                self.dpdkpopen.wait()
            self.dpdkpopen = None
        for p in self.socatpopen:
          p.terminate()
          p.wait()
        self.socatpopen = []

        if self.logfd is not None:
            self.logfd.close()
            self.logfd = None
        Switch.stop(self, deleteIntfs)

class NoOffloadHost(Host):
    def __init__(self, name, inNamespace=True, **params):
        Host.__init__(self, name, inNamespace=inNamespace, **params)

    def config(self, **params):
        r = super(Host, self).config(**params)
        for off in ["rx", "tx", "sg"]:
            cmd = "/sbin/ethtool --offload %s %s off" \
                  % (self.defaultIntf(), off)
            self.cmd(cmd)
        return r

class NoIpv6OffloadHost(NoOffloadHost):
    def __init__(self, name, inNamespace=True, **params):
        NoOffloadHost.__init__(self, name, inNamespace=inNamespace, **params)

    def config(self, **params):
        r = super(NoOffloadHost, self).config(**params)
        self.cmd("sysctl net.ipv6.conf.%s.disable_ipv6=1" % (self.defaultIntf()))
        return r


# Exports for bin/mn
switches = {
    'stratum-bmv2': StratumBmv2Switch,
    'stratum-dpdk': StratumDpdkSwitch,
}

hosts = {
    'no-offload-host': NoOffloadHost,
    'no-ipv6-host': NoIpv6OffloadHost
}
