// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/lib/barefoot/bf_interface.h"

const char bf_sde_install[] = "/usr";
const char bf_switchd_cfg[] = "/usr/share/stratum/tofino_skip_p4_no_bsp.conf";
const bool bf_switchd_background = true;

int main() {
  ::stratum::barefoot::BfInterface::CreateSingleton()->InitSde(
      bf_sde_install, bf_switchd_cfg, bf_switchd_background);

  return 0;
}