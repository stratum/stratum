# Compiling main.p4

`main.p4` represents the reference pipeline for Broadcom's fixed-function switches,
but can also be Tofino or software switches.

## Inside Bazel

Stratum has integrated Bazel rules to compile included P4 programs as part of the source tree.
To compile `main.p4` for all supported platforms, including Broadcom's fixed-function switches,
only a single command is needed:

```bash
bazel build //stratum/pipelines/main/...
```

The output files can be located by running: `bazel aquery //stratum/pipelines/main/... | grep Outputs`.

## External p4c Compiler

TODO


## More information on the p4c-fpm compiler

See: [p4c-fpm README](../../p4c_backends/README.md)
