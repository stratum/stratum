# Hercules: P4C backend

The p4c_backends directory contains [P4 compiler](https://github.com/p4lang/p4c)
extensions for the Hercules vendor-agnostic networking project.  These
extensions take a [P4 specification](http://p4.org/spec/) as input.  They
produce output that Hercules platforms use to encode/decode P4 runtime RPCs
and implement the network behavior defined by the P4 spec.  Refer to the
[overview]
(http://google3/platforms/networking/hercules/g3doc/p4c_backend_overview.md)
for more details.
