# Running Stratum on a Barefoot Tofino based switch

Choose the location where you will be installing the SDE. This environment
variable MUST be set for the Stratum build.
```
export BF_SDE_INSTALL=...
```

## Installing p4lang/PI

```
git clone https://github.com/p4lang/PI.git
cd PI
./autogen.sh
./configure --without-bmv2 --without-proto --without-fe-cpp --without-cli --without-internal-rpc --prefix=$BF_SDE_INSTALL
make [-j4]
[sudo] make install
[sudo ldconfig]
```
The *master* branch **may** work for this repo, but you can also used the commit
we used for testing: 3769806afc02de3f06a151634640bac985a172d0

**Make sure that you do not have an existing PI installation in a system
  directory or you may get issues when building the SDE.**

## Installing the SDE

These steps were tested with SDE 8.4.0. We assume you have the `sde_build.sh`
and `set_sde.bash` convenience scripts, and that you copied them inside the
extracted SDE directory (`bf-sde-8.4.0`).

```
source set_sde.bash
export SDE_INSTALL=$BF_SDE_INSTALL
# This steps will depend on the version of the SDE / sde_build.sh script
# To be on the safe side you can choose to execute all the steps
./sde_build.sh --no-bmv2 -r --bf-drivers-extra-flags="--disable-grpc --enable-pi" -t yes -s 1 -e 6
./sde_build.sh --no-bmv2 -r --bf-drivers-extra-flags="--disable-grpc --enable-pi" -t yes -s 8 -e 14
./sde_build.sh --no-bmv2 -r --bf-drivers-extra-flags="--disable-grpc --enable-pi" -t yes -s 17 -e 18
./sde_build.sh --no-bmv2 -r --bf-drivers-extra-flags="--disable-grpc --enable-pi" -t yes -s 21
```

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
     ./bazel-bin/stratum/hal/bin/barefoot/stratum_bf --local_hercules_url=0.0.0.0:28000 \
     --forwarding_pipeline_configs_file=/tmp/i.x --persistent_config_dir=/tmp
     --bf_sde_install=$BF_SDE_INSTALL --grpc_max_recv_msg_size=256
```
