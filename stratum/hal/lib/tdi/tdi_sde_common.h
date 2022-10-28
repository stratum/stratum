// Copyright 2019-present Barefoot Networks, Inc.
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_TDI_TDI_SDE_COMMON_H_
#define STRATUM_HAL_LIB_TDI_TDI_SDE_COMMON_H_

#include <cstddef>

#include "gflags/gflags_declare.h"
#include "stratum/glue/status/status_macros.h"

// Get the /sys fs file name of the first Tofino ASIC.
extern "C" int switch_pci_sysfs_str_get(char* name, size_t name_size);

#define RETURN_IF_NULL(expr)                                                 \
  do {                                                                       \
    if (expr == nullptr) {                                                   \
      return MAKE_ERROR() << "'" << #expr << "' must be non-null";           \
    }                                                                        \
  } while (0)

DECLARE_string(tdi_sde_config_dir);

namespace stratum {
namespace hal {
namespace tdi {

constexpr int _PI_UPDATE_MAX_NAME_SIZE = 100;

constexpr int MAX_PORT_HDL_STRING_LEN = 100;

}  // namespace tdi
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_TDI_TDI_SDE_COMMON_H_
