// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include <stdio.h>

#include "stratum/lib/barefoot/bf_interface.h"

const char bf_sde_install[] = "/usr";
const char bf_switchd_cfg[] = "/usr/share/stratum/tofino_skip_p4_no_bsp.conf";
const bool bf_switchd_background = false;

int main() {
  int status = 0;
  PackedProtobuf packed_request = NULL;
  PackedProtobuf packed_response = NULL;
  size_t request_size = 0;
  size_t response_size = 0;

  status = bf_p4_init(bf_sde_install, bf_switchd_cfg, bf_switchd_background);
  if (status != 0) return status;
  printf("BF SDE successfully initialized\n");

  status = bf_p4_set_pipeline_config(packed_request, request_size,
                                     &packed_response, &response_size);
  if (status != 0) return status;

  status = bf_p4_write(packed_request, request_size, &packed_response,
                       &response_size);
  if (status != 0) return status;

  return 0;
}
