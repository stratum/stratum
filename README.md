[Stratum - A P4Runtime based reference framework](https://github.com/opennetworkinglab/stratum.git)

Copyright 2018 Google LLC

Build status (master): ![CircleCI Status](https://circleci.com/gh/opennetworkinglab/stratum/tree/master.svg?style=svg&circle-token=4daca5c647bfe024b4420c9fa21e9f6272bcd50d)

# Documentation

# Overview

[P4Runtime](https://p4.org/p4-runtime) provides a flexible mechanism for
configuring the forwarding pipeline on a network switch.

[gNMI](https://github.com/openconfig/reference/tree/master/rpc/gnmi) is a
framework for network device management that uses gRPC as the transport
mechanism.

# Source code

This repository contains source code for a reference implementation of
the P4Runtime and gNMI services, that serves as the hardware abstraction layer
for a network switch. It has been successfully prototyped at Google, running on
production hardware on a data center network subsytem.

# Development environment

We provide a script to create a Docker development environment for Stratum. Run
`setup_dev_env.sh -h` for more information. Typical usage looks like this:

    ./setup_dev_env.sh --pull --mount-ssh --git-name <name> --git-email <email>

The script will build a Docker image using Dockerfile.dev and run a bash session
in it. This directory will be mounted in the Docker image and you will be able
to run git, edit code, and build Stratum / run tests using Bazel.

If you are using Docker on Linux, make sure that you can use Docker as a
non-root user, otherwise you will not be able to run setup_dev_env.sh:

    sudo usermod -aG docker $USER
