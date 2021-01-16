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

# WIP
# 
#  mininet code coming up clean but still few config file logic handling needed
# testing using  from root stratum dir
#
# mn --custom tools/mininet/tofino-model.py \
#   --host no-offload-host --controller none \
#   --switch stratum-t-model  --link tofino
# 
# WIP



def watchdog(sw):
    try:
        while True:
            if StratumTofinoModel.mininet_exception == 1:
                sw.stop()
                return
            if sw.stopped:
                return
    except Exception as e:
        warn("*** ERROR: " + e.message)
        sw.stop()

class StratumTofinoModel(Switch):
    mininet_exception = multiprocessing.Value('i', 0)
    nextNodeId = 0

    def __init__(self, name, inNamespace = True, **kwargs):
        Switch.__init__(self, name, inNamespace = True, **kwargs)
        self.nodeId = StratumTofinoModel.nextNodeId * 64
        StratumTofinoModel.nextNodeId += 1

        self.tmpDir = '/tmp/%s' % self.name
        self.chassisConfigFile = '%s/chassis-config.txt' % self.tmpDir
        self.portVethFile = '%s/ports.json' % self.tmpDir

        # process handles for tofino model and stratum
        self.t_modelProc_h = None
        self.stratumProc_h = None
        # !(run) or run ? boolean bit
        self.stopped = True

        # Remove files from previous executions
        self.cmd("rm -rf %s" % self.tmpDir)
        os.mkdir(self.tmpDir)


    def stop(self, deleteIntfs=True):
        """Terminate switch."""
        self.stopped = True
        if self.proc_handle_live():
            self.try_terminate()
            self.t_modelProc_h = None
            self.stratumProc_h = None
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
                '--p4-target-config=/etc/stratum/tofino_skip_p4.conf',
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
                '-bf_sde_install=/root/sde/install',
                '-bf_switchd_cfg=/etc/stratum/tofino_skip_p4.conf',
            ]
        )

        try:
            # Write cmd_string to log for debugging.
            self.t_logfd = open(f'{self.tmpDir}/tofino_model_process.log' , "w")
            self.t_logfd.write(tof_cmd_string + "\n\n" + "-" * 80 + "\n\n")
            self.t_logfd.flush()
            self.s_logfd = open(f'{self.tmpDir}/stratum_process.log', "w")
            self.s_logfd.write(stratum_cmd_string + "\n\n" + "-" * 80 + "\n\n")
            self.s_logfd.flush()

            self.t_modelProc_h = self.popen(tof_cmd_string, stdout=self.t_logfd, stderr=self.t_logfd)
            self.stratumProc_h = self.popen(stratum_cmd_string, stdout=self.s_logfd, stderr=self.s_logfd)

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
        return json.dumps(data, indent=4)+"\n"

    def getChassisConfig(self):
        config = """description: "chassis config bf tofino model {name}"
chassis {{
  platform: PLT_GENERIC_BAREFOOT_TOFINO
  name: "{name}"
}}
nodes {{
  id: {nodeId}
  name: "{name} node {nodeId}"
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

    def proc_handle_live(self):
        if self.t_modelProc_h is not None: return True
        if self.stratumProc_h is not None: return True
        return False

    def try_terminate(self):
        if self.t_modelProc_h is not None:
            if self.t_modelProc_h.poll() is None:
                self.t_modelProc_h.terminate()
                self.t_modelProc_h.wait()
        if self.stratumProc_h is not None:
            if self.stratumProc_h.poll() is None:
                self.stratumProc_h.terminate()
                self.stratumProc_h.wait()
        if self.t_logfd is not None:
            self.t_logfd.close()
            self.t_logfd = None
        if self.s_logfd is not None:
            self.s_logfd.close()
            self.s_logfd = None

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

class TofinoLink( Link ):
    "Link with tofino port numbering i.e. only allows veth%d"
    def __init__( self, *args, **kwargs):
        Link.__init__( self, *args, **kwargs)

    def intfName( self, node, n ):
        "Construct a tofmodel veth interface vethN for interface n."
        # skip over Hosts as the nodeId not present
        if isinstance(node, Host):
            return node.name + '-eth' + repr( n )
        return f"veth{node.nodeId-1+n}"

# Exports for bin/mn
switches = {'stratum-t-model': StratumTofinoModel}

hosts = {'no-offload-host': NoOffloadHost}

# links = {'tofinolink': TofinoLink}
LINKS = { 'default': Link,  # Note: overridden below
        #   'tc': TCLink,
        #   'tcu': TCULink,
        #   'ovs': OVSLink,
          'tofino': TofinoLink}