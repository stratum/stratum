// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

// This is the main entry for public library tests.
#include <stdlib.h>

#include "gtest/gtest.h"
#include "stratum/glue/init_google.h"

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  InitGoogle(argv[0], &argc, &argv, true);
  return RUN_ALL_TESTS();
}
