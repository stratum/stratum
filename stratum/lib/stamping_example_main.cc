// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include <stdio.h>

#include "stratum/lib/version_variables.h"

int main() {
  printf("kBuildTimestamp: %s\n", kBuildTimestamp);
  printf("kBuildTimestamp2: %i\n", kBuildTimestamp2);
  printf("kBuildTimestampHuman: %s\n", kBuildTimestampHuman);
  printf("kGitRef: %s\n", kGitRef);
  printf("kBuildScmRevision: %s\n", kBuildScmRevision);
}
