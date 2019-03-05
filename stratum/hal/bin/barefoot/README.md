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

**There is an issue with the Stratum profile on ONL for SDE 8.7.0, please refer
  to [this section](#installing-the-sde-on-onl).**

Please follow these steps:

 1. Extract the SDE: `tar -xzvf bf-sde-<SDE_VERSION>.tgz`

 2. Set the required environment variables
```
export SDE=`pwd`/bf-sde-<SDE_VERSION>
export SDE_INSTALL=$BF_SDE_INSTALL
```

 3. Build and install the SDE. Use the provided profile
    (`stratum_profile.yaml`). Feel free to customize the profile if needed;
    please refer to the P4Studio Build documentation. **If you are using the
    reference BSP provided by Barefoot, you may also use P4Studio Build to
    install the BSP (see
    [below](#installing-the-reference-bsp-for-the-wedge)).**
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

### Installing the SDE on ONL

SDE 8.7.0 has an issue when installing the Stratum profile with P4Studio
Build. The PI library is not installed by P4Studio Build and needs to be
installed manually.

To install the SDE on ONL, follow the instructions included in the Barefoot
customer documentation ("Deploying Barefoot P4 Studio SDE on Open Network
Linux"), including the installation of the `dependencies.tar.gz tarball` (this
will install Thrift and other dependencies, but not PI unfortunately). Before
running `p4studio_build.py`, you will need to manually install PI.

 1. Get the appropriate PI SHA from
 `$SDE/p4studio_build/dependencies.yaml`. Look for the following:
```
pi:
    mode: 'git clone'
    url: 'https://github.com/p4lang/PI.git'
    default_sha: <SHA>
```

For SDE 8.7.0, the SHA is a95222eca9b039f6398c048d7e1a1bf7f49b7235.

 2. Install PI
```
cd /tmp
git clone https://github.com/p4lang/PI.git
cd PI
git checkout a95222eca9b039f6398c048d7e1a1bf7f49b7235
./autogen.sh
./configure --without-bmv2 --without-proto --without-internal-rpc --without-cli
make
[sudo] make install
```

You are welcome to install PI in the same install directory as the SDE by
configuring PI with `--prefix=$SDE_INSTALL`. However, note that by default
`p4studio_build.py` will delete the contents of `$SDE_INSTALL` when you run
it. To avoid this, you can call `p4studio_build.py` with `--skip-cleanup`.

 3. Run `p4studio_build.py`
```
cd $SDE/p4studio_build
./p4studio_build.py -up profiles/stratum_profile.yaml [--bsp-path $BSP_PATH] [--skip-cleanup]
```

We are hoping to fix this in future SDE versions and installing PI manually for
ONL will no longer be required.

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
directory. *Do not use the ucli or the Thrift PAL RPC service for port
configuration.* You may use the ucli to check port status (`pm show`).

## Testing gNMI

You can use the tools/gnmi/gnmi-cli.py script for gNMI get, set, and subscriptions:
```
python tools/gnmi/gnmi-cli.py --grpc-addr 0.0.0.0:28000 get /interfaces/interface[name=128]/state/ifindex
python tools/gnmi/gnmi-cli.py --grpc-addr 0.0.0.0:28000 set /interfaces/interface[name=1/1/1]/config/health-indicator --string-val GOOD
python tools/gnmi/gnmi-cli.py --grpc-addr 0.0.0.0:28000 sub /interfaces/interface[name=128]/state/oper-status
```

