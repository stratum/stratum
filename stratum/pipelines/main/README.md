# Compiling main.p4

`main.p4` represents the reference pipeline for Broadcom's fixed-function switches,
but can also be used on Tofino or software switches.

## Inside Bazel

Stratum has integrated Bazel rules to compile included P4 programs as part of the source tree.
To compile `main.p4` for all supported platforms, including Broadcom's fixed-function switches,
only a single command is needed:

```bash
bazel build //stratum/pipelines/main:main_pipelines
```

The output tarball can be located by running: `bazel aquery //stratum/pipelines/main:main_pipelines | grep Outputs`, but the intermediate files are also available in `bazel-bin`.

## Docker `p4c-fpm`

We also publish the `p4c-fpm` compiler as a [Docker container](https://hub.docker.com/repository/docker/stratumproject/p4c-fpm).

## Pushing the pipeline to a switch

In a production setup this is the job of a SDN controller like [ONOS](https://github.com/opennetworkinglab/onos/).

For testing and exploration purposes a tool like [p4runtime-shell](https://github.com/p4lang/p4runtime-shell) is useful.

## More information on the p4c-fpm compiler

See: [p4c-fpm README](../../p4c_backends/README.md)
