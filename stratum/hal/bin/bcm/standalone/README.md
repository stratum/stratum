# Running Stratum on a Broadcom SDKLT based switch

The following guide details how to compile the Stratum binary to run on a Broadcom based switch (i.e. like Tomahawk) using the Broadcom SDKLT.

## Dependencies

Requires the Broadcom SDKLT to be installed (see SDKLT Installation instructions below).

### SDKLT Installation

The Broadcom SDKLT needs to be built and installed as a prerequisite to building Stratum and can be achieved by building the SDKLT Demo App.

The detailed instructions for building the SDKLT Demo App are contained in the SDKLT github repository here: https://github.com/Broadcom-Network-Switching-Software/SDKLT/wiki/Building-the-Demo-App.

An example is given below:

```
sudo apt-get install libyaml-dev python-yaml
git clone https://github.com/Broadcom-Network-Switching-Software/SDKLT.git
cd SDKLT
export SDKLT=$PWD
export SDK=$SDKLT/src
cd $SDK/appl/demo
make -s TARGET_PLATFORM=native_thsim
export SDKLT_INSTALL=$SDK/appl/sdklib/build/xlr_linux
```

It should be noted that the environment variables set here (i.e. SDK, SDKLT and SDKLT_INSTALL) will be required in the Stratum build steps below.

## Building the `stratum_bcm` binary

The `stratum_bcm` binary is a standalone executable which includes:
- a Stratum implementation for bcm
- links to the Broadcom SDKLT libraries and headers

To build the `stratum_bcm` binary you will need to:
1. make sure that the Broadcom SDKLT environment variables are set (see example above in the `SDKLT Installation` section).
2. Clone the Stratum repository
3. Change into the stratum directory
4. Setup the development environment (kicks off a container)
5. Then build the target using Bazel

An example is shown below:

```
export SDKLT=~/SDKLT
export SDK=$SDKLT/src
export SDKLT_INSTALL=$SDK/appl/sdklib/build/xlr_linux
git clone https://github.com/stratum/stratum.git
cd stratum
./setup_dev_env.sh
bazel build //stratum/hal/bin/bcm/standalone:stratum_bcm
```

You can build the binary on your build server and copy it over to the switch.

## Running the `stratum_bcm` binary

Running `stratum_bcm` requires four configuration files, passed as CLI flags:

- base_bcm_chassis_map_file: Protobuf defining chip capabilities and all possible port configurations of a chassis.
    Example found under: `/stratum/hal/config/accton7710_bcm_chassis_map_minimal.pb.txt`
- chassis_config_file: Protobuf setting the config of a specific node.
    Selects a subset of the available port configurations from the chassis map. Determines
    which ports will be available.
    Example found under: `/stratum/hal/config/accton7710_bcm_chassis_config_minimal.pb.txt`
- bcm_sdk_config_file: Yaml config passed to the SDKLT. Must match the chassis map.
    Example found under: `/stratum/hal/config/AS7712-stratum.config.yml`
- bcm_hardware_specs_file: ACL and UDF properties of chips. Found under: `/stratum/hal/config/bcm_hardware_specs.pb.txt`

Depending on your actual cabling, you'll have to adjust the config files. Panel ports 31 & 32 are in loopback mode and should work without cables.

The config flags are best stored in a flagfile `stratum.flags`:

```bash
-external_hercules_urls=0.0.0.0:28000
-persistent_config_dir=/tmp/hercules
-base_bcm_chassis_map_file=/root/as7712_cfg/accton7710_bcm_chassis_map_max.pb.txt
-chassis_config_file=/root/as7712_cfg/accton7710_bcm_chassis_config_max.pb.txt
-bcm_sdk_config_file=/root/as7712-stratum-test-pft-tests/cfg/AS7712-stratum.config.yml
-bcm_hardware_specs_file=/root/as7712_cfg/bcm_hardware_specs.pb.txt
-forwarding_pipeline_configs_file=/tmp/hercules/pipeline_cfg.pb.txt
-write_req_log_file=/tmp/hercules/p4_writes.pb.txt
-bcm_serdes_db_proto_file=/root/as7712_cfg/dummy_serdes_db.pb.txt
-bcm_sdk_checkpoint_dir=/tmp/bcm_chkpt
-colorlogtostderr
-alsologtostderr
-logtosyslog=false
-v=0
```

(You can also use a bash script, passing the flags individually. Prevents [Issue 61](https://github.com/gflags/gflags/issues/61)).

Start stratum:
```bash
./stratum_bcm -flagfile=stratum.flags
```

You should see the ports coming up and have a SDKLT shell prompt:
```
I0628 18:29:10.806623  7930 bcm_chassis_manager.cc:1738] State of SingletonPort (node_id: 1, port_id: 34, slot: 1, port: 3, unit: 0, logical_port: 34, speed: 40G): UP
BCMLT.0>
```
