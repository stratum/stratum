// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/utils.h"

#include <utility>

#include "stratum/hal/lib/barefoot/bfrt_constants.h"
#include "stratum/public/lib/error.h"

namespace stratum {
namespace hal {
namespace barefoot {

::util::Status GetField(const bfrt::BfRtTableKey& table_key,
                        std::string field_name, uint64* field_value) {
  bf_rt_id_t field_id;
  const bfrt::BfRtTable* table;
  bfrt::DataType data_type;
  RETURN_IF_BFRT_ERROR(table_key.tableGet(&table));
  RETURN_IF_BFRT_ERROR(table->keyFieldIdGet(field_name, &field_id));
  RETURN_IF_BFRT_ERROR(table->keyFieldDataTypeGet(field_id, &data_type));
  CHECK_RETURN_IF_FALSE(data_type == bfrt::DataType::UINT64)
      << "Requested uint64 but field has type " << static_cast<int>(data_type);
  RETURN_IF_BFRT_ERROR(table_key.getValue(field_id, field_value));

  return ::util::OkStatus();
}

::util::Status SetField(bfrt::BfRtTableKey* table_key, std::string field_name,
                        uint64 value) {
  bf_rt_id_t field_id;
  const bfrt::BfRtTable* table;
  bfrt::DataType data_type;
  RETURN_IF_BFRT_ERROR(table_key->tableGet(&table));
  RETURN_IF_BFRT_ERROR(table->keyFieldIdGet(field_name, &field_id));
  RETURN_IF_BFRT_ERROR(table->keyFieldDataTypeGet(field_id, &data_type));
  CHECK_RETURN_IF_FALSE(data_type == bfrt::DataType::UINT64)
      << "Setting uint64 but field has type " << static_cast<int>(data_type);
  RETURN_IF_BFRT_ERROR(table_key->setValue(field_id, value));

  return ::util::OkStatus();
}

::util::Status GetField(const bfrt::BfRtTableData& table_data,
                        std::string field_name, uint64* field_value) {
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
  CHECK_RETURN_IF_FALSE(data_type == bfrt::DataType::UINT64)
      << "Requested uint64 but field has type " << static_cast<int>(data_type);
  RETURN_IF_BFRT_ERROR(table_data.getValue(field_id, field_value));

  return ::util::OkStatus();
}

::util::Status GetField(const bfrt::BfRtTableData& table_data,
                        std::string field_name, std::string* field_value) {
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
  CHECK_RETURN_IF_FALSE(data_type == bfrt::DataType::STRING)
      << "Requested string but field has type " << static_cast<int>(data_type);
  RETURN_IF_BFRT_ERROR(table_data.getValue(field_id, field_value));

  return ::util::OkStatus();
}

::util::Status GetField(const bfrt::BfRtTableData& table_data,
                        std::string field_name, bool* field_value) {
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
  CHECK_RETURN_IF_FALSE(data_type == bfrt::DataType::BOOL)
      << "Requested bool but field has type " << static_cast<int>(data_type);
  RETURN_IF_BFRT_ERROR(table_data.getValue(field_id, field_value));

  return ::util::OkStatus();
}

::util::Status SetField(bfrt::BfRtTableData* table_data, std::string field_name,
                        const uint64& value) {
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
  CHECK_RETURN_IF_FALSE(data_type == bfrt::DataType::UINT64)
      << "Setting uint64 but field has type " << static_cast<int>(data_type);
  RETURN_IF_BFRT_ERROR(table_data->setValue(field_id, value));

  return ::util::OkStatus();
}

::util::Status SetField(bfrt::BfRtTableData* table_data, std::string field_name,
                        const std::string& field_value) {
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
  CHECK_RETURN_IF_FALSE(data_type == bfrt::DataType::STRING)
      << "Setting string but field has type " << static_cast<int>(data_type);
  RETURN_IF_BFRT_ERROR(table_data->setValue(field_id, field_value));

  return ::util::OkStatus();
}

::util::Status SetFieldBool(bfrt::BfRtTableData* table_data,
                            std::string field_name, const bool& field_value) {
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
  CHECK_RETURN_IF_FALSE(data_type == bfrt::DataType::BOOL)
      << "Setting bool but field has type " << static_cast<int>(data_type);
  RETURN_IF_BFRT_ERROR(table_data->setValue(field_id, field_value));

  return ::util::OkStatus();
}

::util::Status GetAllEntries(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    bf_rt_target_t bf_dev_target, const bfrt::BfRtTable* table,
    std::vector<std::unique_ptr<bfrt::BfRtTableKey>>* table_keys,
    std::vector<std::unique_ptr<bfrt::BfRtTableData>>* table_datums) {
  CHECK_RETURN_IF_FALSE(table_keys) << "table_keys is null";
  CHECK_RETURN_IF_FALSE(table_datums) << "table_datums is null";

  // Get number of entries.
  uint32 entries;
  RETURN_IF_BFRT_ERROR(table->tableUsageGet(
      *bfrt_session, bf_dev_target,
      bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, &entries));

  table_keys->resize(0);
  table_datums->resize(0);
  if (entries == 0) return ::util::OkStatus();

  // Get first entry.
  {
    std::unique_ptr<bfrt::BfRtTableKey> table_key;
    std::unique_ptr<bfrt::BfRtTableData> table_data;
    RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
    RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));
    RETURN_IF_BFRT_ERROR(table->tableEntryGetFirst(
        *bfrt_session, bf_dev_target,
        bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, table_key.get(),
        table_data.get()));

