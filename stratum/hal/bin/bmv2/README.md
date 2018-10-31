# Running Stratum with the P4.org bmv2 software switch

## Dependencies

### Install system dependencies
```
sudo apt-get install libjudy-dev libgmp-dev libpcap-dev libboost1.58-all-dev
```

### Create a local directory where you will install bmv2
```
mkdir bmv2_install
export BMV2_INSTALL=`pwd`/bmv2_install
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$BMV2_INSTALL/lib
```
*If you plan on adding interfaces to bmv2 (which can only be done right now by
editing main.cpp manually), you may want to install bmv2 in a standard system
directory instead. For example you can set `BMV2_INSTALL` to /usr/local. In this
case you do not need to modify `LD_LIBRARY_PATH` but you will need to run `sudo
ldconfig` after the installation.*

### Install PI
```
git clone https://github.com/p4lang/PI.git
cd PI
./autogen.sh
./configure --without-bmv2 --without-proto --without-fe-cpp --without-cli --without-internal-rpc --prefix=$BMV2_INSTALL
make [-j4]
[sudo] make install
[sudo ldconfig]
```
The *master* branch should work for this repo, but you can also used the commit
we used for testing: 235cb3d117f3b8a3335200ab89f2f6389f6fdb9c.

### Install bmv2
```
git clone git@github.com:p4lang/behavioral-model.git bmv2
cd bmv2
./autogen.sh
./configure CPPFLAGS="-isystem$BMV2_INSTALL/include" --without-nanomsg --without-thrift --with-pi --prefix=$BMV2_INSTALL
make [-j4]
[sudo] make install
[sudo ldconfig]
```
The *master* branch should work for this repo, but you can also used the commit
we used for testing: 13370aaf9329fcb369a3ea3989722eb5f61c07f3.

## Building the `stratum_bmv2` binary

The `stratum_bmv2` binary is a standalone executable which includes:
1. a Stratum implementation for bmv2
2. the `v1model` datapath

To build `stratum_bmv2`, make sure that the `BMV2_INSTALL` environment variable
is set and points to your local bmv2 installation. Then build the Bazel target:
```
bazel build //stratum/hal/bin/bmv2:stratum_bmv2
```

## Running the `stratum_bmv2` binary

As of now the `stratum_bmv2` binary *can only be run from the root of your
Stratum Bazel workspace*:

```
./bazel-bin/stratum/hal/bin/bmv2/stratum_bmv2 --external_hercules_urls=0.0.0.0:28000 --forwarding_pipeline_configs_file=/tmp/config.txt --persistent_config_dir=/tmp/
```

You can ignore the following error, we are working on fixing it:
```
E0808 17:57:36.513559 29298 utils.cc:120] StratumErrorSpace::ERR_FILE_NOT_FOUND:  not found.
E0808 17:57:36.513905 29298 utils.cc:76] Return Error: ReadFileToString(filename, &text) failed with StratumErrorSpace::ERR_FILE_NOT_FOUND:  not found.
W0808 17:57:36.513913 29298 config_monitoring_service.cc:106] No saved chassis config found in . This is normal when the switch is just installed.
```

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
[sudo] docker run -v /tmp:/tmp -w /tmp p4lang/pi update_config.py \
    --grpc-addr <YOUR_HOST_IP_ADDRESS>:28000 --json <prog>.json --p4info <prog>.proto.txt

You can use the loopback program under `testdata/` if you do not have your own
P4 program.
