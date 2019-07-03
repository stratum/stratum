# Stratum-enabled Mininet

We provide a Docker image that can execute a Mininet emulated network using
`stratum_bmv2` as the default switch.

## Build image

To build this image, from the Stratum project root:

    docker build -t <some tag> -f tools/mininet/Dockerfile .

## Obtain image from Docker Hub

Alternatively, you can obtain the image from Docker Hub:

    docker pull opennetworking/mn-stratum

This image is updated daily and built from the master branch of Stratum.

## Run container

To run the container:

    docker run --privileged --rm -it opennetworking/mn-stratum [MININET ARGS]

After running this command, you should see the mininet CLI (`mininet>`).

It is important to run this container in privileged mode (`--privileged`) so
mininet can modify the network interfaces and properties to emulate the desired
topology.

The image defines as entry point the mininet executable configured to use
`stratum_bmv2` as the default switch. Options to the docker run command
(`[MININET ARGS]`) are passed as parameters to the mininet process. For more
information on the supported mininet options, please check the official mininet
documentation.

For example, to run a linear topology with 3 switches:

    docker run --privileged --rm -it opennetworking/mn-stratum --topo linear,3

### gRPC server ports

Each switch is bound to a different gRPC port, starting from 50001 and
increasing. To connect an external client (e.g. an SDN controller) to the
switches, you have to publish the corresponding ports.

For example, when running a topology with 3 switches:

     docker run --privileged --rm -it -p 50001-50003:50001-50003 opennetworking/mn-stratum --topo linear,3

### Logs and other temporary files

To allow easier access to logs and other files, we suggest sharing the
`/tmp` directory inside the container on the host system using the docker run
`-v` option, for example:

    docker run ... -v /tmp/mn-stratum:/tmp ... opennetworking/mn-stratum ...

By using this option, during the container execution, a number of files related
to the execution of `stratum_bmv2` will be available under `/tmp/mn-stratum` in the
host system. Files are contained in a directory named after the switch name used in
Mininet,e.g. s1, s2, etc.

Example of these files are:

* `s1/stratum_bmv2.log`: contains the stratum_bmv2 log for switch `s1`;
* `s1/chassis-config.txt`: the chassis config file used at switch startup;
* `s1/grpc-port.txt`: the gRPC port associated to this switch;
* `s1/onos-netcfg.json`: sample configuration file to connect the switch to the ONOS
   SDN controller;

### Bash alias

A convenient way to quickly start the mn-stratum container is to create an alias
in your bash profile file (`.bashrc`, `.bash_aliases`, or `.bash_profile`) . For
example:

    alias mn-stratum="rm -rf /tmp/mn-stratum && docker run --privileged --rm -it -v /tmp/mn-stratum:/tmp -p50001-50030:50001-50030 --name mn-stratum --hostname mn-stratum opennetworking/mn-stratum"

Then, to run a a simple 1-switch 2-host topology:

    $ mn-stratum
    *** Creating network
    *** Adding controller
    *** Adding hosts:
    h1 h2
    *** Adding switches:
    s1
    *** Adding links:
    (h1, s1) (h2, s1)
    *** Configuring hosts
    h1 h2
    *** Starting controller
    
    *** Starting 1 switches
    s1 ....⚡️ stratum_bmv2 @ 50001
    
    *** Starting CLI:
    mininet>

Or a linear one with 3 switches and 3 hosts:

    $ mn-stratum --topo linear,3
    *** Creating network
    *** Adding controller
    *** Adding hosts:
    h1 h2 h3
    *** Adding switches:
    s1 s2 s3
    *** Adding links:
    (h1, s1) (h2, s2) (h3, s3) (s2, s1) (s3, s2)
    *** Configuring hosts
    h1 h2 h3
    *** Starting controller
    
    *** Starting 3 switches
    s1 .....⚡️ stratum_bmv2 @ 50001
    s2 .....⚡️ stratum_bmv2 @ 50002
    s3 .....⚡️ stratum_bmv2 @ 50003
    
    *** Starting CLI:
    mininet>