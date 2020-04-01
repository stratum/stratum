# Running Stratum with the Netcope SDK

## Dependencies

> You can skip installing system dependencies, netcope, and PI if you are
> using the Docker environment (setup_ubuntu_dev_env.sh)

### Install system dependencies

TODO: what dependencies for Netcope 

```
sudo apt-get install libjudy-dev libgmp-dev libpcap-dev libboost1.58-all-dev
```

### Create a local directory where you will install the Netcope SDK
```
mkdir np4_install
export NP4_INSTALL=`pwd`/np4_install
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$NP4_INSTALL/lib
```

### Install PI

```
cd $NP4_INSTALL
git clone https://github.com/craigsdell/PI.git
```

## Building the `stratum_np4intel` binary

The `stratum_np4intel` binary is a standalone executable which includes:
1. a Stratum implementation for the Netcope SDK
2. the `v1model` datapath

To build `stratum_np4intel`, make sure that the `NP4_INSTALL` environment 
variable is set and points to your local netcope SDK installation. Then 
build the Bazel target:
```
bazel build //stratum/hal/bin/np4intel:stratum_np4intel --define phal_with_onlp=false
```

## Running the `stratum_np4intel` binary

As of now the `stratum_np4intel` binary *can only be run from the root of your
Stratum Bazel workspace*:

```
./bazel-bin/stratum/hal/bin/np4intel/stratum_np4intel \
    --external_stratum_urls=0.0.0.0:28000 \
    --persistent_config_dir=<config dir> \
    --forwarding_pipeline_configs_file=<config dir>/pipeline_config.pb.txt \
    --chassis_config_file=<config dir>/chassis_config.pb.txt \
    --logtosyslog=true
    --v=10
```

For a sample `chassis_config.pb.txt` file, see sample_config.proto.txt in this
directory. For each singleton port, use the Linux interface name as the `name`
and set the `admin_state` to `ADMIN_STATE_ENABLED`.

As a basic test, you can run the following commands. It will start a P4Runtime
client in a Docker image and perform a `SetForwardingPipelineConfig` RPC (which
pushes a new P4 data plane to bmv2). You will need a bmv2 JSON file and a P4Info
Protobuf text file, which you can obtain by compiling your P4 program with the
[p4c](https://github.com/p4lang/p4c) compiler.
```
# compile P4 program (skip if you already have the bmv2 JSON file and the P4Info
# text file)
p4c -b bmv2 -a v1model -o /tmp/ --p4runtime-format text --p4runtime-file /tmp/<prog>.proto.txt <prog>.p4
# run P4Runtime client
cp stratum/hal/bin/bmv2/update_config.py /tmp/ && \
[sudo] docker run -v /tmp:/tmp -w /tmp p4lang/pi ./update_config.py \
    --grpc-addr <YOUR_HOST_IP_ADDRESS>:28000 --json <prog>.json --p4info <prog>.proto.txt
```

