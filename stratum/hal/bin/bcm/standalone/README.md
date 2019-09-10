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

## Running the `stratum_bcm` binary

Work in progress - BCM binary does not run at this point, sdk wrapper code
needs to be completed.