    table_keys->push_back(std::move(table_key));
    table_datums->push_back(std::move(table_data));
  }
  if (entries == 1) return ::util::OkStatus();

  // Get all entries following the first.
  {
    std::vector<std::unique_ptr<bfrt::BfRtTableKey>> keys(entries - 1);
    std::vector<std::unique_ptr<bfrt::BfRtTableData>> data(keys.size());
    bfrt::BfRtTable::keyDataPairs pairs;
    for (size_t i = 0; i < keys.size(); ++i) {
      RETURN_IF_BFRT_ERROR(table->keyAllocate(&keys[i]));
      RETURN_IF_BFRT_ERROR(table->dataAllocate(&data[i]));
      pairs.push_back(std::make_pair(keys[i].get(), data[i].get()));
    }
    uint32 actual = 0;
    RETURN_IF_BFRT_ERROR(table->tableEntryGetNext_n(
        *bfrt_session, bf_dev_target, *(*table_keys)[0], pairs.size(),
        bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, &pairs, &actual));

    table_keys->insert(table_keys->end(), std::make_move_iterator(keys.begin()),
                       std::make_move_iterator(keys.end()));
    table_datums->insert(table_datums->end(),
                         std::make_move_iterator(data.begin()),
                         std::make_move_iterator(data.end()));
  }

  CHECK(table_keys->size() == table_datums->size());
  CHECK(table_keys->size() == entries);

  return ::util::OkStatus();
}


bool IsDontCareMatch(const ::p4::v1::FieldMatch::Exact& exact) { return false; }

bool IsDontCareMatch(const ::p4::v1::FieldMatch::LPM& lpm) {
  return lpm.prefix_len() == 0;
}

bool IsDontCareMatch(const ::p4::v1::FieldMatch::Ternary& ternary) {
  return std::all_of(ternary.mask().begin(), ternary.mask().end(),
                     [](const char c) { return c == '\x00'; });
}

// BFRT defines a "don't care" range match as all zeros for low and high,
// contrary to the P4RT definition.
bool IsDontCareMatch(const ::p4::v1::FieldMatch::Range& range,
                     int field_width) {
  return std::all_of(range.low().begin(), range.low().end(),
                     [](const char c) { return c == '\x00'; }) &&
         std::all_of(range.high().begin(), range.high().end(),
                     [](const char c) { return c == '\x00'; });
}

bool IsDontCareMatch(const ::p4::v1::FieldMatch::Optional& optional) {
  return false;
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
