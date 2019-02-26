# Running Stratum on a Barefoot Tofino based switch

Choose the location where you will be installing the SDE. This environment
variable MUST be set for the Stratum build.
```
export BF_SDE_INSTALL=...
```

## Installing the SDE

These instructions are for SDE version 8.7.0 and later. Starting with SDE
release 8.7.0, Barefoot's P4Studio Build tool comes with a default Stratum
profile, which takes care of installing all the necessary dependencies and
builds the SDE with the appropriate flags.

Please follow these steps:

 1. Extract the SDE: `tar -xzvf bf-sde-<SDE_VERSION>.tgz`

 2. Set the required environment variables
```
export SDE=`pwd`/bf-sde-<SDE_VERSION>
export SDE_INSTALL=$BF_SDE_INSTALL
```

 3. Build and install the SDE. Use the provided profile
    (`stratum_profile.yaml`). Feel free to customize the profile if
    needed; please refer to the P4Studio Build documentation. **If you are using
    the reference BSP provided by Barefoot, you may also use P4Studio Build to
    install the BSP (see below).**
```
cd $SDE/p4studio_build
./p4studio_build.py -up profiles/stratum_profile.yaml
```

### Installing the reference BSP for the Wedge

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

**`stratum_bf` currently needs to link to the thrift library, until support for
  gNMI is complete. As a result you need to set the `THRIFT_INSTALL` environment
  variable to point to the directory where `libthrift.so` is installed. For most
  users, this directory should be `/usr/local/lib`**
```
export THRIFT_INSTALL=/usr/local/lib
bazel build //stratum/hal/bin/barefoot:stratum_bf
```

## Running the binary

```
sudo LD_LIBRARY_PATH=$BF_SDE_INSTALL/lib \
     ./bazel-bin/stratum/hal/bin/barefoot/stratum_bf \
       --external_hercules_urls=0.0.0.0:28000 \
       --grpc_max_recv_msg_size=256 \
       --bf_sde_install=$BF_SDE_INSTALL \
       --persistent_config_dir=<config dir> \
       --forwarding_pipeline_configs_file=<config dir>/p4_pipeline.pb.txt \
       --chassis_config_file=<config dir>/chassis_config.pb.txt \
       --write_req_log_file=<config dir>/p4_writes.pb.txt
```

For a sample `chassis_config.pb.txt` file, see sample_config.proto.txt in this
directory. Note that at the moment, you still need to add & enable the ports
using the ucli or through the Thrift PAL RPC service. ONOS can add & enable the
ports for you if you provide the appropriate netcfg file when using the
barefoot-pro ONOS driver.

## Testing gNMI

You can use the gnmi_sub_once.py script for gNMI ONCE subscriptions:
```
python gnmi_sub_once.py --grpc-addr 0.0.0.0:28000 --interface <name> state counters in-octets
python gnmi_sub_once.py --grpc-addr 0.0.0.0:28000 --interface <name> state oper-status
```

You can use the gnmi_get.py script to test gNMI Get requests:
```
python gnmi_get.py --grpc-addr 0.0.0.0:28000 --interface <name> state counters in-octets
python gnmi_get.py --grpc-addr 0.0.0.0:28000 --interface <name> state oper-status
```
