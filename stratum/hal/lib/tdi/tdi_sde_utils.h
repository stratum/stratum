// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// Target-agnostic utility functions exposed for use outside TdiSdeWrapper.

#ifndef STRATUM_HAL_LIB_TDI_TDI_SDE_UTILS_H_
#define STRATUM_HAL_LIB_TDI_TDI_SDE_UTILS_H_

namespace tdi { class Table; }

namespace stratum {
namespace hal {
namespace tdi {

// Target-neutral SDE table types.
// Note that this is not a comprehensive set of table types.
// It consists solely of table types we need to be able to test for
// in vendor-neutral code.
enum tdi_sde_table_type {
    TDI_SDE_TABLE_TYPE_NONE = 0,
    TDI_SDE_TABLE_TYPE_ACTION_PROFILE,
    TDI_SDE_TABLE_TYPE_COUNTER,
    TDI_SDE_TABLE_TYPE_METER,
    TDI_SDE_TABLE_TYPE_SELECTOR,
};

tdi_sde_table_type GetSdeTableType(const ::tdi::Table& table);

bool IsPreallocatedTable(const ::tdi::Table& table);

}  // namespace tdi
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_TDI_TDI_SDE_UTILS_H_
