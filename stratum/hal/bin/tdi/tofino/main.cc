// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/bin/tdi/tofino/tofino_main.h"

int main(int argc, char* argv[]) {
  return stratum::hal::tdi::TofinoMain(argc, argv).error_code();
}

