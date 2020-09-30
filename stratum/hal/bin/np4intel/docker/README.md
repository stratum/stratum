<!--
Copyright 2020 Dell EMC

SPDX-License-Identifier: Apache-2.0
-->
# NP4 Intel Docker Stratum runtime

This document details the steps needed in order to run a P4 pipeline
on an Intel PAC N3000 card using the Netcope compiler and SDK.

# Setting up the host server with the FPGA card

The Stratum container will run on the server with the FPGA card in it. However
before running the Stratum container there are some things that need to be
done on the OS first.

You should login to the Intel support site as per the following link and
install the card and the OPAE software as per the user guide here: https://www.intel.com/content/www/us/en/programmable/documentation/xgz1560360700260.html

The following links point to software bundles referenced in the install guide.

* `Runtime installer`: https://www.intel.com/content/www/us/en/programmable/f/download/accelerator/license-agreement-pac-n3000.html?swcode=WWW-SWD-DCP-N3000-RTE-11
* `8x10G configuration installer`: https://www.intel.com/content/www/us/en/programmable/f/download/accelerator/license-agreement-pac-n3000.html?swcode=WWW-SWD-DCP-N3000-136-CFG-810G.

At this point your Intel PAC N3000 FPGA card should be ready for you to
install the FPGA pipeline firmware (i.e. output from the Netcope Portal
compiler).

# Compiling a P4 Pipeline for the FPGA card

1. You will first need to contact your Netcope representative to get a
license to use both the Netcope Portal compiler and the Netcope SDK before
proceeding further.

2. Once you have your login details to the Netcope Portal you should be able
to login to the portal here: https://np4-intel.netcope.com

3. At this point you should follow the Netcope Portal instructions to compile
and synthesis the FPGA firmware.  Make sure that you select the correct
SDK version and FPGA card model in order for the compilation to be
compatible with the Intel PAC N3000 card that you are using (i.e. there
are different versions of the Intel PAC N3000 card: 2x2x25G and 2x4x10G).

4. Once the compilation task finishes you should be able to download the
resulting FPGA firmware and put it on the same server that the Intel PAC
N3000 card is plugged into.

5. The resulting FPGA firmware (i.e. named fw.bin in the example script below)
now needs to be signed and then updated to the FPGA card (i.e. N3000).

The following script can be used to achieve this task.

```
#/bin/bash -

# Creating an unsigned image from the Netcope firmware
python3.6 /usr/local/bin/PACSign SR -t UPDATE -H openssl_manager \
    -y -i fw.bin -o fw_signed.bin

# Flash card
sudo fpgasupdate fw_signed.bin

# Reboot card
sudo rsu bmcimg
```

6. The final step is to produce the p4info file that is used by the
controller and stratum to communicate over the P4 Runtime interface.
This is normally done by compiling the P4 pipeline using the p4.org
p4c compiler.

This can be done by passing in the the P4 pipeline as follows:

```
$ p4c-bm2-ss --arch v1model -o ${OUT_DIR}/bmv2.json    \
             --std p4-14 -DTARGET_BMV2 -DCPU_PORT=128  \
             -DWITH_PORT_COUNTER --Werror=legacy       \
             --p4runtime-files ${OUT_DIR}/p4info.txt   \
             top.p4
```

where the file `top.p4` is the top level P4 pipeline file from which all
the others are included (note: that the Netcope compiler requires that the
top level P4 file be named `top.p4`).

*Note:* the Stratum control plane port (i.e. CPU_PORT is set to 128 and
is the P4 port number of the first DPDK port).

*Note:* if the `@pragma use_external_mem` statement is used in the P4
pipeline then this must be commented out in order for the p4c compile
above to successfully complete.

7. Take the compiler generated p4info.txt file and parse it through a
python translator that fixes some of the problems between P4-14 and the
generated p4info.txt file for the generated Netcope firmware.

```
cd ~/stratum/stratum/hal/bin/np4intel/docker
export PYTHONPATH=./py_out
./scripts/tr_p4info.py --p4info ${OUT_DIR}/p4info.txt \
    --platform np4 --p4info-out configs/p4info_np4.txt
```

This will put the resulting NP4 specific p4info file into the configs
directory ready to be used by the Stratum startup scripts.

# Installing the Netcope SDK on the host server

*Note:* you'll need to contact your Netcope representative to obtain access
        to the Netcope SDK software bundle tarball.

1. Untar the Netcope SDK tarball into a directory

```
cd ~/
tar xzf np4_intel_4_7_1-1.tgz
```

2. Go to the centos driver folder in the Netcope NP4 bundle cloned in the
previous step and run the install script.

```
cd ~/np4_intel_4_7_1-1/centos
sudo bash np4-intel-n3000-4.7.1-1-centos.bin
```

*Note:* a full readme is contained in the ~/np4_intel_4_7_1-1/centos directory

# Setting up the docker environment

All that should be needed to create the stratum NP4 docker image is a
system running docker.  Refer to your OS for these instructions.

# Building the Stratum NP4 runtime container image

1. Change to docker directory of stratum for NP4 Intel

```
cd ~/stratum/stratum/hal/bin/np4intel/docker
```

2. run the container build script passing the Netcope Ubuntu SDK binary as
   an argument to the command line.

```
./build-stratum-np4intel-container.sh ~/np4_intel_4_7_1-1/ubuntu/np4-intel-n3000-4.7.1-1-ubuntu.bin
```

# Running the Stratum container

The following instructions detail how to configure and run the Stratum
NP4 Intel container on a CentOS host OS.

1. Update the configurations

Take a look in the configs directory and update the configs as necessary.

```
cd ~/stratum/stratum/hal/bin/np4intel/docker/configs
```

The files in the directory are as follows:
- p4info_np4.txt: the P4 Info file (produced in the P4 pipeline compilation)
- dpdk_config.pb.txt: Is a config file that passes DPDK EAL init arguments to the DPDK libraries and can also disable DPDK all together if required (i.e. for testing)
- pipeline_config.pb.txt: is the forwarding pipeline config file that is autogenerated at run time via the stratum-entrypoint.sh script
- stratum.flags: is the flags that are passed to stratum on start up
- device_configs/device_config_node_id_1.pb.txt: is the device config file for node id 1, each FPGA device is given a unique "node id" and then the parameters in this file are used to initialise the NP4 and DPKD interfaces.

2. Run the Stratum NP4 Intel container

```
cd ~/stratum/stratum/hal/bin/np4intel/docker
./start-stratum-container.sh

```

The stratum binary should now be running, log files can be found in the  logs directory (i.e. logs/stratum_np4*).

3. Running a bash cli in the container

By default the start script will run the stratum NP4 Intel binary but
an optional argument "--bash" can be passed to instead run a bash cli to
allow you to manually start stratum using the stratum-entrypoint.sh script.

```
./start-stratum-container.sh --bash
```

4. Running the debug version of the Stratum binary

Similar to the `--bash` parameter above, the `--debug` parameter can be
passed to the start up script that will then run the debug version of
the stratum binary with all the symbol tables included.  This can be used
to debug problems using gdb and the source code.

```
./start-stratum-container.sh --debug
```

5. At this point the ptf framework can be used to load tables or a real
control plane can be started to talk to Stratum on port 9559. If it's
on the local machine it will likely be localhost:28000 (depends on your
docker network settings).  You will need to configure the network to
redirect packets to this container if your trying to access stratum off
box (i.e. using ipchains).


