<!--
Copyright 2020 Technische Universität Darmstadt
Copyright 2020-present Open Networking Foundation

SPDX-License-Identifier: Apache-2.0
-->
# Installation guide:<br/>ONOS + Stratum + Barefoot Tofino switch

These instructions have been tested on an [Accton Wedge 100BF-32X](https://www.edge-core.com/productsInfo.php?cls=1&cls2=180&cls3=181&id=335), yet should work on all Tofino-based switches.

The coarse steps are as follows:
- On host: build ONL
- On switch: install built ONL
- On switch: start Stratum
- On host: start ONOS
- On host: build `fabric-tofino` ONOS app
- On host: install and activate `fabric-tofino` app in ONOS
- On host: register switch in ONOS using `netcfg`


## Build or download ONL

You have two choices to obtain an ONL image.

### 1) Building ONL yourself

ONL can be built wherever you like.

```
git clone --depth 1  --single-branch --branch onf-ONLPv2 https://github.com/opennetworkinglab/OpenNetworkLinux
cd OpenNetworkLinux
VERSION=9 make docker
# or
sudo VERSION=9 make docker

find -type f -name "*INSTALLER"
```
There should be an installer file with a filename similar to:

`./RELEASE/stretch/amd64/ONL-ONLPv2_ONL-OS_2020-06-22.1054-bb20e13_AMD64_INSTALLED_INSTALLER`.

This is the ONL image you have to install on the switch in the next step.

### 2) Using a pre-built ONL (recommended)

You can also download a pre-built ONL from [here](https://github.com/opennetworkinglab/OpenNetworkLinux/releases).

:warning: You have to build or use the ONL from the fork mentioned above - if you use a pre-built installer from the official [OpenNetworkLinux website](https://opennetlinux.org/binaries/), you will run into trouble with Stratum.


## Install ONL using ONIE

- Restart switch with a serial console cable attached
- Wait for the GNU GRUB selection screen on boot-up
- Select `ONIE`
- Select `ONIE: rescue`
- Install the ONL image created in the previous step by either
    - exposing the `INSTALLER` via HTTP
        - `onie-nos-install http://the-http-server-where-the-installer-is-hosted/ONL-ONLPv2_ONL-OS_2020-06-22.1054-bb20e13_AMD64_INSTALLED_INSTALLER`
    - copying the `INSTALLER` to the switch using scp
        - `scp YOUR_HOST:ONL-ONLPv2_ONL-OS_2020-06-22.1054-bb20e13_AMD64_INSTALLED_INSTALLER .`
        - `onie-nos-install ONL-ONLPv2_ONL-OS_2020-06-22.1054-bb20e13_AMD64_INSTALLED_INSTALLER`

The switch should restart after ONIE finished installing ONL.

:pencil2: The `user:password` for ONL is `root:onl`.

## Start Stratum with Docker

On the switch, install [Docker](https://linuxize.com/post/how-to-install-and-use-docker-on-debian-9/).
Docker is already installed in the pre-built version from the fork mentioned above.

```
git clone git@github.com:stratum/stratum.git # or checkout a specific branch/tag via --branch some_branch
cd stratum
# Show options
./stratum/hal/bin/barefoot/docker/start-stratum-container.sh -help

# Start Stratum in foreground
./stratum/hal/bin/barefoot/docker/start-stratum-container.sh
```

:warning: **Important**: Huge table allocation **must** be activated on the host system, see [here](https://github.com/stratum/stratum/blob/master/stratum/hal/bin/barefoot/README.md#huge-pages--dma-allocation-error).

:pencil2: (Optional) You can tunnel the ports exposed by Stratum (P4Runtime, gNMI, gNOI) via SSH tunnels:

```
for port in 9339 9559 28000; do
    ssh \
        -o ExitOnForwardFailure=yes \
        -f \
        -N \
        -L 0.0.0.0:$port:localhost:$port \
        $HOST
done
```
The ports on the Stratum switch are now tunneled to your local ports, e.g., accessing localhost:9339 will actually access port 9339 on the switch.


## Start ONOS

ONOS can be started on any host with a connection to switch.

```
docker run \
    --rm \
    --tty \
    --detach \
    --publish 8181:8181 \
    --publish 8101:8101 \
    --publish 5005:5005 \
    --publish 830:830 \
    --name onos \
    onosproject/onos:2.2.3

docker logs --follow onos
```

Visit [http://localhost:8181/onos/ui/login.html](http://localhost:8181/onos/ui/login.html) to confirm that ONOS is initialized (can take some while).

:pencil2: The `user:password` for ONOS is `onos:rocks`.

:pencil2: (Optional) For easier handling, you can tunnel the ONOS ports to your local machine:
```
HOST="THE_SSH_HOST_YOU_STARTED_ONOS_ON"
for port in 8181 8101 5005 830; do
    ssh \
        -o ExitOnForwardFailure=yes \
        -f \
        -N \
        -L 0.0.0.0:$port:localhost:$port \
        $HOST
done
```
This tunnels the ports to your local machine which enables you to execute ONOS commands locally, e.g., `onos onos@localhost` (user/password is the same as for the GUI).



## Enable ONOS apps

Enter the ONOS CLI (`onos onos@localhost`) and execute
```
onos> app activate org.onosproject.drivers.barefoot
onos> app activate org.onosproject.segmentrouting
```

## Compile `fabric-tofino`

[Main page](https://github.com/opencord/fabric-tofino)

You now have to install a ONOS pipeconf for the Tofino switch.

```
git clone git@github.com:opencord/fabric-tofino.git
cd fabric-tofino

# This will take a while - generated a *.oar file containing the Pipeconfs with compiled P4 programs (drivers for ONOS)
make pipeconf
# This will install the app (*.oar) in ONOS. It will be activated as well
make pipeconf-install ONOS_HOST=localhost
```

## Connect ONOS to your switch

Adapt the `tofino-netcfg.json` in the `fabric-tofino` to your switch. See [fabric-tofino README](https://github.com/opencord/fabric-tofino#4---connect-onos-to-a-stratum-switch) for more information.

The `managementAddress` is the IP of the management port of the switch.

### Selecting the pipeconf for your device
In particular, you have to adapt the `pipeconf` part in the `tofino-netcfg.json` to your switch.

There are two Edgecore-internal codenames for Tofino ASICs, namely: `montara` (Tofino 18D/20D/32D), and `mavericks` (Tofino 32Q/64Q).
The Accton Wedge 100BF-32X is powered by `montara`.

:pencil: **Note**: if you have the Barefoot SDE installed on your switch, you can get the chip type by using using the SDE shell:
```
./stratum/hal/bin/barefoot/docker/start-stratum-container.sh -bf_switchd_background=false

# A lot of logs ...

bf-sde> pipe_mgr dev
-------------------------------------
Device|Type |#pipe|#stg|#prsr|#macblk
------|-----|-----|----|-----|-------
0     |T32D |2    |12  |18   |-1
```
In this case, you would have to use either `org.opencord.fabric.tofino.montara_sde_9_0_0`  (default), `org.opencord.fabric-bng.tofino.montara_sde_9_0_0`, `org.opencord.fabric-spgw.tofino.montara_sde_9_0_0`, or `org.opencord.fabric-int.tofino.montara_sde_9_0_0` as the pipeconf for your device.

### Apply netcfg to ONOS
Register the switch in ONOS by executing:
```
make netcfg ONOS_HOST=localhost
```

## Test installation

Check your ONOS GUI or CLI for your switching device.
ONOS may take some time to provision the switch.
ONOS should show a green checkmark in the "Devices" tab and correctly identify the switch ports.


## Troubleshooting

The ONOS and Stratum logs are the first place to look for errors.
When one step fails or have some error, try killing ONOS and reconfiguring again.

You can use the [ONOS CLI](https://wiki.onosproject.org/display/ONOS/Appendix+A+%3A+CLI+commands) or GUI to check for pipeconfs, devices, ...:
```
onos> devices
onos> drivers
onos> pipeconfs | grep tofino
onos> apps
```

In case of problems with these instructions, feel free to create an issue or PR.


## Useful links

- ["Running Stratum on a Barefoot Tofino based switch"](https://github.com/stratum/stratum/blob/master/stratum/hal/bin/barefoot/README.md)
    - mostly about building/running Stratum, not preparing the OS or integrating with ONOS
- [fabric-tofino](https://github.com/opencord/fabric-tofino)
- ["Running Stratum on a Barefoot Tofino based switch"](https://github.com/stratum/stratum/tree/master/stratum/hal/bin/barefoot)
- ["Trellis+Stratum example" (Untested)](https://github.com/stratum/stratum/tree/master/tools/mininet/examples/trellis)
- [ONL building](https://opennetlinux.org/doc-building.html)
- [stratum-bf Docker](https://registry.hub.docker.com/r/stratumproject/stratum-bf)
- [ONOS CLI](https://wiki.onosproject.org/display/ONOS/Appendix+A+%3A+CLI+commands)
- ["Using ONOS to control Stratum-enabled Intel/Barefoot Tofino-based switches"](https://wiki.onosproject.org/pages/viewpage.action?pageId=16122978)