// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// Target-agnostic utility functions exposed for use outside TdiSdeWrapper.

#include "stratum/hal/lib/tdi/tdi_sde_utils.h"
#include "tdi/common/tdi_table.hpp"
#include "tdi_rt/tdi_rt_defs.h"

namespace stratum {
namespace hal {
namespace tdi {

tdi_sde_table_type GetSdeTableType(const ::tdi::Table& table) {
  auto table_type =
      static_cast<tdi_rt_table_type_e>(table.tableInfoGet()->tableTypeGet());
  switch (table_type) {
    case TDI_RT_TABLE_TYPE_ACTION_PROFILE:
      return TDI_SDE_TABLE_TYPE_ACTION_PROFILE;
    case TDI_RT_TABLE_TYPE_COUNTER:
      return TDI_SDE_TABLE_TYPE_ACTION_PROFILE;
    case TDI_RT_TABLE_TYPE_METER:
      return TDI_SDE_TABLE_TYPE_METER;
    case TDI_RT_TABLE_TYPE_SELECTOR:
      return TDI_SDE_TABLE_TYPE_SELECTOR;
    default:
      return TDI_SDE_TABLE_TYPE_NONE;
  }
}

}  // namespace tdi
}  // namespace hal
}  // namespace stratum
