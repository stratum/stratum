// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0
#ifndef STRATUM_HAL_LIB_BAREFOOT_UTILS_H_
#define STRATUM_HAL_LIB_BAREFOOT_UTILS_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/synchronization/mutex.h"
#include "bf_rt/bf_rt_init.hpp"
#include "bf_rt/bf_rt_session.hpp"
#include "bf_rt/bf_rt_table_key.hpp"
#include "p4/v1/p4runtime.pb.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/barefoot/bfrt.pb.h"
#include "stratum/hal/lib/barefoot/macros.h"

namespace stratum {
namespace hal {
namespace barefoot {

// Helper function to extract a value from table keys.
::util::Status GetField(const bfrt::BfRtTableKey& table_key,
                        std::string field_name, uint64* field_value);

// Helper function to set a value in table keys.
::util::Status SetField(bfrt::BfRtTableKey* table_key, std::string field_name,
                        uint64 value);

// Helper functions to extract a value from table data.
::util::Status GetField(const bfrt::BfRtTableData& table_data,
                        std::string field_name, uint64* field_value);
::util::Status GetField(const bfrt::BfRtTableData& table_data,
                        std::string field_name, std::string* field_value);
::util::Status GetField(const bfrt::BfRtTableData& table_data,
                        std::string field_name, bool* field_value);
template <typename T>
::util::Status GetField(const bfrt::BfRtTableData& table_data,
                        std::string field_name, std::vector<T>* field_values) {
  bf_rt_id_t field_id;
  const bfrt::BfRtTable* table;
  bfrt::DataType data_type;
  RETURN_IF_BFRT_ERROR(table_data.getParent(&table));
  if (table->actionIdApplicable()) {
    bf_rt_id_t action_id;
    RETURN_IF_BFRT_ERROR(table_data.actionIdGet(&action_id));
    RETURN_IF_BFRT_ERROR(
        table->dataFieldIdGet(field_name, action_id, &field_id));
    RETURN_IF_BFRT_ERROR(
        table->dataFieldDataTypeGet(field_id, action_id, &data_type));
  } else {
    RETURN_IF_BFRT_ERROR(table->dataFieldIdGet(field_name, &field_id));
    RETURN_IF_BFRT_ERROR(table->dataFieldDataTypeGet(field_id, &data_type));
  }
  CHECK_RETURN_IF_FALSE(data_type == bfrt::DataType::INT_ARR ||
                        data_type == bfrt::DataType::BOOL_ARR)
      << "Requested array but field has type " << static_cast<int>(data_type);
  RETURN_IF_BFRT_ERROR(table_data.getValue(field_id, field_values));

  return ::util::OkStatus();
}

// Helper functions to set a value in table data.
::util::Status SetField(bfrt::BfRtTableData* table_data, std::string field_name,
                        const uint64& value);
::util::Status SetField(bfrt::BfRtTableData* table_data, std::string field_name,
                        const std::string& field_value);
// The function name is different to prevent unwanted type conversions.
::util::Status SetFieldBool(bfrt::BfRtTableData* table_data,
                            std::string field_name, const bool& field_value);
template <typename T>
::util::Status SetField(bfrt::BfRtTableData* table_data, std::string field_name,
                        const std::vector<T>& value) {
  bf_rt_id_t field_id;
  const bfrt::BfRtTable* table;
  bfrt::DataType data_type;
  RETURN_IF_BFRT_ERROR(table_data->getParent(&table));
  if (table->actionIdApplicable()) {
    bf_rt_id_t action_id;
    RETURN_IF_BFRT_ERROR(table_data->actionIdGet(&action_id));
    RETURN_IF_BFRT_ERROR(
        table->dataFieldIdGet(field_name, action_id, &field_id));
    RETURN_IF_BFRT_ERROR(
        table->dataFieldDataTypeGet(field_id, action_id, &data_type));
  } else {
    RETURN_IF_BFRT_ERROR(table->dataFieldIdGet(field_name, &field_id));
    RETURN_IF_BFRT_ERROR(table->dataFieldDataTypeGet(field_id, &data_type));
  }
  CHECK_RETURN_IF_FALSE(data_type == bfrt::DataType::INT_ARR ||
                        data_type == bfrt::DataType::BOOL_ARR)
      << "Requested array but field has type " << static_cast<int>(data_type);
  RETURN_IF_BFRT_ERROR(table_data->setValue(field_id, value));

  return ::util::OkStatus();
}

// Helper function to fetch all entries of a table. The length of the vectors
// will reflect the number of entries in the table. In case of errors, the state
// of table_keys and table datum is undefined.
::util::Status GetAllEntries(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    bf_rt_target_t bf_dev_target, const bfrt::BfRtTable* table,
    std::vector<std::unique_ptr<bfrt::BfRtTableKey>>* table_keys,
    std::vector<std::unique_ptr<bfrt::BfRtTableData>>* table_datums);

// A set of helper functions to determine whether a P4 match object constructed
// from a bfrt table key is a "don't care" match.
bool IsDontCareMatch(const ::p4::v1::FieldMatch::Exact& exact);
bool IsDontCareMatch(const ::p4::v1::FieldMatch::LPM& lpm);
bool IsDontCareMatch(const ::p4::v1::FieldMatch::Ternary& ternary);
// The field width is only taken as a upper bound, byte strings longer than that
// are not checked.
bool IsDontCareMatch(const ::p4::v1::FieldMatch::Range& range, int field_width);
// If the Optional match should be a wildcard, the FieldMatch must be omitted.
// Otherwise, this behaves like an exact match.
bool IsDontCareMatch(const ::p4::v1::FieldMatch::Optional& optional);

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_UTILS_H_
