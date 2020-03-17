# Stratum: P4C backend

The p4c_backends directory contains [P4 compiler](https://github.com/p4lang/p4c)
extensions for the Stratum vendor-agnostic networking project.  These
extensions take a [P4 specification](http://p4.org/spec/) as input.  They
produce output that Stratum platforms use to encode/decode P4 runtime RPCs
and implement the network behavior defined by the P4 spec.  Refer to the
[overview](../../README.md) for more info and to the `main.p4`
[pipeline](../pipelines/main/README.md) for an example.
