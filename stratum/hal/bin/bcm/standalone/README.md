# Stratum on a Broadcom SDKLT based switch

The following guide details how to compile the Stratum binary to run on a Broadcom based switch (i.e. like Tomahawk) using the Broadcom SDKLT.

## Build Dependencies

Stratum comes with a development Docker container for build purposes.

Reasonable new GCC or Clang (preferred). [SDKLT](https://github.com/opennetworkinglab/SDKLT) is fetched by bazel automatically during the build process. At runtime two Kernel modules are required, which can be downloaded on [GitHub](https://github.com/opennetworkinglab/SDKLT/releases).

## Building the `stratum_bcm` binary

The `stratum_bcm` binary is a standalone executable which includes:
- a Stratum implementation for bcm
- links to the Broadcom SDKLT libraries and headers

To build the `stratum_bcm` binary you will need to:
1. Clone the Stratum repository
2. Change into the stratum directory
3. Setup the development environment (kicks off a container)
4. Then build the target using Bazel

An example is shown below:

```
git clone https://github.com/stratum/stratum.git
cd stratum
./setup_dev_env.sh
bazel build //stratum/hal/bin/bcm/standalone:stratum_bcm
```

You can build the binary on your build server and copy it over to the switch.

## Running the `stratum_bcm` binary

SDKLT requires two Kernel modules to be installed for Packet IO and interfacing with the ASIC. We provide prebuilt binaries for Kernel 4.14.49 in the release [tarball]((https://github.com/opennetworkinglab/SDKLT/releases)). Install them before running stratum:
`insmod linux_ngbde.ko && insmod linux_ngknet.ko`

Running `stratum_bcm` requires five configuration files, passed as CLI flags:

- base_bcm_chassis_map_file: Protobuf defining chip capabilities and all possible port configurations of a chassis.
    Example found under: `/stratum/hal/config/**platform name**/base_bcm_chassis_map.pb.txt`
- chassis_config_file: Protobuf setting the config of a specific node.
    Selects a subset of the available port configurations from the chassis map. Determines
    which ports will be available.
    Example found under: `/stratum/hal/config/**platform name**/chassis_config.pb.txt`
- bcm_sdk_config_file: Yaml config passed to the SDKLT. Must match the chassis map.
    Example found under: `/stratum/hal/config/**platform name**/SDKLT.yml`
- bcm_hardware_specs_file: ACL and UDF properties of chips. Found under: `/stratum/hal/config/bcm_hardware_specs.pb.txt`
- bcm_serdes_db_proto_file: Contains SerDes configuration. Not implemented yet, can be an empty file.

Depending on your actual cabling, you'll have to adjust the config files. Panel ports 31 & 32 are in loopback mode and should work without cables.

The config flags are best stored in a flag file `stratum.flags`:

```bash
-external_stratum_urls=0.0.0.0:28000
-persistent_config_dir=/etc/stratum
-base_bcm_chassis_map_file=/etc/stratum/chassis_map.pb.txt
-chassis_config_file=/etc/stratum/chassis_config.pb.txt
-bcm_sdk_config_file=/etc/stratum/sdk_config.yml
-bcm_hardware_specs_file=/etc/stratum/bcm_hardware_specs.pb.txt
-forwarding_pipeline_configs_file=/tmp/stratum/pipeline_cfg.pb.txt
-write_req_log_file=/tmp/stratum/p4_writes.pb.txt
-bcm_serdes_db_proto_file=/etc/stratum/dummy_serdes_db.pb.txt
-bcm_sdk_checkpoint_dir=/tmp/stratum/bcm_chkpt
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
