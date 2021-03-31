# coding=utf-8
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

'''
This module contains a switch class for Mininet: StratumTofinoModel

Usage
-----
From withing the Docker container, you can run Mininet using the following:
$ mn --custom tools/mininet/tofino-model.py --switch stratum-t-model --link tofino
'''

import json
import multiprocessing
import os
import socket
import threading
import time

from mininet.log import warn
from mininet.node import Switch, Host
from mininet.link import Link

TOFINO_MODEL_BIN = 'tofino-model'
STRATUM_BIN = 'stratum_bf'
# DEV_STRATUM_BIN = '/root/stratum/bazel-bin/stratum/hal/bin/barefoot/stratum_bf'

def watchdog(sw):
    try:
        with open(sw.keepalive, "w") as f:
            f.write(f"Remove this file to terminate {sw.name}")
        while True:
            if StratumTofinoModel.mininet_exception == 1 \
                or not os.path.isfile(sw.keepalive):
                sw.stop()
                return
            if sw.stopped:
                return
            # protect against already crashed process, will be none and fail to poll()
            if sw.modelProc_h is not None and sw.stratumProc_h is not None:
                # poll open proc handles
                if sw.modelProc_h.poll() is None and sw.stratumProc_h.poll() is None:
                    time.sleep(1)
                else:
                    warn("\n*** WARN: switch %s died ☠️ \n" % sw.name)
                    sw.stop()
                    return
    except Exception as e:
        warn("*** ERROR: " + e.message)
        sw.stop()

