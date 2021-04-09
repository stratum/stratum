// Copyright 2018-2019 Barefoot Networks, Inc.
// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BAREFOOT_BF_INIT_H_
#define STRATUM_HAL_LIB_BAREFOOT_BF_INIT_H_

#ifdef __cplusplus
extern "C" {
#endif

// Initializes bf_switchd library. This must be called before creating
// the BfSdeWrapper.
int InitBfSwitchd(const char* bf_sde_install, const char* bf_switchd_cfg,
                  bool bf_switchd_background);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STRATUM_HAL_LIB_BAREFOOT_BF_INIT_H_
