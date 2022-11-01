// Copyright 2019-present Barefoot Networks, Inc.
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// Helper functions for use within TdiSdeWrapper.

#ifndef STRATUM_HAL_LIB_TDI_TDI_SDE_HELPERS_H_
#define STRATUM_HAL_LIB_TDI_TDI_SDE_HELPERS_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/tdi/tdi_sde_common.h"
#include "stratum/hal/lib/tdi/tdi_sde_wrapper.h"

namespace stratum {
namespace hal {
namespace tdi {
namespace helpers {

// Convert kbit/s to bytes/s (* 1000 / 8).
inline constexpr uint64 KbitsToBytesPerSecond(uint64 kbps) {
  return kbps * 125;
}

// Convert bytes/s to kbit/s (/ 1000 * 8).
inline constexpr uint64 BytesPerSecondToKbits(uint64 bytes) {
  return bytes / 125;
}

// Target-agnostic helper functions

::util::StatusOr<std::string> DumpTableMetadata(const ::tdi::Table* table);

::util::StatusOr<std::string> DumpTableKey(const ::tdi::TableKey* table_key);

::util::StatusOr<std::string> DumpTableData(const ::tdi::TableData* table_data);

::util::Status GetFieldExact(const ::tdi::TableKey& table_key,
                             std::string field_name, uint32_t* field_value);

::util::Status SetFieldExact(::tdi::TableKey* table_key, std::string field_name,
                             uint64 field_value);

::util::Status SetField(::tdi::TableKey* table_key, std::string field_name,
                        ::tdi::KeyFieldValue value);

::util::Status GetField(const ::tdi::TableData& table_data,
                        std::string field_name, uint64* field_value);

::util::Status GetField(const ::tdi::TableData& table_data,
                        std::string field_name, std::string* field_value);

::util::Status GetFieldBool(const ::tdi::TableData& table_data,
                            std::string field_name, bool* field_value);

template <typename T>
::util::Status GetField(const ::tdi::TableData& table_data,
                        std::string field_name, std::vector<T>* field_values) {
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(table_data.getParent(&table));

  const ::tdi::DataFieldInfo* dataFieldInfo;
  tdi_id_t action_id = table_data.actionIdGet();
  dataFieldInfo = table->tableInfoGet()->dataFieldGet(field_name, action_id);
  RETURN_IF_NULL(dataFieldInfo);

  auto field_id = dataFieldInfo->idGet();
  auto data_type = dataFieldInfo->dataTypeGet();
  RET_CHECK(data_type == TDI_FIELD_DATA_TYPE_INT_ARR ||
            data_type == TDI_FIELD_DATA_TYPE_BOOL_ARR)
      << "Requested array but field has type " << static_cast<int>(data_type);
  RETURN_IF_TDI_ERROR(table_data.getValue(field_id, field_values));

  return ::util::OkStatus();
}

::util::Status SetField(::tdi::TableData* table_data, std::string field_name,
                        const uint64& value);

::util::Status SetField(::tdi::TableData* table_data, std::string field_name,
                        const std::string& field_value);

::util::Status SetFieldBool(::tdi::TableData* table_data,
                            std::string field_name, const bool& field_value);

template <typename T>
::util::Status SetField(::tdi::TableData* table_data, std::string field_name,
                        const std::vector<T>& field_value) {
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(table_data->getParent(&table));

  auto action_id = table_data->actionIdGet();
  auto dataFieldInfo =
      table->tableInfoGet()->dataFieldGet(field_name, action_id);
  RETURN_IF_NULL(dataFieldInfo);

  auto field_id = dataFieldInfo->idGet();
  auto data_type = dataFieldInfo->dataTypeGet();
  RET_CHECK(data_type == TDI_FIELD_DATA_TYPE_INT_ARR ||
            data_type == TDI_FIELD_DATA_TYPE_BOOL_ARR)
      << "Requested array but field has type " << static_cast<int>(data_type);

  RETURN_IF_TDI_ERROR(table_data->setValue(field_id, field_value));

  return ::util::OkStatus();
}

::util::Status GetAllEntries(
    std::shared_ptr<::tdi::Session> tdi_session, ::tdi::Target tdi_dev_target,
    const ::tdi::Table* table,
    std::vector<std::unique_ptr<::tdi::TableKey>>* table_keys,
    std::vector<std::unique_ptr<::tdi::TableData>>* table_values);

bool IsPreallocatedTable(const ::tdi::Table& table);

}  // namespace helpers
}  // namespace tdi
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_TDI_TDI_SDE_HELPERS_H_