class StratumTofinoModel(Switch):
    mininet_exception = multiprocessing.Value('i', 0)
    nextNodeId = 0

    def __init__(self, name, inNamespace = True, 
                 sdeInstall="/root/sde/install", 
                 bfSwitchdConfig="/etc/stratum/tofino_skip_p4.conf", **kwargs):
        Switch.__init__(self, name, inNamespace = True, **kwargs)
        self.sdeInstall = sdeInstall
        self.bfSwitchdConfig = bfSwitchdConfig
        self.nodeId = StratumTofinoModel.nextNodeId * 64
        StratumTofinoModel.nextNodeId += 1

        self.tmpDir = '/tmp/%s' % self.name
        self.chassisConfigFile = '%s/chassis-config.txt' % self.tmpDir
        self.portVethFile = '%s/ports.json' % self.tmpDir

        # process handles for tofino model and stratum
        self.modelProcHandle = None
        self.stratumProcHandle = None
        # !(run) or run ? boolean bit
        self.stopped = True

        # In case of exceptions, mininet removes *.out files from /tmp. We use
        # this as a signal to terminate the switch instance (if active).
        self.keepalive = '/tmp/%s-watchdog.out' % self.name
        # Remove files from previous executions
        self.cmd("rm -rf %s" % self.tmpDir)
        os.mkdir(self.tmpDir)


    def stop(self, deleteIntfs=True):
        """Terminate switch."""
        self.stopped = True
        if self.anyProcHandleLive():
            self.tryTerminate()
            self.modelProcHandle = None
            self.stratumProcHandle = None
        Switch.stop(self, deleteIntfs)

    def start(self, controllers):

        if not self.stopped: return

        #TODO this could prob be replaced by setting an ip in the parent.init
        # or something to trigger ip link up...
        self.cmd("/usr/sbin/ip l set dev lo up")

        with open(self.chassisConfigFile, 'w') as fp:
            fp.write(self.getChassisConfig())
        with open(self.portVethFile, 'w') as fp:
            fp.write(self.getPortVethMap())
        tof_cmd_string = " ".join(
            [
                TOFINO_MODEL_BIN,
                f'--p4-target-config={self.bfSwitchdConfig}',
                f'-f{self.portVethFile}'
            ]
        )
        stratum_cmd_string = " ".join(
            [
                STRATUM_BIN,
                f'-chassis_config_file={self.chassisConfigFile}',
                '-bf_sim',
                '-enable_onlp=false',
                '-bf_switchd_background=false',
                '-v=6',
                '-alsologtostderr=true',
                f'-bf_sde_install={self.sdeInstall}',
                f'-bf_switchd_cfg={self.bfSwitchdConfig}',
            ]
        )

        try:
            # Write cmd_string to log for debugging.
            self.modelLogHandle = open(f'{self.tmpDir}/tofino_model_process.log' , "w")
            self.modelLogHandle.write(tof_cmd_string + "\n\n" + "-" * 80 + "\n\n")
            self.modelLogHandle.flush()
            self.stratumLogHandle = open(f'{self.tmpDir}/stratum_process.log', "w")
            self.stratumLogHandle.write(stratum_cmd_string + "\n\n" + "-" * 80 + "\n\n")
            self.stratumLogHandle.flush()

            self.modelProcHandle = self.popen(tof_cmd_string, stdout=self.t_logfd, stderr=self.t_logfd)
            self.stratumProcHandle = self.popen(stratum_cmd_string, stdout=self.s_logfd, stderr=self.s_logfd)

            # We want to be notified if processes quits prematurely...
            self.stopped = False
            threading.Thread(target=watchdog, args=[self]).start()

        except Exception:
            StratumTofinoModel.mininet_exception = 1
            self.stop()
            raise

    def getPortVethMap(self):
        intf_number = 1
        portsVeth = list()
        for intf_name in self.intfNames():
            if intf_name == 'lo': continue
            portsVeth.append(
                {
                    "device_port": intf_number,
                    "veth1": int(intf_name[4:]),
                    "veth2": int(intf_name[4:])+100
                }
            )
            intf_number+=1

        data =  { "PortToVeth": portsVeth }
        return json.dumps(data, indent=4) + "\n"

    def getChassisConfig(self):
        config = """description: "chassis config bf tofino model {name}"
chassis {{
  platform: PLT_GENERIC_BAREFOOT_TOFINO
  name: "{name}"
}}
nodes {{
  id: {nodeId}
  name: "{name}"
  slot: 1
  index: 1
}}\n""".format(name=self.name, nodeId=self.nodeId)

        intf_number = 1
        for intf_name in self.intfNames():
            if intf_name == 'lo':
                continue
            config = config + """singleton_ports {{
  id: {intfNumber}
  name: "{intfName}"
  slot: 1
  port: {intfNumber}
  channel: 1
  speed_bps: 10000000000
  config_params {{
    admin_state: ADMIN_STATE_ENABLED
  }}
  node: {nodeId}
}}\n""".format(intfName=intf_name, intfNumber=intf_number, nodeId=self.nodeId)
            intf_number += 1

        return config

    def anyProcHandleLive(self):
        return self.modelProcHandle is not None or self.stratumProcHandle is not None

    def tryTerminate(self):
        if self.modelProcHandle is not None:
            if self.modelProcHandle.poll() is None:
                self.modelProcHandle.terminate()
                self.modelProcHandle.wait()
        if self.stratumProc_h is not None:
            if self.stratumProcHandle.poll() is None:
                self.stratumProcHandle.terminate()
                self.stratumProcHandle.wait()
        if self.modelLogHandle is not None:
            self.modelLogHandle.close()
            self.modelLogHandle = None
        if self.stratumLogHandle is not None:
            self.stratumLogHandle.close()
            self.stratumLogHandle = None

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

class TofinoLink(Link):
    "Link with tofino port numbering i.e. only allows veth%d"
    def __init__(self, *args, **kwargs):
        Link.__init__(self, *args, **kwargs)

    def intfName(self, node, n):
        "Construct a tofmodel veth interface vethN for interface n."
        # skip over Hosts as the nodeId not present
        if isinstance(node, Host):
            return node.name + '-eth' + repr( n )
        return f"veth{node.nodeId-1+n}"

# Exports for bin/mn
switches = {'stratum-t-model': StratumTofinoModel}

hosts = {'no-offload-host': NoOffloadHost}

LINKS = { 'default': Link,  # Note: overridden below
        #   'tc': TCLink,
        #   'tcu': TCULink,
        #   'ovs': OVSLink,
          'tofino': TofinoLink}
