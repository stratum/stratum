# Running Stratum on a Barefoot Tofino based switch

Choose the location where you will be installing the SDE. This environment
variable MUST be set for the Stratum build.
```
export BF_SDE_INSTALL=...
```

## BSP or BSP-less mode (with ONLP)?

Stratum can be run on Tofino-based platforms in 2 different modes:
 * if your platform comes with ONLPv2 and a JSON "port mapping" file is provided
   by the platform vendor (see this
   [example](platforms/x86-64-accton-wedge100bf-32x-r0.json) for the Wedge 100bf-32x),
   you can use Stratum in "BSP-less mode". Refer to this
   [section](#running-the-binary-in-bsp-less-mode) for more information. This is
   the recommended mode.
 * otherwise, you need to build & install the BSP. You will not be able to use
   the Stratum ONLP support.

## Installing the SDE

These instructions are for SDE version 8.8.0. Starting with SDE release 8.7.0,
Barefoot's P4Studio Build tool comes with a default Stratum profile, which takes
care of installing all the necessary dependencies and builds the SDE with the
appropriate flags.

Please follow these steps:

 1. Extract the SDE: `tar -xzvf bf-sde-<SDE_VERSION>.tgz`

 2. Set the required environment variables
```
export SDE=`pwd`/bf-sde-<SDE_VERSION>
export SDE_INSTALL=$BF_SDE_INSTALL
```

 3. Build and install the SDE. Use the provided profile
    (`stratum_profile.yaml`). Feel free to customize the profile if needed;
    please refer to the P4Studio Build documentation. If you are using the
    reference BSP provided by Barefoot, you may also use P4Studio Build to
    install the BSP (see [below](#installing-the-reference-bsp-for-the-wedge)).
```
cd $SDE/p4studio_build
./p4studio_build.py -up profiles/stratum_profile.yaml
```

If your platform supports BSP-less mode (**recommended**), you do not need to
install the BSP. Refer to the section
[below](#running-the-binary-in-bsp-less-mode).

### Installing the reference BSP for the Wedge

**Ignore this section if your platform supports ONLPv2 and BSP-less mode.**

If you are using the reference BSP provided by Barefoot (for the Wedge switch),
you may use P4Studio Build to install the BSP. All you need to do is extract the
BSP tarball and **use an extra command-line option when running P4Studio
Build**:

```
tar -xzvf bf-reference-bsp-<SDE_VERSION>.tgz
export BSP_PATH=`pwd`/bf-reference-bsp-<SDE_VERSION>
```
Replace step 3 in the sequence above with:
```
cd $SDE/p4studio_build
./p4studio_build.py -up profiles/stratum_profile.yaml --bsp-path $BSP_PATH
```

You may also still install the BSP manually. If you are not using the reference
BSP, you will need to install the BSP yourself (under `$BF_SDE_INSTALL`) based
on your vendor's instructions.

## Building the binary

If your platform comes with ONLPv2 and you want to use Stratum in BSP-less mode,
you may want to set the `ONLP_INSTALL` environment variable to point to your
ONLP installation before building `stratum_bf`. You can also build `stratum_bf`
without setting `ONLP_INSTALL`, in which case the default ONLP library will be
downloaded and we will build against it. This is useful if you are building
`stratum_bf` for simulation, or if you are not building it directly on the
switch (in this case just make sure the correct ONLP library for your platform
is loaded at runtime).

**`stratum_bf` currently needs to link to the thrift library, until support for
  gNMI is complete. As a result you need to set the `THRIFT_INSTALL` environment
  variable to point to the directory where `libthrift.so` is installed. For most
  users, this directory should be `/usr/local/lib`**
```
export THRIFT_INSTALL=/usr/local/lib
bazel build //stratum/hal/bin/barefoot:stratum_bf
```

## Running the binary (with BSP or Tofino software model)

```
sudo LD_LIBRARY_PATH=$BF_SDE_INSTALL/lib \
     ./bazel-bin/stratum/hal/bin/barefoot/stratum_bf \
       --external_hercules_urls=0.0.0.0:28000 \
       --grpc_max_recv_msg_size=256 \
       --bf_sde_install=$BF_SDE_INSTALL \
       --persistent_config_dir=<config dir> \
       --forwarding_pipeline_configs_file=<config dir>/p4_pipeline.pb.txt \
       --chassis_config_file=<config dir>/chassis_config.pb.txt \
       --write_req_log_file=<config dir>/p4_writes.pb.txt \
       --bf_sim
```

For a sample `chassis_config.pb.txt` file, see sample_config.proto.txt in this
directory. *Do not use the ucli or the Thrift PAL RPC service for port
configuration.* You may use the ucli to check port status (`pm show`).

The `--bf_sim` flag tells Stratum not to use the Phal ONLP implementation, but
`PhalSim`, a "fake" Phal implementation, instead. Use this flag when you are
using a vendor-provided BSP or running Stratum with the Tofino software model.

## Running the binary in BSP-less mode

If ONLP support is available for your platform, you do not need to use a
BSP. Instead the platform vendor can provide a JSON "port mapping" file (see
this [example](platforms/x86-64-accton-wedge100bf-32x-r0.json) for the Wedge
100bf-32x) and Stratum takes care of making the information exposed by ONLP
available to the SDE as needed.

To start Stratum in BSP-less mode, copy the JSON port mapping file for your
platform to `$BF_SDE_INSTALL/share` and run `stratum_bf` with
`--bf_switchd_cfg=stratum/hal/bin/barefoot/tofino_skip_p4_no_bsp.conf`.

Platforms with repeaters (such as the Wedge 100bf-65x) are not currently
supported in BSP-less mode.

We only support DAC cables at the moment, and autoneg must be forced "on" for
every port. See [sample_config.proto.txt](sample_config.proto.txt) for an
example (look for `autoneg: TRI_STATE_TRUE`). We are working on adding support
for optical cables.

## Testing gNMI

You can use the tools/gnmi/gnmi-cli.py script for gNMI get, set, and subscriptions:
```
python tools/gnmi/gnmi-cli.py --grpc-addr 0.0.0.0:28000 get /interfaces/interface[name=128]/state/ifindex
python tools/gnmi/gnmi-cli.py --grpc-addr 0.0.0.0:28000 set /interfaces/interface[name=1/1/1]/config/health-indicator --string-val GOOD
python tools/gnmi/gnmi-cli.py --grpc-addr 0.0.0.0:28000 sub /interfaces/interface[name=128]/state/oper-status
```

