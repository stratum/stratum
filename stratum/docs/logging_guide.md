<!--
Copyright 2021-present Open Networking Foundation

SPDX-License-Identifier: Apache-2.0
-->

# Guide to Logging

Use these rules when writing new code that prints log messages.

### Expected failures

These are failures that are expected to occur and are handled internally by the
code. Use a low level (`INFO`) or don't log them at all.
Examples: missing ChassisConfig or pipeline on first startup.

### Repeated failures

Some failures will repeatedly print the same message without adding any new
information. Consider using `LOG_FIRST_N()` with no more than 3 repetitions.
Examples: unconfigured ports on Tofino.

### Transient failures

Transient failures are temporary in nature and resolve on their on, either
after some time passes or after a condition changes. This means that a later
retry of same operation has a chance of success. Use `LOG_EVERY_N()` with N
between 10-100, depending on the frequency of the error. Aim for a rate of no
more than 1 message/second.
Examples: failed PacketOut transmission because a link is down.

### Further reading

[glog user guide](https://github.com/google/glog/blob/master/README.rst#user-guide)

[Full list of log primitives](https://github.com/google/glog/blob/c39fcf6e8a4b721d57285ea49f668182e9d6cd1c/src/glog/logging.h.in#L223)
