<!--
Copyright 2019 Google LLC
Copyright 2019-present Open Networking Foundation

SPDX-License-Identifier: Apache-2.0
-->

# gNMI test scenarios

This directory contains multiple .cdlang files that contain gNMI test scenarios.

To build and execute those tests you should execute:

```bash
bazel run //stratum/testing/scenarios:scenarios_test
```

This command will trigger the following actions:

1. Compilation of the cdl\_tool from the //stratum/testing/cdlang directory
   (if not present)

1. Compilation of all .cdlang files

1. Generation of //bazel-genfiles/stratum/testing/scenarios/scenarios.cc that
   implements the tests defined in the .cdlang files

1. Compilation of bazel-genfiles/stratum/testing/scenarios/scenarios.cc

1. Compilation of required libraries (like implementation of gNMI protobufs)

1. Linkage of the pieces into one executable: sceanrios\_test

1. Execution of the scearios\_test executable that will run the scenarios
   against a gNMI server that is reachable at `localhost:9339`.

If the gNMI server is not located on the same host, its address should be
provided by using the `--gnmi_url` option. For example:

```bash
bazel run //stratum/testing/scenarios:scenarios_test -- --gnmi_url bmv2.company:9339
```

# Files in this directory

* BUILD - the bazel build file

* README.md - this file

* test\_main.cc - the main test file that contains the `main()` function

* scenarios.cc.tmpl and lib.cc.tmpl - Golang text template files used to
  generate C++ code implementig the scenarios

# How to contribute

Just create new gNMI test scenarios and put them into separate .cdlang files.

That's it. Nothing else needs to be touched and/or modified!
