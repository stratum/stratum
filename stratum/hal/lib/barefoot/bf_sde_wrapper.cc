// Copyright 2019-present Barefoot Networks, Inc.
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bf_sde_wrapper.h"

#include <memory>
#include <set>
#include <utility>

#include "absl/strings/match.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "bf_rt/bf_rt_table_operations.hpp"
#include "lld/lld_sku.h"
#include "stratum/glue/gtl/cleanup.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/glue/gtl/stl_util.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/logging.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/barefoot/bfrt_constants.h"
#include "stratum/hal/lib/barefoot/macros.h"
#include "stratum/hal/lib/barefoot/utils.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/p4/utils.h"
#include "stratum/lib/channel/channel.h"
#include "stratum/lib/constants.h"
#include "stratum/lib/utils.h"

extern "C" {
#include "tofino/bf_pal/bf_pal_port_intf.h"
#include "tofino/bf_pal/dev_intf.h"
#include "tofino/bf_pal/pltfm_intf.h"
#include "tofino/pdfixed/pd_devport_mgr.h"
#include "tofino/pdfixed/pd_tm.h"
// Flag to enable detailed logging in the SDE pipe manager.
extern bool stat_mgr_enable_detail_trace;
}

DEFINE_string(bfrt_sde_config_dir, "/var/run/stratum/bfrt_config",
              "The dir used by the SDE to load the device configuration.");

namespace stratum {
namespace hal {
namespace barefoot {

constexpr absl::Duration BfSdeWrapper::kWriteTimeout;
constexpr int32 BfSdeWrapper::kBfDefaultMtu;
constexpr int _PI_UPDATE_MAX_NAME_SIZE = 100;

// Helper functions for dealing with the SDE API.
namespace {
// Convert kbit/s to bytes/s (* 1000 / 8).
inline constexpr uint64 KbitsToBytesPerSecond(uint64 kbps) {
  return kbps * 125;
}

// Convert bytes/s to kbit/s (/ 1000 * 8).
inline constexpr uint64 BytesPerSecondToKbits(uint64 bytes) {
  return bytes / 125;
}

::util::Status DumpTableKey(const bfrt::BfRtTableKey* table_key) {
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(table_key->tableGet(&table));
  std::vector<bf_rt_id_t> key_field_ids;
  RETURN_IF_BFRT_ERROR(table->keyFieldIdListGet(&key_field_ids));

  LOG(INFO) << "Table key {";
  for (const auto& field_id : key_field_ids) {
    std::string field_name;
    bfrt::KeyFieldType key_type;
    size_t field_size;
    RETURN_IF_BFRT_ERROR(table->keyFieldNameGet(field_id, &field_name));
    RETURN_IF_BFRT_ERROR(table->keyFieldTypeGet(field_id, &key_type));
    RETURN_IF_BFRT_ERROR(table->keyFieldSizeGet(field_id, &field_size));

    std::string value;
    switch (key_type) {
      case bfrt::KeyFieldType::EXACT: {
        std::string v(NumBitsToNumBytes(field_size), '\x00');
        RETURN_IF_BFRT_ERROR(table_key->getValue(
            field_id, v.size(),
            reinterpret_cast<uint8*>(gtl::string_as_array(&v))));
        value = StringToHex(v);
        break;
      }
      case bfrt::KeyFieldType::RANGE: {
        std::string l(NumBitsToNumBytes(field_size), '\x00');
        std::string h(NumBitsToNumBytes(field_size), '\x00');
        RETURN_IF_BFRT_ERROR(table_key->getValueRange(
            field_id, l.size(),
            reinterpret_cast<uint8*>(gtl::string_as_array(&l)),
            reinterpret_cast<uint8*>(gtl::string_as_array(&h))));
        value = absl::StrCat(StringToHex(l), " - ", StringToHex(h));
        break;
      }
      default:
        RETURN_ERROR(ERR_INTERNAL)
            << "Unknown key_type: " << static_cast<int>(key_type) << ".";
    }

    LOG(INFO) << "\t" << field_name << ": field_id: " << field_id
              << " key_type: " << static_cast<int>(key_type)
              << " field_size: " << field_size << " value: " << value;
  }
  LOG(INFO) << "}";

  return ::util::OkStatus();
}

::util::Status DumpTableData(const bfrt::BfRtTableData* table_data) {
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(table_data->getParent(&table));

  LOG(INFO) << "Table data {";
  std::vector<bf_rt_id_t> data_field_ids;
  if (table->actionIdApplicable()) {
    bf_rt_id_t action_id;
    RETURN_IF_BFRT_ERROR(table_data->actionIdGet(&action_id));
    LOG(INFO) << "\taction_id: " << action_id;
    RETURN_IF_BFRT_ERROR(table->dataFieldIdListGet(action_id, &data_field_ids));
  } else {
    RETURN_IF_BFRT_ERROR(table->dataFieldIdListGet(&data_field_ids));
  }

  for (const auto& field_id : data_field_ids) {
    std::string field_name;
    bfrt::DataType data_type;
    size_t field_size;
    bool is_active;
    if (table->actionIdApplicable()) {
      bf_rt_id_t action_id;
      RETURN_IF_BFRT_ERROR(table_data->actionIdGet(&action_id));
      RETURN_IF_BFRT_ERROR(
          table->dataFieldNameGet(field_id, action_id, &field_name));
      // FIXME
      // RETURN_IF_BFRT_ERROR(table->dataFieldDataTypeGet(field_id,
      // &data_type)); RETURN_IF_BFRT_ERROR(table->dataFieldSizeGet(field_id,
      // &field_size)); RETURN_IF_BFRT_ERROR(table_data->isActive(field_id,
      // &is_active));
    } else {
      RETURN_IF_BFRT_ERROR(table->dataFieldNameGet(field_id, &field_name));
      RETURN_IF_BFRT_ERROR(table->dataFieldDataTypeGet(field_id, &data_type));
      RETURN_IF_BFRT_ERROR(table->dataFieldSizeGet(field_id, &field_size));
      RETURN_IF_BFRT_ERROR(table_data->isActive(field_id, &is_active));
    }

    std::string value;
    switch (data_type) {
      case bfrt::DataType::UINT64: {
        uint64 v;
        RETURN_IF_BFRT_ERROR(table_data->getValue(field_id, &v));
        value = std::to_string(v);
        break;
      }
      case bfrt::DataType::BYTE_STREAM: {
        std::string v(NumBitsToNumBytes(field_size), '\x00');
        RETURN_IF_BFRT_ERROR(table_data->getValue(
            field_id, v.size(),
            reinterpret_cast<uint8*>(gtl::string_as_array(&v))));
        value = StringToHex(v);
        break;
      }
      case bfrt::DataType::INT_ARR: {
        std::vector<uint64_t> v;
        RETURN_IF_BFRT_ERROR(table_data->getValue(field_id, &v));
        value = PrintVector(v, ",");
        break;
      }
      default:
        RETURN_ERROR(ERR_INTERNAL)
            << "Unknown data_type: " << static_cast<int>(data_type) << ".";
    }

    LOG(INFO) << "\t" << field_name << ": field_id: " << field_id
              << " data_type: " << static_cast<int>(data_type)
              << " field_size: " << field_size << " value: " << value
              << " is_active: " << is_active;
  }
  LOG(INFO) << "}";

  return ::util::OkStatus();
}

::util::Status GetField(const bfrt::BfRtTableKey& table_key,
                        std::string field_name, uint64* field_value) {
  bf_rt_id_t field_id;
  const bfrt::BfRtTable* table;
  bfrt::DataType data_type;
  RETURN_IF_BFRT_ERROR(table_key.tableGet(&table));
  RETURN_IF_BFRT_ERROR(table->keyFieldIdGet(field_name, &field_id));
  RETURN_IF_BFRT_ERROR(table->keyFieldDataTypeGet(field_id, &data_type));
  CHECK_RETURN_IF_FALSE(data_type == bfrt::DataType::UINT64)
      << "Requested uint64 but field " << field_name << " has type "
      << static_cast<int>(data_type);
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
      << "Setting uint64 but field " << field_name << " has type "
      << static_cast<int>(data_type);
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
      << "Requested uint64 but field " << field_name << " has type "
      << static_cast<int>(data_type);
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
      << "Requested string but field " << field_name << " has type "
      << static_cast<int>(data_type);
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
      << "Requested bool but field " << field_name << " has type "
      << static_cast<int>(data_type);
  RETURN_IF_BFRT_ERROR(table_data.getValue(field_id, field_value));

  return ::util::OkStatus();
}

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
      << "Setting uint64 but field " << field_name << " has type "
      << static_cast<int>(data_type);
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
      << "Setting string but field " << field_name << " has type "
      << static_cast<int>(data_type);
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
      << "Setting bool but field " << field_name << " has type "
      << static_cast<int>(data_type);
  RETURN_IF_BFRT_ERROR(table_data->setValue(field_id, field_value));

  return ::util::OkStatus();
}

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

::util::Status GetAllEntries(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    bf_rt_target_t bf_dev_target, const bfrt::BfRtTable* table,
    std::vector<std::unique_ptr<bfrt::BfRtTableKey>>* table_keys,
    std::vector<std::unique_ptr<bfrt::BfRtTableData>>* table_datums) {
  CHECK_RETURN_IF_FALSE(table_keys) << "table_keys is null";
  CHECK_RETURN_IF_FALSE(table_datums) << "table_datums is null";

  // Get number of entries. Some types of tables are preallocated and are always
  // "full". The SDE does not support querying the usage on these.
  uint32 entries;
  bfrt::BfRtTable::TableType table_type;
  RETURN_IF_BFRT_ERROR(table->tableTypeGet(&table_type));
  if (table_type == bfrt::BfRtTable::TableType::METER ||
      table_type == bfrt::BfRtTable::TableType::COUNTER) {
    size_t table_size;
#if defined(SDE_9_4_0)
    RETURN_IF_BFRT_ERROR(
        table->tableSizeGet(*bfrt_session, bf_dev_target, &table_size));
#else
    RETURN_IF_BFRT_ERROR(table->tableSizeGet(&table_size));
#endif  // SDE_9_4_0
    entries = table_size;
  } else {
    RETURN_IF_BFRT_ERROR(table->tableUsageGet(
        *bfrt_session, bf_dev_target,
        bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, &entries));
  }

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

}  // namespace

::util::Status TableKey::SetExact(int id, const std::string& value) {
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(table_key_->tableGet(&table));
  size_t field_size_bits;
  RETURN_IF_BFRT_ERROR(table->keyFieldSizeGet(id, &field_size_bits));
  std::string v = P4RuntimeByteStringToPaddedByteString(
      value, NumBitsToNumBytes(field_size_bits));
  RETURN_IF_BFRT_ERROR(table_key_->setValue(
      id, reinterpret_cast<const uint8*>(v.data()), v.size()));

  return ::util::OkStatus();
}

::util::Status TableKey::SetTernary(int id, const std::string& value,
                                    const std::string& mask) {
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(table_key_->tableGet(&table));
  size_t field_size_bits;
  RETURN_IF_BFRT_ERROR(table->keyFieldSizeGet(id, &field_size_bits));
  std::string v = P4RuntimeByteStringToPaddedByteString(
      value, NumBitsToNumBytes(field_size_bits));
  std::string m = P4RuntimeByteStringToPaddedByteString(
      mask, NumBitsToNumBytes(field_size_bits));
  DCHECK_EQ(v.size(), m.size());
  RETURN_IF_BFRT_ERROR(table_key_->setValueandMask(
      id, reinterpret_cast<const uint8*>(v.data()),
      reinterpret_cast<const uint8*>(m.data()), v.size()));

  return ::util::OkStatus();
}

::util::Status TableKey::SetLpm(int id, const std::string& prefix,
                                uint16 prefix_length) {
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(table_key_->tableGet(&table));
  size_t field_size_bits;
  RETURN_IF_BFRT_ERROR(table->keyFieldSizeGet(id, &field_size_bits));
  std::string p = P4RuntimeByteStringToPaddedByteString(
      prefix, NumBitsToNumBytes(field_size_bits));
  RETURN_IF_BFRT_ERROR(table_key_->setValueLpm(
      id, reinterpret_cast<const uint8*>(p.data()), prefix_length, p.size()));

  return ::util::OkStatus();
}

::util::Status TableKey::SetRange(int id, const std::string& low,
                                  const std::string& high) {
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(table_key_->tableGet(&table));
  size_t field_size_bits;
  RETURN_IF_BFRT_ERROR(table->keyFieldSizeGet(id, &field_size_bits));
  std::string l = P4RuntimeByteStringToPaddedByteString(
      low, NumBitsToNumBytes(field_size_bits));
  std::string h = P4RuntimeByteStringToPaddedByteString(
      high, NumBitsToNumBytes(field_size_bits));
  DCHECK_EQ(l.size(), h.size());
  RETURN_IF_BFRT_ERROR(table_key_->setValueRange(
      id, reinterpret_cast<const uint8*>(l.data()),
      reinterpret_cast<const uint8*>(h.data()), l.size()));

  return ::util::OkStatus();
}

::util::Status TableKey::SetPriority(uint32 priority) {
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(table_key_->tableGet(&table));
  bf_rt_id_t priority_field_id;
  RETURN_IF_BFRT_ERROR(
      table->keyFieldIdGet("$MATCH_PRIORITY", &priority_field_id));
  RETURN_IF_BFRT_ERROR(table_key_->setValue(priority_field_id, priority));

  return ::util::OkStatus();
}

::util::Status TableKey::GetExact(int id, std::string* value) const {
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(table_key_->tableGet(&table));
  size_t field_size_bits;
  RETURN_IF_BFRT_ERROR(table->keyFieldSizeGet(id, &field_size_bits));
  value->clear();
  value->resize(NumBitsToNumBytes(field_size_bits));
  RETURN_IF_BFRT_ERROR(table_key_->getValue(
      id, value->size(),
      reinterpret_cast<uint8*>(gtl::string_as_array(value))));

  return ::util::OkStatus();
}

::util::Status TableKey::GetTernary(int id, std::string* value,
                                    std::string* mask) const {
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(table_key_->tableGet(&table));
  size_t field_size_bits;
  RETURN_IF_BFRT_ERROR(table->keyFieldSizeGet(id, &field_size_bits));
  value->clear();
  value->resize(NumBitsToNumBytes(field_size_bits));
  mask->clear();
  mask->resize(NumBitsToNumBytes(field_size_bits));
  RETURN_IF_BFRT_ERROR(table_key_->getValueandMask(
      id, value->size(), reinterpret_cast<uint8*>(gtl::string_as_array(value)),
      reinterpret_cast<uint8*>(gtl::string_as_array(mask))));

  return ::util::OkStatus();
}

::util::Status TableKey::GetLpm(int id, std::string* prefix,
                                uint16* prefix_length) const {
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(table_key_->tableGet(&table));
  size_t field_size_bits;
  RETURN_IF_BFRT_ERROR(table->keyFieldSizeGet(id, &field_size_bits));
  prefix->clear();
  prefix->resize(NumBitsToNumBytes(field_size_bits));
  RETURN_IF_BFRT_ERROR(table_key_->getValueLpm(
      id, prefix->size(),
      reinterpret_cast<uint8*>(gtl::string_as_array(prefix)), prefix_length));

  return ::util::OkStatus();
}

::util::Status TableKey::GetRange(int id, std::string* low,
                                  std::string* high) const {
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(table_key_->tableGet(&table));
  size_t field_size_bits;
  RETURN_IF_BFRT_ERROR(table->keyFieldSizeGet(id, &field_size_bits));
  low->clear();
  low->resize(NumBitsToNumBytes(field_size_bits));
  high->clear();
  high->resize(NumBitsToNumBytes(field_size_bits));
  RETURN_IF_BFRT_ERROR(table_key_->getValueRange(
      id, low->size(), reinterpret_cast<uint8*>(gtl::string_as_array(low)),
      reinterpret_cast<uint8*>(gtl::string_as_array(high))));

  return ::util::OkStatus();
}

::util::Status TableKey::GetPriority(uint32* priority) const {
  // const bfrt::BfRtTable* table;
  // RETURN_IF_BFRT_ERROR(table_key_->tableGet(&table));
  uint64 bf_priority;
  RETURN_IF_ERROR(GetField(*table_key_, "$MATCH_PRIORITY", &bf_priority));
  *priority = bf_priority;

  return ::util::OkStatus();
}

::util::StatusOr<std::unique_ptr<BfSdeInterface::TableKeyInterface>>
TableKey::CreateTableKey(const bfrt::BfRtInfo* bfrt_info_, int table_id) {
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));
  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  auto key = std::unique_ptr<BfSdeInterface::TableKeyInterface>(
      new TableKey(std::move(table_key)));
  return key;
}

::util::Status TableData::SetParam(int id, const std::string& value) {
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(table_data_->getParent(&table));
  bf_rt_id_t action_id = 0;
  if (table->actionIdApplicable()) {
    RETURN_IF_BFRT_ERROR(table_data_->actionIdGet(&action_id));
  }
  size_t field_size_bits;
  if (action_id) {
    RETURN_IF_BFRT_ERROR(
        table->dataFieldSizeGet(id, action_id, &field_size_bits));
  } else {
    RETURN_IF_BFRT_ERROR(table->dataFieldSizeGet(id, &field_size_bits));
  }
  std::string p = P4RuntimeByteStringToPaddedByteString(
      value, NumBitsToNumBytes(field_size_bits));
  RETURN_IF_BFRT_ERROR(table_data_->setValue(
      id, reinterpret_cast<const uint8*>(p.data()), p.size()));

  return ::util::OkStatus();
}

::util::Status TableData::GetParam(int id, std::string* value) const {
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(table_data_->getParent(&table));
  bf_rt_id_t action_id = 0;
  if (table->actionIdApplicable()) {
    RETURN_IF_BFRT_ERROR(table_data_->actionIdGet(&action_id));
  }
  size_t field_size_bits;
  if (action_id) {
    RETURN_IF_BFRT_ERROR(
        table->dataFieldSizeGet(id, action_id, &field_size_bits));
  } else {
    RETURN_IF_BFRT_ERROR(table->dataFieldSizeGet(id, &field_size_bits));
  }
  value->clear();
  value->resize(NumBitsToNumBytes(field_size_bits));
  RETURN_IF_BFRT_ERROR(table_data_->getValue(
      id, value->size(),
      reinterpret_cast<uint8*>(gtl::string_as_array(value))));

  return ::util::OkStatus();
}

::util::Status TableData::SetActionMemberId(uint64 action_member_id) {
  return SetField(table_data_.get(), "$ACTION_MEMBER_ID", action_member_id);
}

::util::Status TableData::GetActionMemberId(uint64* action_member_id) const {
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(table_data_->getParent(&table));
  // Here we assume that table entries with action IDs (direct match-action) can
  // never hold action member or group IDs (indirect match-action). Since this
  // function is regularly called on both, we do not log this error here.
  if (table->actionIdApplicable()) {
    return MAKE_ERROR(ERR_ENTRY_NOT_FOUND).without_logging()
           << "This direct table does not contain action member IDs.";
  }
  bf_rt_id_t field_id;
  bfrt::DataType data_type;
  RETURN_IF_BFRT_ERROR(table->dataFieldIdGet("$ACTION_MEMBER_ID", &field_id));
  RETURN_IF_BFRT_ERROR(table->dataFieldDataTypeGet(field_id, &data_type));
  CHECK_RETURN_IF_FALSE(data_type == bfrt::DataType::UINT64)
      << "Requested uint64 but field $ACTION_MEMBER_ID has type "
      << static_cast<int>(data_type);
  bool is_active;
  RETURN_IF_BFRT_ERROR(table_data_->isActive(field_id, &is_active));
  if (!is_active) {
    RETURN_ERROR(ERR_ENTRY_NOT_FOUND).without_logging()
        << "Field $ACTION_MEMBER_ID is not active.";
  }
  RETURN_IF_BFRT_ERROR(table_data_->getValue(field_id, action_member_id));

  return ::util::OkStatus();
}

::util::Status TableData::SetSelectorGroupId(uint64 selector_group_id) {
  return SetField(table_data_.get(), "$SELECTOR_GROUP_ID", selector_group_id);
}

::util::Status TableData::GetSelectorGroupId(uint64* selector_group_id) const {
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(table_data_->getParent(&table));
  // Here we assume that table entries with action IDs (direct match-action) can
  // never hold action member or group IDs (indirect match-action). Since this
  // function is regularly called on both, we do not log this error here.
  if (table->actionIdApplicable()) {
    return MAKE_ERROR(ERR_ENTRY_NOT_FOUND).without_logging()
           << "This direct table does not contain action group IDs.";
  }
  bf_rt_id_t field_id;
  bfrt::DataType data_type;
  RETURN_IF_BFRT_ERROR(table->dataFieldIdGet("$SELECTOR_GROUP_ID", &field_id));
  RETURN_IF_BFRT_ERROR(table->dataFieldDataTypeGet(field_id, &data_type));
  CHECK_RETURN_IF_FALSE(data_type == bfrt::DataType::UINT64)
      << "Requested uint64 but field $SELECTOR_GROUP_ID has type "
      << static_cast<int>(data_type);
  bool is_active;
  RETURN_IF_BFRT_ERROR(table_data_->isActive(field_id, &is_active));
  if (!is_active) {
    RETURN_ERROR(ERR_ENTRY_NOT_FOUND).without_logging()
        << "Field $SELECTOR_GROUP_ID is not active.";
  }
  RETURN_IF_BFRT_ERROR(table_data_->getValue(field_id, selector_group_id));

  return ::util::OkStatus();
}

::util::Status TableData::SetOnlyCounterData(uint64 bytes, uint64 packets) {
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(table_data_->getParent(&table));

  bf_rt_id_t action_id = 0;
  if (table->actionIdApplicable()) {
    RETURN_IF_BFRT_ERROR(table_data_->actionIdGet(&action_id));
  }
  if (!action_id) {
    bf_rt_id_t table_id;
    table->tableIdGet(&table_id);
    std::string table_name;
    table->tableNameGet(&table_name);
    LOG(WARNING) << "Trying to set counter data on a table entry without "
                 << "action ID. This might not behave as expected, please "
                 << "report this to the Stratum authors: table_id " << table_id
                 << " table_name " << table_name << ".";
  }
  std::vector<bf_rt_id_t> ids;
  bf_rt_id_t field_id_bytes;
  bf_status_t has_bytes =
      table->dataFieldIdGet("$COUNTER_SPEC_BYTES", action_id, &field_id_bytes);
  if (has_bytes == BF_SUCCESS) {
    ids.push_back(field_id_bytes);
  }
  bf_rt_id_t field_id_packets;
  bf_status_t has_packets =
      table->dataFieldIdGet("$COUNTER_SPEC_PKTS", action_id, &field_id_packets);
  if (has_packets == BF_SUCCESS) {
    ids.push_back(field_id_packets);
  }
  if (action_id) {
    RETURN_IF_BFRT_ERROR(table->dataReset(ids, action_id, table_data_.get()));
  } else {
    RETURN_IF_BFRT_ERROR(table->dataReset(ids, table_data_.get()));
  }
  if (has_bytes == BF_SUCCESS) {
    RETURN_IF_BFRT_ERROR(table_data_->setValue(field_id_bytes, bytes));
  }
  if (has_packets == BF_SUCCESS) {
    RETURN_IF_BFRT_ERROR(table_data_->setValue(field_id_packets, packets));
  }

  return ::util::OkStatus();
}

::util::Status TableData::SetCounterData(uint64 bytes, uint64 packets) {
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(table_data_->getParent(&table));

  bf_rt_id_t action_id = 0;
  if (table->actionIdApplicable()) {
    RETURN_IF_BFRT_ERROR(table_data_->actionIdGet(&action_id));
  }
  if (!action_id) {
    bf_rt_id_t table_id;
    table->tableIdGet(&table_id);
    std::string table_name;
    table->tableNameGet(&table_name);
    LOG(WARNING) << "Trying to set counter data on a table entry without "
                 << "action ID. This might not behave as expected, please "
                 << "report this to the Stratum authors: table_id " << table_id
                 << " table_name " << table_name << ".";
  }
  bf_rt_id_t field_id_bytes;
  bf_status_t has_bytes =
      table->dataFieldIdGet("$COUNTER_SPEC_BYTES", action_id, &field_id_bytes);
  bf_rt_id_t field_id_packets;
  bf_status_t has_packets =
      table->dataFieldIdGet("$COUNTER_SPEC_PKTS", action_id, &field_id_packets);
  if (has_bytes == BF_SUCCESS) {
    RETURN_IF_BFRT_ERROR(table_data_->setValue(field_id_bytes, bytes));
  }
  if (has_packets == BF_SUCCESS) {
    RETURN_IF_BFRT_ERROR(table_data_->setValue(field_id_packets, packets));
  }

  return ::util::OkStatus();
}

::util::Status TableData::GetCounterData(uint64* bytes, uint64* packets) const {
  CHECK_RETURN_IF_FALSE(bytes);
  CHECK_RETURN_IF_FALSE(packets);
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(table_data_->getParent(&table));

  bf_rt_id_t action_id = 0;
  if (table->actionIdApplicable()) {
    RETURN_IF_BFRT_ERROR(table_data_->actionIdGet(&action_id));
  }

  bf_rt_id_t field_id;
  // Try to read byte counter.
  bf_status_t bf_status;
  if (action_id) {
    bf_status =
        table->dataFieldIdGet("$COUNTER_SPEC_BYTES", action_id, &field_id);
  } else {
    bf_status = table->dataFieldIdGet("$COUNTER_SPEC_BYTES", &field_id);
  }
  if (bf_status == BF_SUCCESS) {
    uint64 counter_val;
    RETURN_IF_BFRT_ERROR(table_data_->getValue(field_id, &counter_val));
    *bytes = counter_val;
  }

  // Try to read packet counter.
  if (action_id) {
    bf_status =
        table->dataFieldIdGet("$COUNTER_SPEC_PKTS", action_id, &field_id);
  } else {
    bf_status = table->dataFieldIdGet("$COUNTER_SPEC_PKTS", &field_id);
  }
  if (bf_status == BF_SUCCESS) {
    uint64 counter_val;
    RETURN_IF_BFRT_ERROR(table_data_->getValue(field_id, &counter_val));
    *packets = counter_val;
  }

  return ::util::OkStatus();
}

::util::Status TableData::GetActionId(int* action_id) const {
  CHECK_RETURN_IF_FALSE(action_id);
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(table_data_->getParent(&table));
  bf_rt_id_t bf_action_id = 0;
  if (table->actionIdApplicable()) {
    RETURN_IF_BFRT_ERROR(table_data_->actionIdGet(&bf_action_id));
  }
  *action_id = bf_action_id;

  return ::util::OkStatus();
}

::util::Status TableData::Reset(int action_id) {
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(table_data_->getParent(&table));
  if (action_id) {
    RETURN_IF_BFRT_ERROR(table->dataReset(action_id, table_data_.get()));
  } else {
    RETURN_IF_BFRT_ERROR(table->dataReset(table_data_.get()));
  }

  return ::util::OkStatus();
}

::util::StatusOr<std::unique_ptr<BfSdeInterface::TableDataInterface>>
TableData::CreateTableData(const bfrt::BfRtInfo* bfrt_info_, int table_id,
                           int action_id) {
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  if (action_id) {
    RETURN_IF_BFRT_ERROR(table->dataAllocate(action_id, &table_data));
  } else {
    RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));
  }
  auto data = std::unique_ptr<BfSdeInterface::TableDataInterface>(
      new TableData(std::move(table_data)));
  return data;
}

namespace {

// A callback function executed in SDE port state change thread context.
bf_status_t sde_port_status_callback(bf_dev_id_t device, bf_dev_port_t dev_port,
                                     bool up, void* cookie) {
  BfSdeWrapper* bf_sde_wrapper = BfSdeWrapper::GetSingleton();
  if (!bf_sde_wrapper) {
    LOG(ERROR) << "BfSdeWrapper singleton instance is not initialized.";
    return BF_INTERNAL_ERROR;
  }
  // Forward the event.
  auto status = bf_sde_wrapper->OnPortStatusEvent(device, dev_port, up);

  return status.ok() ? BF_SUCCESS : BF_INTERNAL_ERROR;
}

::util::StatusOr<bf_port_speed_t> PortSpeedHalToBf(uint64 speed_bps) {
  switch (speed_bps) {
    case kOneGigBps:
      return BF_SPEED_1G;
    case kTenGigBps:
      return BF_SPEED_10G;
    case kTwentyFiveGigBps:
      return BF_SPEED_25G;
    case kFortyGigBps:
      return BF_SPEED_40G;
    case kFiftyGigBps:
      return BF_SPEED_50G;
    case kHundredGigBps:
      return BF_SPEED_100G;
    default:
      RETURN_ERROR(ERR_INVALID_PARAM) << "Unsupported port speed.";
  }
}

::util::StatusOr<int> AutonegHalToBf(TriState autoneg) {
  switch (autoneg) {
    case TRI_STATE_UNKNOWN:
      return 0;
    case TRI_STATE_TRUE:
      return 1;
    case TRI_STATE_FALSE:
      return 2;
    default:
      RETURN_ERROR(ERR_INVALID_PARAM) << "Invalid autoneg state.";
  }
}

::util::StatusOr<bf_fec_type_t> FecModeHalToBf(FecMode fec_mode,
                                               uint64 speed_bps) {
  if (fec_mode == FEC_MODE_UNKNOWN || fec_mode == FEC_MODE_OFF) {
    return BF_FEC_TYP_NONE;
  } else if (fec_mode == FEC_MODE_ON || fec_mode == FEC_MODE_AUTO) {
    // we have to "guess" the FEC type to use based on the port speed.
    switch (speed_bps) {
      case kOneGigBps:
        RETURN_ERROR(ERR_INVALID_PARAM) << "Invalid FEC mode for 1Gbps mode.";
      case kTenGigBps:
      case kFortyGigBps:
        return BF_FEC_TYP_FIRECODE;
      case kTwentyFiveGigBps:
      case kFiftyGigBps:
      case kHundredGigBps:
      case kTwoHundredGigBps:
      case kFourHundredGigBps:
        return BF_FEC_TYP_REED_SOLOMON;
      default:
        RETURN_ERROR(ERR_INVALID_PARAM) << "Unsupported port speed.";
    }
  }
  RETURN_ERROR(ERR_INVALID_PARAM) << "Invalid FEC mode.";
}

::util::StatusOr<bf_loopback_mode_e> LoopbackModeToBf(
    LoopbackState loopback_mode) {
  switch (loopback_mode) {
    case LOOPBACK_STATE_NONE:
      return BF_LPBK_NONE;
    case LOOPBACK_STATE_MAC:
      return BF_LPBK_MAC_NEAR;
    default:
      RETURN_ERROR(ERR_INVALID_PARAM)
          << "Unsupported loopback mode: " << LoopbackState_Name(loopback_mode)
          << ".";
  }
}

}  // namespace

BfSdeWrapper* BfSdeWrapper::singleton_ = nullptr;
ABSL_CONST_INIT absl::Mutex BfSdeWrapper::init_lock_(absl::kConstInit);

BfSdeWrapper::BfSdeWrapper() : port_status_event_writer_(nullptr) {}

::util::StatusOr<PortState> BfSdeWrapper::GetPortState(int device, int port) {
  int state;
  RETURN_IF_BFRT_ERROR(
      bf_pal_port_oper_state_get(static_cast<bf_dev_id_t>(device),
                                 static_cast<bf_dev_port_t>(port), &state));
  return state ? PORT_STATE_UP : PORT_STATE_DOWN;
}

::util::Status BfSdeWrapper::GetPortCounters(int device, int port,
                                             PortCounters* counters) {
  uint64_t stats[BF_NUM_RMON_COUNTERS];
  RETURN_IF_BFRT_ERROR(
      bf_pal_port_all_stats_get(static_cast<bf_dev_id_t>(device),
                                static_cast<bf_dev_port_t>(port), stats));
  counters->set_in_octets(stats[bf_mac_stat_OctetsReceived]);
  counters->set_out_octets(stats[bf_mac_stat_OctetsTransmittedTotal]);
  counters->set_in_unicast_pkts(
      stats[bf_mac_stat_FramesReceivedwithUnicastAddresses]);
  counters->set_out_unicast_pkts(stats[bf_mac_stat_FramesTransmittedUnicast]);
  counters->set_in_broadcast_pkts(
      stats[bf_mac_stat_FramesReceivedwithBroadcastAddresses]);
  counters->set_out_broadcast_pkts(
      stats[bf_mac_stat_FramesTransmittedBroadcast]);
  counters->set_in_multicast_pkts(
      stats[bf_mac_stat_FramesReceivedwithMulticastAddresses]);
  counters->set_out_multicast_pkts(
      stats[bf_mac_stat_FramesTransmittedMulticast]);
  counters->set_in_discards(stats[bf_mac_stat_FramesDroppedBufferFull]);
  counters->set_out_discards(0);       // stat not available
  counters->set_in_unknown_protos(0);  // stat not meaningful
  counters->set_in_errors(stats[bf_mac_stat_FrameswithanyError]);
  counters->set_out_errors(stats[bf_mac_stat_FramesTransmittedwithError]);
  counters->set_in_fcs_errors(stats[bf_mac_stat_FramesReceivedwithFCSError]);

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::OnPortStatusEvent(int device, int port, bool up) {
  // Create PortStatusEvent message.
  PortState state = up ? PORT_STATE_UP : PORT_STATE_DOWN;
  PortStatusEvent event = {device, port, state};

  {
    absl::ReaderMutexLock l(&port_status_event_writer_lock_);
    if (!port_status_event_writer_) {
      return ::util::OkStatus();
    }
    return port_status_event_writer_->Write(event, kWriteTimeout);
  }
}

::util::Status BfSdeWrapper::RegisterPortStatusEventWriter(
    std::unique_ptr<ChannelWriter<PortStatusEvent>> writer) {
  absl::WriterMutexLock l(&port_status_event_writer_lock_);
  port_status_event_writer_ = std::move(writer);
  RETURN_IF_BFRT_ERROR(
      bf_pal_port_status_notif_reg(sde_port_status_callback, nullptr));
  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::UnregisterPortStatusEventWriter() {
  absl::WriterMutexLock l(&port_status_event_writer_lock_);
  port_status_event_writer_ = nullptr;
  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::AddPort(int device, int port, uint64 speed_bps,
                                     FecMode fec_mode) {
  ASSIGN_OR_RETURN(auto bf_speed, PortSpeedHalToBf(speed_bps));
  ASSIGN_OR_RETURN(auto bf_fec_mode, FecModeHalToBf(fec_mode, speed_bps));
  RETURN_IF_BFRT_ERROR(bf_pal_port_add(static_cast<bf_dev_id_t>(device),
                                       static_cast<bf_dev_port_t>(port),
                                       bf_speed, bf_fec_mode));
  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::DeletePort(int device, int port) {
  RETURN_IF_BFRT_ERROR(bf_pal_port_del(static_cast<bf_dev_id_t>(device),
                                       static_cast<bf_dev_port_t>(port)));
  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::EnablePort(int device, int port) {
  RETURN_IF_BFRT_ERROR(bf_pal_port_enable(static_cast<bf_dev_id_t>(device),
                                          static_cast<bf_dev_port_t>(port)));
  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::DisablePort(int device, int port) {
  RETURN_IF_BFRT_ERROR(bf_pal_port_disable(static_cast<bf_dev_id_t>(device),
                                           static_cast<bf_dev_port_t>(port)));
  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::SetPortShapingRate(int device, int port,
                                                bool is_in_pps,
                                                uint32 burst_size,
                                                uint64 rate_per_second) {
  if (!is_in_pps) {
    rate_per_second /= 1000;  // The SDE expects the bitrate in kbps.
  }

  RETURN_IF_BFRT_ERROR(p4_pd_tm_set_port_shaping_rate(
      device, port, is_in_pps, burst_size, rate_per_second));

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::EnablePortShaping(int device, int port,
                                               TriState enable) {
  if (enable == TriState::TRI_STATE_TRUE) {
    RETURN_IF_BFRT_ERROR(p4_pd_tm_enable_port_shaping(device, port));
  } else if (enable == TriState::TRI_STATE_FALSE) {
    RETURN_IF_BFRT_ERROR(p4_pd_tm_disable_port_shaping(device, port));
  }

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::SetPortAutonegPolicy(int device, int port,
                                                  TriState autoneg) {
  ASSIGN_OR_RETURN(auto autoneg_v, AutonegHalToBf(autoneg));
  RETURN_IF_BFRT_ERROR(bf_pal_port_autoneg_policy_set(
      static_cast<bf_dev_id_t>(device), static_cast<bf_dev_port_t>(port),
      autoneg_v));
  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::SetPortMtu(int device, int port, int32 mtu) {
  if (mtu < 0) {
    RETURN_ERROR(ERR_INVALID_PARAM) << "Invalid MTU value.";
  }
  if (mtu == 0) mtu = kBfDefaultMtu;
  RETURN_IF_BFRT_ERROR(bf_pal_port_mtu_set(
      static_cast<bf_dev_id_t>(device), static_cast<bf_dev_port_t>(port),
      static_cast<uint32>(mtu), static_cast<uint32>(mtu)));
  return ::util::OkStatus();
}

bool BfSdeWrapper::IsValidPort(int device, int port) {
  return bf_pal_port_is_valid(device, port) == BF_SUCCESS;
}

::util::Status BfSdeWrapper::SetPortLoopbackMode(int device, int port,
                                                 LoopbackState loopback_mode) {
  if (loopback_mode == LOOPBACK_STATE_UNKNOWN) {
    // Do nothing if we try to set loopback mode to the default one (UNKNOWN).
    return ::util::OkStatus();
  }
  ASSIGN_OR_RETURN(bf_loopback_mode_e lp_mode, LoopbackModeToBf(loopback_mode));
  RETURN_IF_BFRT_ERROR(
      bf_pal_port_loopback_mode_set(static_cast<bf_dev_id_t>(device),
                                    static_cast<bf_dev_port_t>(port), lp_mode));

  return ::util::OkStatus();
}

::util::StatusOr<bool> BfSdeWrapper::IsSoftwareModel(int device) {
  bool is_sw_model;
  auto bf_status = bf_pal_pltfm_type_get(device, &is_sw_model);
  CHECK_RETURN_IF_FALSE(bf_status == BF_SUCCESS)
      << "Error getting software model status.";

  return is_sw_model;
}

std::string BfSdeWrapper::GetBfChipType(int device) const {
  bf_dev_type_t dev_type = lld_sku_get_dev_type(device);
  switch (dev_type) {
    case BF_DEV_BFNT10064Q:
      return "TOFINO_64Q";
    case BF_DEV_BFNT10032Q:
      return "TOFINO_32Q";
    case BF_DEV_BFNT10032D:
      return "TOFINO_32D";
    case BF_DEV_BFNT10024D:
      return "TOFINO_24D";
    case BF_DEV_BFNT10018Q:
      return "TOFINO_18Q";
    case BF_DEV_BFNT10018D:
      return "TOFINO_18D";
    case BF_DEV_BFNT10017D:
      return "TOFINO_17D";
    case BF_DEV_BFNT20128Q:
      return "TOFINO2_128Q";
#ifdef BF_DEV_BFNT20128QM
    case BF_DEV_BFNT20128QM:  // added in 9.3.0
      return "TOFINO2_128QM";
#endif  // BF_DEV_BFNT20128QM
#ifdef BF_DEV_BFNT20128QH
    case BF_DEV_BFNT20128QH:  // added in 9.3.0
      return "TOFINO2_128QH";
#endif  // BF_DEV_BFNT20128QH
    case BF_DEV_BFNT20096T:
      return "TOFINO2_96T";
    case BF_DEV_BFNT20080T:
      return "TOFINO2_80T";
#ifdef BF_DEV_BFNT20080TM
    case BF_DEV_BFNT20080TM:  // added in 9.3.0
      return "TOFINO2_80TM";
#endif  // BF_DEV_BFNT20080TM
    case BF_DEV_BFNT20064Q:
      return "TOFINO2_64Q";
    case BF_DEV_BFNT20064D:
      return "TOFINO2_64D";
#ifdef BF_DEV_BFNT20032D
    case BF_DEV_BFNT20032D:  // removed in 9.3.0
      return "TOFINO2_32D";
#endif  // BF_DEV_BFNT20032D
#ifdef BF_DEV_BFNT20032S
    case BF_DEV_BFNT20032S:  // removed in 9.3.0
      return "TOFINO2_32S";
#endif  // BF_DEV_BFNT20032S
    case BF_DEV_BFNT20048D:
      return "TOFINO2_48D";
#ifdef BF_DEV_BFNT20036D
    case BF_DEV_BFNT20036D:  // removed in 9.3.0
      return "TOFINO2_36D";
#endif  // BF_DEV_BFNT20036D
#ifdef BF_DEV_BFNT20032E
    case BF_DEV_BFNT20032E:  // removed in 9.3.0
      return "TOFINO2_32E";
#endif  // BF_DEV_BFNT20032E
#ifdef BF_DEV_BFNT20064E
    case BF_DEV_BFNT20064E:  // removed in 9.3.0
      return "TOFINO2_64E";
#endif  // BF_DEV_BFNT20064E
    default:
      return "UNKNOWN";
  }
}

std::string BfSdeWrapper::GetSdeVersion() const {
#if defined(SDE_9_1_0)
  return "9.1.0";
#elif defined(SDE_9_2_0)
  return "9.2.0";
#elif defined(SDE_9_3_0)
  return "9.3.0";
#elif defined(SDE_9_3_1)
  return "9.3.1";
#elif defined(SDE_9_4_0)
  return "9.4.0";
#else
#error Unsupported SDE version
#endif
}

::util::StatusOr<uint32> BfSdeWrapper::GetPortIdFromPortKey(
    int device, const PortKey& port_key) {
  const int port = port_key.port;
  CHECK_RETURN_IF_FALSE(port >= 0)
      << "Port ID must be non-negative. Attempted to get port " << port
      << " on dev " << device << ".";

  // PortKey uses three possible values for channel:
  //     > 0: port is channelized (first channel is 1)
  //     0: port is not channelized
  //     < 0: port channel is not important (e.g. for port groups)
  // BF SDK expects the first channel to be 0
  //     Convert base-1 channel to base-0 channel if port is channelized
  //     Otherwise, port is already 0 in the non-channelized case
  const int channel =
      (port_key.channel > 0) ? port_key.channel - 1 : port_key.channel;
  CHECK_RETURN_IF_FALSE(channel >= 0)
      << "Channel must be set for port " << port << " on dev " << device << ".";

  char port_string[MAX_PORT_HDL_STRING_LEN];
  int r = snprintf(port_string, sizeof(port_string), "%d/%d", port, channel);
  CHECK_RETURN_IF_FALSE(r > 0 && r < sizeof(port_string))
      << "Failed to build port string for port " << port << " channel "
      << channel << " on dev " << device << ".";

  bf_dev_port_t dev_port;
  RETURN_IF_BFRT_ERROR(bf_pal_port_str_to_dev_port_map(
      static_cast<bf_dev_id_t>(device), port_string, &dev_port));
  return static_cast<uint32>(dev_port);
}

::util::StatusOr<int> BfSdeWrapper::GetPcieCpuPort(int device) {
  int port = p4_devport_mgr_pcie_cpu_port_get(device);
  CHECK_RETURN_IF_FALSE(port != -1);
  return port;
}

::util::Status BfSdeWrapper::SetTmCpuPort(int device, int port) {
  CHECK_RETURN_IF_FALSE(p4_pd_tm_set_cpuport(device, port) == 0)
      << "Unable to set CPU port " << port << " on device " << device;
  return ::util::OkStatus();
}

// BFRT

::util::Status BfSdeWrapper::AddDevice(int device,
                                       const BfrtDeviceConfig& device_config) {
  absl::WriterMutexLock l(&data_lock_);

  // CHECK_RETURN_IF_FALSE(initialized_) << "Not initialized";
  CHECK_RETURN_IF_FALSE(device_config.programs_size() > 0);

  // if (pipeline_initialized_) {
  // RETURN_IF_BFRT_ERROR(bf_device_remove(device));
  // }

  bfrt_device_manager_ = &bfrt::BfRtDevMgr::getInstance();
  bfrt_id_mapper_.reset();

  RETURN_IF_BFRT_ERROR(bf_pal_device_warm_init_begin(
      device, BF_DEV_WARM_INIT_FAST_RECFG, BF_DEV_SERDES_UPD_NONE,
      /* upgrade_agents */ true));
  bf_device_profile_t device_profile = {};

  // Commit new files to disk and build device profile for SDE to load.
  RETURN_IF_ERROR(RecursivelyCreateDir(FLAGS_bfrt_sde_config_dir));
  // Need to extend the lifetime of the path strings until the SDE read them.
  std::vector<std::unique_ptr<std::string>> path_strings;
  device_profile.num_p4_programs = device_config.programs_size();
  for (int i = 0; i < device_config.programs_size(); ++i) {
    const auto& program = device_config.programs(i);
    const std::string program_path =
        absl::StrCat(FLAGS_bfrt_sde_config_dir, "/", program.name());
    auto bfrt_path = absl::make_unique<std::string>(
        absl::StrCat(program_path, "/bfrt.json"));
    RETURN_IF_ERROR(RecursivelyCreateDir(program_path));
    RETURN_IF_ERROR(WriteStringToFile(program.bfrt(), *bfrt_path));

    bf_p4_program_t* p4_program = &device_profile.p4_programs[i];
    ::snprintf(p4_program->prog_name, _PI_UPDATE_MAX_NAME_SIZE, "%s",
               program.name().c_str());
    p4_program->bfrt_json_file = &(*bfrt_path)[0];
    p4_program->num_p4_pipelines = program.pipelines_size();
    path_strings.emplace_back(std::move(bfrt_path));
    CHECK_RETURN_IF_FALSE(program.pipelines_size() > 0);
    for (int j = 0; j < program.pipelines_size(); ++j) {
      const auto& pipeline = program.pipelines(j);
      const std::string pipeline_path =
          absl::StrCat(program_path, "/", pipeline.name());
      auto context_path = absl::make_unique<std::string>(
          absl::StrCat(pipeline_path, "/context.json"));
      auto config_path = absl::make_unique<std::string>(
          absl::StrCat(pipeline_path, "/tofino.bin"));
      RETURN_IF_ERROR(RecursivelyCreateDir(pipeline_path));
      RETURN_IF_ERROR(WriteStringToFile(pipeline.context(), *context_path));
      RETURN_IF_ERROR(WriteStringToFile(pipeline.config(), *config_path));

      bf_p4_pipeline_t* pipeline_profile = &p4_program->p4_pipelines[j];
      ::snprintf(pipeline_profile->p4_pipeline_name, _PI_UPDATE_MAX_NAME_SIZE,
                 "%s", pipeline.name().c_str());
      pipeline_profile->cfg_file = &(*config_path)[0];
      pipeline_profile->runtime_context_file = &(*context_path)[0];
      path_strings.emplace_back(std::move(config_path));
      path_strings.emplace_back(std::move(context_path));

      CHECK_RETURN_IF_FALSE(pipeline.scope_size() <= MAX_P4_PIPELINES);
      pipeline_profile->num_pipes_in_scope = pipeline.scope_size();
      for (int p = 0; p < pipeline.scope_size(); ++p) {
        const auto& scope = pipeline.scope(p);
        pipeline_profile->pipe_scope[p] = scope;
      }
    }
  }

  // bf_device_add?
  // This call re-initializes most SDE components.
  RETURN_IF_BFRT_ERROR(bf_pal_device_add(device, &device_profile));
  RETURN_IF_BFRT_ERROR(bf_pal_device_warm_init_end(device));

  // Set SDE log levels for modules of interest.
  // TODO(max): create story around SDE logs. How to get them into glog? What
  // levels to enable for which modules?
  CHECK_RETURN_IF_FALSE(
      bf_sys_log_level_set(BF_MOD_BFRT, BF_LOG_DEST_STDOUT, BF_LOG_WARN) == 0);
  CHECK_RETURN_IF_FALSE(
      bf_sys_log_level_set(BF_MOD_PKT, BF_LOG_DEST_STDOUT, BF_LOG_WARN) == 0);
  CHECK_RETURN_IF_FALSE(
      bf_sys_log_level_set(BF_MOD_PIPE, BF_LOG_DEST_STDOUT, BF_LOG_WARN) == 0);
  stat_mgr_enable_detail_trace = false;
  if (VLOG_IS_ON(2)) {
    CHECK_RETURN_IF_FALSE(bf_sys_log_level_set(BF_MOD_PIPE, BF_LOG_DEST_STDOUT,
                                               BF_LOG_INFO) == 0);
    stat_mgr_enable_detail_trace = true;
  }

  RETURN_IF_BFRT_ERROR(bfrt_device_manager_->bfRtInfoGet(
      device, device_config.programs(0).name(), &bfrt_info_));

  // FIXME: if all we ever do is create and push, this could be one call.
  bfrt_id_mapper_ = BfrtIdMapper::CreateInstance();
  RETURN_IF_ERROR(
      bfrt_id_mapper_->PushForwardingPipelineConfig(device_config, bfrt_info_));

  return ::util::OkStatus();
}

// Create and start an new session.
::util::StatusOr<std::shared_ptr<BfSdeInterface::SessionInterface>>
BfSdeWrapper::CreateSession() {
  return Session::CreateSession();
}

::util::StatusOr<std::unique_ptr<BfSdeInterface::TableKeyInterface>>
BfSdeWrapper::CreateTableKey(int table_id) {
  ::absl::ReaderMutexLock l(&data_lock_);
  return TableKey::CreateTableKey(bfrt_info_, table_id);
}

::util::StatusOr<std::unique_ptr<BfSdeInterface::TableDataInterface>>
BfSdeWrapper::CreateTableData(int table_id, int action_id) {
  ::absl::ReaderMutexLock l(&data_lock_);
  return TableData::CreateTableData(bfrt_info_, table_id, action_id);
}

//  Packetio

::util::Status BfSdeWrapper::TxPacket(int device, const std::string& buffer) {
  bf_pkt* pkt = nullptr;
  RETURN_IF_BFRT_ERROR(
      bf_pkt_alloc(device, &pkt, buffer.size(), BF_DMA_CPU_PKT_TRANSMIT_0));
  auto pkt_cleaner =
      gtl::MakeCleanup([pkt, device]() { bf_pkt_free(device, pkt); });
  RETURN_IF_BFRT_ERROR(bf_pkt_data_copy(
      pkt, reinterpret_cast<const uint8*>(buffer.data()), buffer.size()));
  RETURN_IF_BFRT_ERROR(bf_pkt_tx(device, pkt, BF_PKT_TX_RING_0, pkt));
  pkt_cleaner.release();

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::StartPacketIo(int device) {
  // Maybe move to InitSde function?
  if (!bf_pkt_is_inited(device)) {
    RETURN_IF_BFRT_ERROR(bf_pkt_init());
  }

  // type of i should be bf_pkt_tx_ring_t?
  for (int tx_ring = BF_PKT_TX_RING_0; tx_ring < BF_PKT_TX_RING_MAX;
       ++tx_ring) {
    RETURN_IF_BFRT_ERROR(bf_pkt_tx_done_notif_register(
        device, BfSdeWrapper::BfPktTxNotifyCallback,
        static_cast<bf_pkt_tx_ring_t>(tx_ring)));
  }

  for (int rx_ring = BF_PKT_RX_RING_0; rx_ring < BF_PKT_RX_RING_MAX;
       ++rx_ring) {
    RETURN_IF_BFRT_ERROR(
        bf_pkt_rx_register(device, BfSdeWrapper::BfPktRxNotifyCallback,
                           static_cast<bf_pkt_rx_ring_t>(rx_ring), nullptr));
  }
  VLOG(1) << "Registered packetio callbacks on device " << device << ".";

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::StopPacketIo(int device) {
  for (int tx_ring = BF_PKT_TX_RING_0; tx_ring < BF_PKT_TX_RING_MAX;
       ++tx_ring) {
    RETURN_IF_BFRT_ERROR(bf_pkt_tx_done_notif_deregister(
        device, static_cast<bf_pkt_tx_ring_t>(tx_ring)));
  }

  for (int rx_ring = BF_PKT_RX_RING_0; rx_ring < BF_PKT_RX_RING_MAX;
       ++rx_ring) {
    RETURN_IF_BFRT_ERROR(
        bf_pkt_rx_deregister(device, static_cast<bf_pkt_rx_ring_t>(rx_ring)));
  }
  VLOG(1) << "Unregistered packetio callbacks on device " << device << ".";

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::RegisterPacketReceiveWriter(
    int device, std::unique_ptr<ChannelWriter<std::string>> writer) {
  absl::WriterMutexLock l(&packet_rx_callback_lock_);
  device_to_packet_rx_writer_[device] = std::move(writer);
  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::UnregisterPacketReceiveWriter(int device) {
  absl::WriterMutexLock l(&packet_rx_callback_lock_);
  device_to_packet_rx_writer_.erase(device);
  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::HandlePacketRx(bf_dev_id_t device, bf_pkt* pkt,
                                            bf_pkt_rx_ring_t rx_ring) {
  absl::ReaderMutexLock l(&packet_rx_callback_lock_);
  auto rx_writer = gtl::FindOrNull(device_to_packet_rx_writer_, device);
  CHECK_RETURN_IF_FALSE(rx_writer)
      << "No Rx callback registered for device id " << device << ".";

  std::string buffer(reinterpret_cast<const char*>(bf_pkt_get_pkt_data(pkt)),
                     bf_pkt_get_pkt_size(pkt));
  if (!(*rx_writer)->TryWrite(buffer).ok()) {
    LOG_EVERY_N(INFO, 500) << "Dropped packet received from CPU.";
  }

  VLOG(1) << "Received packet from CPU " << buffer.size() << " bytes "
          << StringToHex(buffer);

  return ::util::OkStatus();
}

bf_status_t BfSdeWrapper::BfPktTxNotifyCallback(bf_dev_id_t device,
                                                bf_pkt_tx_ring_t tx_ring,
                                                uint64 tx_cookie,
                                                uint32 status) {
  VLOG(1) << "Tx done notification for device: " << device
          << " tx ring: " << tx_ring << " tx cookie: " << tx_cookie
          << " status: " << status;

  bf_pkt* pkt = reinterpret_cast<bf_pkt*>(tx_cookie);
  return bf_pkt_free(device, pkt);
}

bf_status_t BfSdeWrapper::BfPktRxNotifyCallback(bf_dev_id_t device, bf_pkt* pkt,
                                                void* cookie,
                                                bf_pkt_rx_ring_t rx_ring) {
  BfSdeWrapper* bf_sde_wrapper = BfSdeWrapper::GetSingleton();
  // TODO(max): Handle error
  bf_sde_wrapper->HandlePacketRx(device, pkt, rx_ring);
  return bf_pkt_free(device, pkt);
}

bf_rt_target_t BfSdeWrapper::GetDeviceTarget(int device) const {
  bf_rt_target_t dev_tgt = {};
  dev_tgt.dev_id = device;
  dev_tgt.pipe_id = BF_DEV_PIPE_ALL;
  return dev_tgt;
}

// PRE
namespace {
::util::Status PrintMcGroupEntry(const bfrt::BfRtTable* table,
                                 const bfrt::BfRtTableKey* table_key,
                                 const bfrt::BfRtTableData* table_data) {
  std::vector<uint32> mc_node_list;
  std::vector<bool> l1_xid_valid_list;
  std::vector<uint32> l1_xid_list;
  uint64 multicast_group_id;

  // Key: $MGID
  RETURN_IF_ERROR(GetField(*table_key, kMgid, &multicast_group_id));
  // Data: $MULTICAST_NODE_ID
  RETURN_IF_ERROR(GetField(*table_data, kMcNodeId, &mc_node_list));
  // Data: $MULTICAST_NODE_L1_XID_VALID
  RETURN_IF_ERROR(GetField(*table_data, kMcNodeL1XidValid, &l1_xid_valid_list));
  // Data: $MULTICAST_NODE_L1_XID
  RETURN_IF_ERROR(GetField(*table_data, kMcNodeL1Xid, &l1_xid_list));

  LOG(INFO) << "Multicast group id " << multicast_group_id << " has "
            << mc_node_list.size() << " nodes.";
  for (const auto& node : mc_node_list) {
    LOG(INFO) << "\tnode id " << node;
  }

  return ::util::OkStatus();
}

::util::Status PrintMcNodeEntry(const bfrt::BfRtTable* table,
                                const bfrt::BfRtTableKey* table_key,
                                const bfrt::BfRtTableData* table_data) {
  // Key: $MULTICAST_NODE_ID (24 bit)
  uint64 node_id;
  RETURN_IF_ERROR(GetField(*table_key, kMcNodeId, &node_id));
  // Data: $MULTICAST_RID (16 bit)
  uint64 rid;
  RETURN_IF_ERROR(GetField(*table_data, kMcReplicationId, &rid));
  // Data: $DEV_PORT
  std::vector<uint32> ports;
  RETURN_IF_ERROR(GetField(*table_data, kMcNodeDevPort, &ports));

  std::string ports_str = " ports [ ";
  for (const auto& port : ports) {
    ports_str += std::to_string(port) + " ";
  }
  ports_str += "]";
  LOG(INFO) << "Node id " << node_id << ": rid " << rid << ports_str;

  return ::util::OkStatus();
}
}  // namespace

::util::Status BfSdeWrapper::DumpPreState(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session) {
  if (VLOG_IS_ON(2)) {
    auto real_session = std::dynamic_pointer_cast<Session>(session);
    CHECK_RETURN_IF_FALSE(real_session);

    auto bf_dev_tgt = GetDeviceTarget(device);
    const bfrt::BfRtTable* table;

    // Dump group table
    LOG(INFO) << "#### $pre.mgid ####";
    RETURN_IF_BFRT_ERROR(
        bfrt_info_->bfrtTableFromNameGet(kPreMgidTable, &table));
    std::vector<std::unique_ptr<bfrt::BfRtTableKey>> keys;
    std::vector<std::unique_ptr<bfrt::BfRtTableData>> datums;
    RETURN_IF_BFRT_ERROR(
        bfrt_info_->bfrtTableFromNameGet(kPreMgidTable, &table));
    RETURN_IF_ERROR(GetAllEntries(real_session->bfrt_session_, bf_dev_tgt,
                                  table, &keys, &datums));
    for (size_t i = 0; i < keys.size(); ++i) {
      const std::unique_ptr<bfrt::BfRtTableData>& table_data = datums[i];
      const std::unique_ptr<bfrt::BfRtTableKey>& table_key = keys[i];
      PrintMcGroupEntry(table, table_key.get(), table_data.get());
    }
    LOG(INFO) << "###################";

    // Dump node table
    LOG(INFO) << "#### $pre.node ####";
    RETURN_IF_BFRT_ERROR(
        bfrt_info_->bfrtTableFromNameGet(kPreNodeTable, &table));
    RETURN_IF_ERROR(GetAllEntries(real_session->bfrt_session_, bf_dev_tgt,
                                  table, &keys, &datums));
    for (size_t i = 0; i < keys.size(); ++i) {
      const std::unique_ptr<bfrt::BfRtTableData>& table_data = datums[i];
      const std::unique_ptr<bfrt::BfRtTableKey>& table_key = keys[i];
      PrintMcNodeEntry(table, table_key.get(), table_data.get());
    }
    LOG(INFO) << "###################";
  }
  return ::util::OkStatus();
}

::util::StatusOr<uint32> BfSdeWrapper::GetFreeMulticastNodeId(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session) {
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  auto bf_dev_tgt = GetDeviceTarget(device);
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromNameGet(kPreNodeTable, &table));
  size_t table_size;
#if defined(SDE_9_4_0)
  RETURN_IF_BFRT_ERROR(table->tableSizeGet(*real_session->bfrt_session_,
                                           bf_dev_tgt, &table_size));
#else
  RETURN_IF_BFRT_ERROR(table->tableSizeGet(&table_size));
#endif  // SDE_9_4_0
  uint32 usage;
  RETURN_IF_BFRT_ERROR(table->tableUsageGet(
      *real_session->bfrt_session_, bf_dev_tgt,
      bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, &usage));
  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));
  uint32 id = usage;
  for (size_t _ = 0; _ < table_size; ++_) {
    // Key: $MULTICAST_NODE_ID
    RETURN_IF_ERROR(SetField(table_key.get(), kMcNodeId, id));
    bf_status_t status = table->tableEntryGet(
        *real_session->bfrt_session_, bf_dev_tgt, *table_key,
        bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, table_data.get());
    if (status == BF_OBJECT_NOT_FOUND) {
      return id;
    } else if (status == BF_SUCCESS) {
      id++;
      continue;
    } else {
      RETURN_IF_BFRT_ERROR(status);
    }
  }

  RETURN_ERROR(ERR_TABLE_FULL) << "Could not find free multicast node id.";
}

::util::StatusOr<uint32> BfSdeWrapper::CreateMulticastNode(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    int mc_replication_id, const std::vector<uint32>& mc_lag_ids,
    const std::vector<uint32>& ports) {
  ::absl::ReaderMutexLock l(&data_lock_);

  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  const bfrt::BfRtTable* table;  // PRE node table.
  bf_rt_id_t table_id;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromNameGet(kPreNodeTable, &table));
  RETURN_IF_BFRT_ERROR(table->tableIdGet(&table_id));

  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));

  auto bf_dev_tgt = GetDeviceTarget(device);

  ASSIGN_OR_RETURN(uint64 mc_node_id, GetFreeMulticastNodeId(device, session));

  // Key: $MULTICAST_NODE_ID
  RETURN_IF_ERROR(SetField(table_key.get(), kMcNodeId, mc_node_id));
  // Data: $MULTICAST_RID (16 bit)
  RETURN_IF_ERROR(
      SetField(table_data.get(), kMcReplicationId, mc_replication_id));
  // Data: $MULTICAST_LAG_ID
  RETURN_IF_ERROR(SetField(table_data.get(), kMcNodeLagId, mc_lag_ids));
  // Data: $DEV_PORT
  RETURN_IF_ERROR(SetField(table_data.get(), kMcNodeDevPort, ports));

  RETURN_IF_BFRT_ERROR(table->tableEntryAdd(
      *real_session->bfrt_session_, bf_dev_tgt, *table_key, *table_data));

  return mc_node_id;
}

::util::StatusOr<std::vector<uint32>> BfSdeWrapper::GetNodesInMulticastGroup(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 group_id) {
  ::absl::ReaderMutexLock l(&data_lock_);

  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  auto bf_dev_tgt = GetDeviceTarget(device);
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromNameGet(kPreMgidTable, &table));

  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));
  // Key: $MGID
  RETURN_IF_ERROR(SetField(table_key.get(), kMgid, group_id));
  RETURN_IF_BFRT_ERROR(table->tableEntryGet(
      *real_session->bfrt_session_, bf_dev_tgt, *table_key,
      bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, table_data.get()));
  // Data: $MULTICAST_NODE_ID
  std::vector<uint32> mc_node_list;
  RETURN_IF_ERROR(GetField(*table_data, kMcNodeId, &mc_node_list));

  return mc_node_list;
}

::util::Status BfSdeWrapper::DeleteMulticastNodes(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    const std::vector<uint32>& mc_node_ids) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  auto bf_dev_tgt = GetDeviceTarget(device);
  const bfrt::BfRtTable* table;
  bf_rt_id_t table_id;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromNameGet(kPreNodeTable, &table));
  RETURN_IF_BFRT_ERROR(table->tableIdGet(&table_id));

  // TODO(max): handle partial delete failures
  for (const auto& mc_node_id : mc_node_ids) {
    std::unique_ptr<bfrt::BfRtTableKey> table_key;
    RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
    RETURN_IF_ERROR(SetField(table_key.get(), kMcNodeId, mc_node_id));
    RETURN_IF_BFRT_ERROR(table->tableEntryDel(*real_session->bfrt_session_,
                                              bf_dev_tgt, *table_key));
  }

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::GetMulticastNode(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 mc_node_id, int* replication_id, std::vector<uint32>* lag_ids,
    std::vector<uint32>* ports) {
  CHECK_RETURN_IF_FALSE(replication_id);
  CHECK_RETURN_IF_FALSE(lag_ids);
  CHECK_RETURN_IF_FALSE(ports);
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  auto bf_dev_tgt = GetDeviceTarget(device);
  const bfrt::BfRtTable* table;  // PRE node table.
  bf_rt_id_t table_id;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromNameGet(kPreNodeTable, &table));
  RETURN_IF_BFRT_ERROR(table->tableIdGet(&table_id));

  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));
  // Key: $MULTICAST_NODE_ID
  RETURN_IF_ERROR(SetField(table_key.get(), kMcNodeId, mc_node_id));
  RETURN_IF_BFRT_ERROR(table->tableEntryGet(
      *real_session->bfrt_session_, bf_dev_tgt, *table_key,
      bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, table_data.get()));
  // Data: $DEV_PORT
  std::vector<uint32> dev_ports;
  RETURN_IF_ERROR(GetField(*table_data, kMcNodeDevPort, &dev_ports));
  *ports = dev_ports;
  // Data: $RID (16 bit)
  uint64 rid;
  RETURN_IF_ERROR(GetField(*table_data, kMcReplicationId, &rid));
  *replication_id = rid;
  // Data: $MULTICAST_LAG_ID
  std::vector<uint32> lags;
  RETURN_IF_ERROR(GetField(*table_data, kMcNodeLagId, &lags));
  *lag_ids = lags;

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::WriteMulticastGroup(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 group_id, const std::vector<uint32>& mc_node_ids, bool insert) {
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  const bfrt::BfRtTable* table;  // PRE MGID table.
  bf_rt_id_t table_id;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromNameGet(kPreMgidTable, &table));
  RETURN_IF_BFRT_ERROR(table->tableIdGet(&table_id));
  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));

  std::vector<uint32> mc_node_list;
  std::vector<bool> l1_xid_valid_list;
  std::vector<uint32> l1_xid_list;
  for (const auto& mc_node_id : mc_node_ids) {
    mc_node_list.push_back(mc_node_id);
    // TODO(Yi): P4Runtime doesn't support XID, set invalid for now.
    l1_xid_valid_list.push_back(false);
    l1_xid_list.push_back(0);
  }
  // Key: $MGID
  RETURN_IF_ERROR(SetField(table_key.get(), kMgid, group_id));
  // Data: $MULTICAST_NODE_ID
  RETURN_IF_ERROR(SetField(table_data.get(), kMcNodeId, mc_node_list));
  // Data: $MULTICAST_NODE_L1_XID_VALID
  RETURN_IF_ERROR(
      SetField(table_data.get(), kMcNodeL1XidValid, l1_xid_valid_list));
  // Data: $MULTICAST_NODE_L1_XID
  RETURN_IF_ERROR(SetField(table_data.get(), kMcNodeL1Xid, l1_xid_list));

  auto bf_dev_tgt = GetDeviceTarget(device);
  if (insert) {
    RETURN_IF_BFRT_ERROR(table->tableEntryAdd(
        *real_session->bfrt_session_, bf_dev_tgt, *table_key, *table_data));

  } else {
    RETURN_IF_BFRT_ERROR(table->tableEntryMod(
        *real_session->bfrt_session_, bf_dev_tgt, *table_key, *table_data));
  }

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::InsertMulticastGroup(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 group_id, const std::vector<uint32>& mc_node_ids) {
  ::absl::ReaderMutexLock l(&data_lock_);
  return WriteMulticastGroup(device, session, group_id, mc_node_ids, true);
}
::util::Status BfSdeWrapper::ModifyMulticastGroup(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 group_id, const std::vector<uint32>& mc_node_ids) {
  ::absl::ReaderMutexLock l(&data_lock_);
  return WriteMulticastGroup(device, session, group_id, mc_node_ids, false);
}

::util::Status BfSdeWrapper::DeleteMulticastGroup(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 group_id) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  auto bf_dev_tgt = GetDeviceTarget(device);
  const bfrt::BfRtTable* table;  // PRE MGID table.
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromNameGet(kPreMgidTable, &table));
  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  // Key: $MGID
  RETURN_IF_ERROR(SetField(table_key.get(), kMgid, group_id));
  RETURN_IF_BFRT_ERROR(table->tableEntryDel(*real_session->bfrt_session_,
                                            bf_dev_tgt, *table_key));

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::GetMulticastGroups(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 group_id, std::vector<uint32>* group_ids,
    std::vector<std::vector<uint32>>* mc_node_ids) {
  CHECK_RETURN_IF_FALSE(group_ids);
  CHECK_RETURN_IF_FALSE(mc_node_ids);
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  auto bf_dev_tgt = GetDeviceTarget(device);
  const bfrt::BfRtTable* table;  // PRE MGID table.
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromNameGet(kPreMgidTable, &table));
  std::vector<std::unique_ptr<bfrt::BfRtTableKey>> keys;
  std::vector<std::unique_ptr<bfrt::BfRtTableData>> datums;
  // Is this a wildcard read?
  if (group_id != 0) {
    keys.resize(1);
    datums.resize(1);
    RETURN_IF_BFRT_ERROR(table->keyAllocate(&keys[0]));
    RETURN_IF_BFRT_ERROR(table->dataAllocate(&datums[0]));
    // Key: $MGID
    RETURN_IF_ERROR(SetField(keys[0].get(), kMgid, group_id));
    RETURN_IF_BFRT_ERROR(table->tableEntryGet(
        *real_session->bfrt_session_, bf_dev_tgt, *keys[0],
        bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, datums[0].get()));
  } else {
    RETURN_IF_ERROR(GetAllEntries(real_session->bfrt_session_, bf_dev_tgt,
                                  table, &keys, &datums));
  }

  group_ids->resize(0);
  mc_node_ids->resize(0);
  for (size_t i = 0; i < keys.size(); ++i) {
    const std::unique_ptr<bfrt::BfRtTableData>& table_data = datums[i];
    const std::unique_ptr<bfrt::BfRtTableKey>& table_key = keys[i];
    ::p4::v1::MulticastGroupEntry result;
    // Key: $MGID
    uint64 group_id;
    RETURN_IF_ERROR(GetField(*table_key, kMgid, &group_id));
    group_ids->push_back(group_id);
    // Data: $MULTICAST_NODE_ID
    std::vector<uint32> mc_node_list;
    RETURN_IF_ERROR(GetField(*table_data, kMcNodeId, &mc_node_list));
    mc_node_ids->push_back(mc_node_list);
  }

  CHECK_EQ(group_ids->size(), keys.size());
  CHECK_EQ(mc_node_ids->size(), keys.size());

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::WriteCloneSession(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 session_id, int egress_port, int cos, int max_pkt_len, bool insert) {
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromNameGet("$mirror.cfg", &table));
  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  bf_rt_id_t action_id;
  RETURN_IF_BFRT_ERROR(table->actionIdGet("$normal", &action_id));
  RETURN_IF_BFRT_ERROR(table->dataAllocate(action_id, &table_data));

  // Key: $sid
  RETURN_IF_ERROR(SetField(table_key.get(), "$sid", session_id));
  // Data: $direction
  RETURN_IF_ERROR(SetField(table_data.get(), "$direction", "BOTH"));
  // Data: $session_enable
  RETURN_IF_ERROR(SetFieldBool(table_data.get(), "$session_enable", true));
  // Data: $ucast_egress_port
  RETURN_IF_ERROR(
      SetField(table_data.get(), "$ucast_egress_port", egress_port));
  // Data: $ucast_egress_port_valid
  RETURN_IF_ERROR(
      SetFieldBool(table_data.get(), "$ucast_egress_port_valid", true));
  // Data: $ingress_cos
  RETURN_IF_ERROR(SetField(table_data.get(), "$ingress_cos", cos));
  // Data: $max_pkt_len
  RETURN_IF_ERROR(SetField(table_data.get(), "$max_pkt_len", max_pkt_len));

  auto bf_dev_tgt = GetDeviceTarget(device);
  if (insert) {
    RETURN_IF_BFRT_ERROR(table->tableEntryAdd(
        *real_session->bfrt_session_, bf_dev_tgt, *table_key, *table_data));
  } else {
    RETURN_IF_BFRT_ERROR(table->tableEntryMod(
        *real_session->bfrt_session_, bf_dev_tgt, *table_key, *table_data));
  }

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::InsertCloneSession(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 session_id, int egress_port, int cos, int max_pkt_len) {
  ::absl::ReaderMutexLock l(&data_lock_);
  return WriteCloneSession(device, session, session_id, egress_port, cos,
                           max_pkt_len, true);
}

::util::Status BfSdeWrapper::ModifyCloneSession(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 session_id, int egress_port, int cos, int max_pkt_len) {
  ::absl::ReaderMutexLock l(&data_lock_);
  return WriteCloneSession(device, session, session_id, egress_port, cos,
                           max_pkt_len, false);
}

::util::Status BfSdeWrapper::DeleteCloneSession(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 session_id) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromNameGet("$mirror.cfg", &table));
  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  bf_rt_id_t action_id;
  RETURN_IF_BFRT_ERROR(table->actionIdGet("$normal", &action_id));
  RETURN_IF_BFRT_ERROR(table->dataAllocate(action_id, &table_data));
  // Key: $sid
  RETURN_IF_ERROR(SetField(table_key.get(), "$sid", session_id));

  auto bf_dev_tgt = GetDeviceTarget(device);
  RETURN_IF_BFRT_ERROR(table->tableEntryDel(*real_session->bfrt_session_,
                                            bf_dev_tgt, *table_key));

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::GetCloneSessions(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 session_id, std::vector<uint32>* session_ids,
    std::vector<int>* egress_ports, std::vector<int>* coss,
    std::vector<int>* max_pkt_lens) {
  CHECK_RETURN_IF_FALSE(session_ids);
  CHECK_RETURN_IF_FALSE(egress_ports);
  CHECK_RETURN_IF_FALSE(coss);
  CHECK_RETURN_IF_FALSE(max_pkt_lens);
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  auto bf_dev_tgt = GetDeviceTarget(device);
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromNameGet("$mirror.cfg", &table));
  bf_rt_id_t action_id;
  RETURN_IF_BFRT_ERROR(table->actionIdGet("$normal", &action_id));
  std::vector<std::unique_ptr<bfrt::BfRtTableKey>> keys;
  std::vector<std::unique_ptr<bfrt::BfRtTableData>> datums;
  // Is this a wildcard read?
  if (session_id != 0) {
    keys.resize(1);
    datums.resize(1);
    RETURN_IF_BFRT_ERROR(table->keyAllocate(&keys[0]));
    RETURN_IF_BFRT_ERROR(table->dataAllocate(action_id, &datums[0]));
    // Key: $sid
    RETURN_IF_ERROR(SetField(keys[0].get(), "$sid", session_id));
    RETURN_IF_BFRT_ERROR(table->tableEntryGet(
        *real_session->bfrt_session_, bf_dev_tgt, *keys[0],
        bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, datums[0].get()));
  } else {
    RETURN_IF_ERROR(GetAllEntries(real_session->bfrt_session_, bf_dev_tgt,
                                  table, &keys, &datums));
  }

  session_ids->resize(0);
  egress_ports->resize(0);
  coss->resize(0);
  max_pkt_lens->resize(0);
  for (size_t i = 0; i < keys.size(); ++i) {
    const std::unique_ptr<bfrt::BfRtTableData>& table_data = datums[i];
    const std::unique_ptr<bfrt::BfRtTableKey>& table_key = keys[i];
    // Key: $sid
    uint64 session_id;
    RETURN_IF_ERROR(GetField(*table_key, "$sid", &session_id));
    session_ids->push_back(session_id);
    // Data: $ingress_cos
    uint64 ingress_cos;
    RETURN_IF_ERROR(GetField(*table_data, "$ingress_cos", &ingress_cos));
    coss->push_back(ingress_cos);
    // Data: $max_pkt_len
    uint64 pkt_len;
    RETURN_IF_ERROR(GetField(*table_data, "$max_pkt_len", &pkt_len));
    max_pkt_lens->push_back(pkt_len);
    // Data: $ucast_egress_port
    uint64 port;
    RETURN_IF_ERROR(GetField(*table_data, "$ucast_egress_port", &port));
    egress_ports->push_back(port);
    // Data: $session_enable
    bool session_enable;
    RETURN_IF_ERROR(GetField(*table_data, "$session_enable", &session_enable));
    CHECK_RETURN_IF_FALSE(session_enable)
        << "Found a session that is not enabled.";
    // Data: $ucast_egress_port_valid
    bool ucast_egress_port_valid;
    RETURN_IF_ERROR(GetField(*table_data, "$ucast_egress_port_valid",
                             &ucast_egress_port_valid));
    CHECK_RETURN_IF_FALSE(ucast_egress_port_valid)
        << "Found a unicase egress port that is not set valid.";
  }

  CHECK_EQ(session_ids->size(), keys.size());
  CHECK_EQ(egress_ports->size(), keys.size());
  CHECK_EQ(coss->size(), keys.size());
  CHECK_EQ(max_pkt_lens->size(), keys.size());

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::WriteIndirectCounter(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 counter_id, int counter_index, absl::optional<uint64> byte_count,
    absl::optional<uint64> packet_count) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(counter_id, &table));

  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));

  // Counter key: $COUNTER_INDEX
  RETURN_IF_ERROR(SetField(table_key.get(), "$COUNTER_INDEX", counter_index));

  // Counter data: $COUNTER_SPEC_BYTES
  if (byte_count.has_value()) {
    bf_rt_id_t field_id;
    auto bf_status = table->dataFieldIdGet("$COUNTER_SPEC_BYTES", &field_id);
    if (bf_status == BF_SUCCESS) {
      RETURN_IF_BFRT_ERROR(table_data->setValue(field_id, byte_count.value()));
    }
  }
  // Counter data: $COUNTER_SPEC_PKTS
  if (packet_count.has_value()) {
    bf_rt_id_t field_id;
    auto bf_status = table->dataFieldIdGet("$COUNTER_SPEC_PKTS", &field_id);
    if (bf_status == BF_SUCCESS) {
      RETURN_IF_BFRT_ERROR(
          table_data->setValue(field_id, packet_count.value()));
    }
  }
  auto bf_dev_tgt = GetDeviceTarget(device);
  RETURN_IF_BFRT_ERROR(table->tableEntryMod(
      *real_session->bfrt_session_, bf_dev_tgt, *table_key, *table_data));

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::ReadIndirectCounter(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 counter_id, absl::optional<uint32> counter_index,
    std::vector<uint32>* counter_indices,
    std::vector<absl::optional<uint64>>* byte_counts,
    std::vector<absl::optional<uint64>>* packet_counts,
    absl::Duration timeout) {
  CHECK_RETURN_IF_FALSE(counter_indices);
  CHECK_RETURN_IF_FALSE(byte_counts);
  CHECK_RETURN_IF_FALSE(packet_counts);
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  auto bf_dev_tgt = GetDeviceTarget(device);
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(counter_id, &table));
  std::vector<std::unique_ptr<bfrt::BfRtTableKey>> keys;
  std::vector<std::unique_ptr<bfrt::BfRtTableData>> datums;

  RETURN_IF_ERROR(DoSynchronizeCounters(device, session, counter_id, timeout));

  // Is this a wildcard read?
  if (counter_index) {
    keys.resize(1);
    datums.resize(1);
    RETURN_IF_BFRT_ERROR(table->keyAllocate(&keys[0]));
    RETURN_IF_BFRT_ERROR(table->dataAllocate(&datums[0]));

    // Key: $COUNTER_INDEX
    RETURN_IF_ERROR(
        SetField(keys[0].get(), "$COUNTER_INDEX", counter_index.value()));
    RETURN_IF_BFRT_ERROR(table->tableEntryGet(
        *real_session->bfrt_session_, bf_dev_tgt, *keys[0],
        bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, datums[0].get()));
  } else {
    RETURN_IF_ERROR(GetAllEntries(real_session->bfrt_session_, bf_dev_tgt,
                                  table, &keys, &datums));
  }

  counter_indices->resize(0);
  byte_counts->resize(0);
  packet_counts->resize(0);
  for (size_t i = 0; i < keys.size(); ++i) {
    const std::unique_ptr<bfrt::BfRtTableData>& table_data = datums[i];
    const std::unique_ptr<bfrt::BfRtTableKey>& table_key = keys[i];
    // Key: $COUNTER_INDEX
    uint64 bf_counter_index;
    RETURN_IF_ERROR(GetField(*table_key, "$COUNTER_INDEX", &bf_counter_index));
    counter_indices->push_back(bf_counter_index);

    absl::optional<uint64> byte_count;
    absl::optional<uint64> packet_count;
    // Counter data: $COUNTER_SPEC_BYTES
    bf_rt_id_t field_id;
    auto bf_status = table->dataFieldIdGet("$COUNTER_SPEC_BYTES", &field_id);
    if (bf_status == BF_SUCCESS) {
      uint64 counter_data;
      RETURN_IF_BFRT_ERROR(table_data->getValue(field_id, &counter_data));
      byte_count = counter_data;
    }
    byte_counts->push_back(byte_count);

    // Counter data: $COUNTER_SPEC_PKTS
    bf_status = table->dataFieldIdGet("$COUNTER_SPEC_PKTS", &field_id);
    if (bf_status == BF_SUCCESS) {
      uint64 counter_data;
      RETURN_IF_BFRT_ERROR(table_data->getValue(field_id, &counter_data));
      packet_count = counter_data;
    }
    packet_counts->push_back(packet_count);
  }

  CHECK_EQ(counter_indices->size(), keys.size());
  CHECK_EQ(byte_counts->size(), keys.size());
  CHECK_EQ(packet_counts->size(), keys.size());

  return ::util::OkStatus();
}

namespace {
// Helper function to get the field ID of the "f1" register data field.
// TODO(max): Maybe use table name and strip off "pipe." at the beginning?
// std::string table_name;
// RETURN_IF_BFRT_ERROR(table->tableNameGet(&table_name));
// RETURN_IF_BFRT_ERROR(
//     table->dataFieldIdGet(absl::StrCat(table_name, ".", "f1"), &field_id));
::util::StatusOr<bf_rt_id_t> GetRegisterDataFieldId(
    const bfrt::BfRtTable* table) {
  std::vector<bf_rt_id_t> data_field_ids;
  RETURN_IF_BFRT_ERROR(table->dataFieldIdListGet(&data_field_ids));
  for (const auto& field_id : data_field_ids) {
    std::string field_name;
    RETURN_IF_BFRT_ERROR(table->dataFieldNameGet(field_id, &field_name));
    bfrt::DataType data_type;
    RETURN_IF_BFRT_ERROR(table->dataFieldDataTypeGet(field_id, &data_type));
    if (absl::EndsWith(field_name, ".f1")) {
      return field_id;
    }
  }

  RETURN_ERROR(ERR_INTERNAL) << "Could not find register data field id.";
}
}  // namespace

::util::Status BfSdeWrapper::WriteRegister(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 table_id, absl::optional<uint32> register_index,
    const std::string& register_data) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));

  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));

  // Register data: <register_name>.f1
  // The current bf-p4c compiler emits the fully-qualified field name, including
  // parent table and pipeline. We cannot use just "f1" as the field name.
  bf_rt_id_t field_id;
  ASSIGN_OR_RETURN(field_id, GetRegisterDataFieldId(table));
  size_t data_field_size_bits;
  RETURN_IF_BFRT_ERROR(
      table->dataFieldSizeGet(field_id, &data_field_size_bits));
  // The SDE expects a string with the full width.
  std::string value = P4RuntimeByteStringToPaddedByteString(
      register_data, data_field_size_bits);
  RETURN_IF_BFRT_ERROR(table_data->setValue(
      field_id, reinterpret_cast<const uint8*>(value.data()), value.size()));

  auto bf_dev_tgt = GetDeviceTarget(device);
  if (register_index) {
    // Single index target.
    // Register key: $REGISTER_INDEX
    RETURN_IF_ERROR(
        SetField(table_key.get(), kRegisterIndex, register_index.value()));
    RETURN_IF_BFRT_ERROR(table->tableEntryMod(
        *real_session->bfrt_session_, bf_dev_tgt, *table_key, *table_data));
  } else {
    // Wildcard write to all indices.
    size_t table_size;
#if defined(SDE_9_4_0)
    RETURN_IF_BFRT_ERROR(table->tableSizeGet(*real_session->bfrt_session_,
                                             bf_dev_tgt, &table_size));
#else
    RETURN_IF_BFRT_ERROR(table->tableSizeGet(&table_size));
#endif  // SDE_9_4_0
    for (size_t i = 0; i < table_size; ++i) {
      // Register key: $REGISTER_INDEX
      RETURN_IF_ERROR(SetField(table_key.get(), kRegisterIndex, i));
      RETURN_IF_BFRT_ERROR(table->tableEntryMod(
          *real_session->bfrt_session_, bf_dev_tgt, *table_key, *table_data));
    }
  }

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::ReadRegisters(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 table_id, absl::optional<uint32> register_index,
    std::vector<uint32>* register_indices, std::vector<uint64>* register_datas,
    absl::Duration timeout) {
  CHECK_RETURN_IF_FALSE(register_indices);
  CHECK_RETURN_IF_FALSE(register_datas);
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  RETURN_IF_ERROR(SynchronizeRegisters(device, session, table_id, timeout));

  auto bf_dev_tgt = GetDeviceTarget(device);
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));
  std::vector<std::unique_ptr<bfrt::BfRtTableKey>> keys;
  std::vector<std::unique_ptr<bfrt::BfRtTableData>> datums;

  // Is this a wildcard read?
  if (register_index) {
    keys.resize(1);
    datums.resize(1);
    RETURN_IF_BFRT_ERROR(table->keyAllocate(&keys[0]));
    RETURN_IF_BFRT_ERROR(table->dataAllocate(&datums[0]));

    // Key: $REGISTER_INDEX
    RETURN_IF_ERROR(
        SetField(keys[0].get(), "$REGISTER_INDEX", register_index.value()));
    RETURN_IF_BFRT_ERROR(table->tableEntryGet(
        *real_session->bfrt_session_, bf_dev_tgt, *keys[0],
        bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, datums[0].get()));
  } else {
    RETURN_IF_ERROR(GetAllEntries(real_session->bfrt_session_, bf_dev_tgt,
                                  table, &keys, &datums));
  }

  register_indices->resize(0);
  register_datas->resize(0);
  for (size_t i = 0; i < keys.size(); ++i) {
    const std::unique_ptr<bfrt::BfRtTableData>& table_data = datums[i];
    const std::unique_ptr<bfrt::BfRtTableKey>& table_key = keys[i];
    // Key: $REGISTER_INDEX
    uint64 bf_register_index;
    RETURN_IF_ERROR(
        GetField(*table_key, "$REGISTER_INDEX", &bf_register_index));
    register_indices->push_back(bf_register_index);
    // Data: <register_name>.f1
    ASSIGN_OR_RETURN(auto f1_field_id, GetRegisterDataFieldId(table));

    bfrt::DataType data_type;
    RETURN_IF_BFRT_ERROR(table->dataFieldDataTypeGet(f1_field_id, &data_type));
    switch (data_type) {
      case bfrt::DataType::BYTE_STREAM: {
        // Even though the data type says byte stream, the SDE can only allows
        // fetching the data in an uint64 vector with one entry per pipe.
        std::vector<uint64> register_data;
        RETURN_IF_BFRT_ERROR(table_data->getValue(f1_field_id, &register_data));
        CHECK_RETURN_IF_FALSE(register_data.size() > 0);
        register_datas->push_back(register_data[0]);
        break;
      }
      default:
        RETURN_ERROR(ERR_INVALID_PARAM)
            << "Unsupported register data type " << static_cast<int>(data_type)
            << " for register in table " << table_id;
    }
  }

  CHECK_EQ(register_indices->size(), keys.size());
  CHECK_EQ(register_datas->size(), keys.size());

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::WriteIndirectMeter(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 table_id, absl::optional<uint32> meter_index, bool in_pps,
    uint64 cir, uint64 cburst, uint64 pir, uint64 pburst) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));

  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));

  // Meter data: $METER_SPEC_*
  if (in_pps) {
    RETURN_IF_ERROR(SetField(table_data.get(), kMeterCirPps, cir));
    RETURN_IF_ERROR(
        SetField(table_data.get(), kMeterCommitedBurstPackets, cburst));
    RETURN_IF_ERROR(SetField(table_data.get(), kMeterPirPps, pir));
    RETURN_IF_ERROR(SetField(table_data.get(), kMeterPeakBurstPackets, pburst));
  } else {
    RETURN_IF_ERROR(
        SetField(table_data.get(), kMeterCirKbps, BytesPerSecondToKbits(cir)));
    RETURN_IF_ERROR(SetField(table_data.get(), kMeterCommitedBurstKbits,
                             BytesPerSecondToKbits(cburst)));
    RETURN_IF_ERROR(
        SetField(table_data.get(), kMeterPirKbps, BytesPerSecondToKbits(pir)));
    RETURN_IF_ERROR(SetField(table_data.get(), kMeterPeakBurstKbits,
                             BytesPerSecondToKbits(pburst)));
  }

  auto bf_dev_tgt = GetDeviceTarget(device);
  if (meter_index) {
    // Single index target.
    // Meter key: $METER_INDEX
    RETURN_IF_ERROR(
        SetField(table_key.get(), kMeterIndex, meter_index.value()));
    RETURN_IF_BFRT_ERROR(table->tableEntryMod(
        *real_session->bfrt_session_, bf_dev_tgt, *table_key, *table_data));
  } else {
    // Wildcard write to all indices.
    size_t table_size;
#if defined(SDE_9_4_0)
    RETURN_IF_BFRT_ERROR(table->tableSizeGet(*real_session->bfrt_session_,
                                             bf_dev_tgt, &table_size));
#else
    RETURN_IF_BFRT_ERROR(table->tableSizeGet(&table_size));
#endif  // SDE_9_4_0
    for (size_t i = 0; i < table_size; ++i) {
      // Meter key: $METER_INDEX
      RETURN_IF_ERROR(SetField(table_key.get(), kMeterIndex, i));
      RETURN_IF_BFRT_ERROR(table->tableEntryMod(
          *real_session->bfrt_session_, bf_dev_tgt, *table_key, *table_data));
    }
  }

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::ReadIndirectMeters(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 table_id, absl::optional<uint32> meter_index,
    std::vector<uint32>* meter_indices, std::vector<uint64>* cirs,
    std::vector<uint64>* cbursts, std::vector<uint64>* pirs,
    std::vector<uint64>* pbursts, std::vector<bool>* in_pps) {
  CHECK_RETURN_IF_FALSE(meter_indices);
  CHECK_RETURN_IF_FALSE(cirs);
  CHECK_RETURN_IF_FALSE(cbursts);
  CHECK_RETURN_IF_FALSE(pirs);
  CHECK_RETURN_IF_FALSE(pbursts);
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  auto bf_dev_tgt = GetDeviceTarget(device);
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));
  std::vector<std::unique_ptr<bfrt::BfRtTableKey>> keys;
  std::vector<std::unique_ptr<bfrt::BfRtTableData>> datums;

  // Is this a wildcard read?
  if (meter_index) {
    keys.resize(1);
    datums.resize(1);
    RETURN_IF_BFRT_ERROR(table->keyAllocate(&keys[0]));
    RETURN_IF_BFRT_ERROR(table->dataAllocate(&datums[0]));

    // Key: $METER_INDEX
    RETURN_IF_ERROR(SetField(keys[0].get(), kMeterIndex, meter_index.value()));
    RETURN_IF_BFRT_ERROR(table->tableEntryGet(
        *real_session->bfrt_session_, bf_dev_tgt, *keys[0],
        bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, datums[0].get()));
  } else {
    RETURN_IF_ERROR(GetAllEntries(real_session->bfrt_session_, bf_dev_tgt,
                                  table, &keys, &datums));
  }

  meter_indices->resize(0);
  cirs->resize(0);
  cbursts->resize(0);
  pirs->resize(0);
  pbursts->resize(0);
  in_pps->resize(0);
  for (size_t i = 0; i < keys.size(); ++i) {
    const std::unique_ptr<bfrt::BfRtTableData>& table_data = datums[i];
    const std::unique_ptr<bfrt::BfRtTableKey>& table_key = keys[i];
    // Key: $METER_INDEX
    uint64 bf_meter_index;
    RETURN_IF_ERROR(GetField(*table_key, kMeterIndex, &bf_meter_index));
    meter_indices->push_back(bf_meter_index);

    // Data: $METER_SPEC_*
    std::vector<bf_rt_id_t> data_field_ids;
    RETURN_IF_BFRT_ERROR(table->dataFieldIdListGet(&data_field_ids));
    for (const auto& field_id : data_field_ids) {
      std::string field_name;
      RETURN_IF_BFRT_ERROR(table->dataFieldNameGet(field_id, &field_name));
      if (field_name == kMeterCirKbps) {  // kbits
        uint64 cir;
        RETURN_IF_BFRT_ERROR(table_data->getValue(field_id, &cir));
        cirs->push_back(KbitsToBytesPerSecond(cir));
        in_pps->push_back(false);
      } else if (field_name == kMeterCommitedBurstKbits) {
        uint64 cburst;
        RETURN_IF_BFRT_ERROR(table_data->getValue(field_id, &cburst));
        cbursts->push_back(KbitsToBytesPerSecond(cburst));
      } else if (field_name == kMeterPirKbps) {
        uint64 pir;
        RETURN_IF_BFRT_ERROR(table_data->getValue(field_id, &pir));
        pirs->push_back(KbitsToBytesPerSecond(pir));
      } else if (field_name == kMeterPeakBurstKbits) {
        uint64 pburst;
        RETURN_IF_BFRT_ERROR(table_data->getValue(field_id, &pburst));
        pbursts->push_back(KbitsToBytesPerSecond(pburst));
      } else if (field_name == kMeterCirPps) {  // Packets
        uint64 cir;
        RETURN_IF_BFRT_ERROR(table_data->getValue(field_id, &cir));
        cirs->push_back(cir);
        in_pps->push_back(true);
      } else if (field_name == kMeterCommitedBurstPackets) {
        uint64 cburst;
        RETURN_IF_BFRT_ERROR(table_data->getValue(field_id, &cburst));
        cbursts->push_back(cburst);
      } else if (field_name == kMeterPirPps) {
        uint64 pir;
        RETURN_IF_BFRT_ERROR(table_data->getValue(field_id, &pir));
        pirs->push_back(pir);
      } else if (field_name == kMeterPeakBurstPackets) {
        uint64 pburst;
        RETURN_IF_BFRT_ERROR(table_data->getValue(field_id, &pburst));
        pbursts->push_back(pburst);
      } else {
        RETURN_ERROR(ERR_INVALID_PARAM)
            << "Unknown meter field " << field_name << " in meter with id "
            << table_id << ".";
      }
    }
  }

  CHECK_EQ(meter_indices->size(), keys.size());
  CHECK_EQ(cirs->size(), keys.size());
  CHECK_EQ(cbursts->size(), keys.size());
  CHECK_EQ(pirs->size(), keys.size());
  CHECK_EQ(pbursts->size(), keys.size());
  CHECK_EQ(in_pps->size(), keys.size());

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::WriteActionProfileMember(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 table_id, int member_id, const TableDataInterface* table_data,
    bool insert) {
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);
  auto real_table_data = dynamic_cast<const TableData*>(table_data);
  CHECK_RETURN_IF_FALSE(real_table_data);

  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));

  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  // Key: $ACTION_MEMBER_ID
  RETURN_IF_ERROR(SetField(table_key.get(), "$ACTION_MEMBER_ID", member_id));

  auto bf_dev_tgt = GetDeviceTarget(device);
  if (insert) {
    RETURN_IF_BFRT_ERROR(table->tableEntryAdd(*real_session->bfrt_session_,
                                              bf_dev_tgt, *table_key,
                                              *real_table_data->table_data_));
  } else {
    RETURN_IF_BFRT_ERROR(table->tableEntryMod(*real_session->bfrt_session_,
                                              bf_dev_tgt, *table_key,
                                              *real_table_data->table_data_));
  }

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::InsertActionProfileMember(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 table_id, int member_id, const TableDataInterface* table_data) {
  ::absl::ReaderMutexLock l(&data_lock_);
  return WriteActionProfileMember(device, session, table_id, member_id,
                                  table_data, true);
}

::util::Status BfSdeWrapper::ModifyActionProfileMember(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 table_id, int member_id, const TableDataInterface* table_data) {
  ::absl::ReaderMutexLock l(&data_lock_);
  return WriteActionProfileMember(device, session, table_id, member_id,
                                  table_data, false);
}

::util::Status BfSdeWrapper::DeleteActionProfileMember(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 table_id, int member_id) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));

  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));

  // Key: $ACTION_MEMBER_ID
  RETURN_IF_ERROR(SetField(table_key.get(), "$ACTION_MEMBER_ID", member_id));

  auto bf_dev_tgt = GetDeviceTarget(device);
  RETURN_IF_BFRT_ERROR(table->tableEntryDel(*real_session->bfrt_session_,
                                            bf_dev_tgt, *table_key));

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::GetActionProfileMembers(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 table_id, int member_id, std::vector<int>* member_ids,
    std::vector<std::unique_ptr<TableDataInterface>>* table_datas) {
  CHECK_RETURN_IF_FALSE(member_ids);
  CHECK_RETURN_IF_FALSE(table_datas);
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  auto bf_dev_tgt = GetDeviceTarget(device);
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));
  std::vector<std::unique_ptr<bfrt::BfRtTableKey>> keys;
  std::vector<std::unique_ptr<bfrt::BfRtTableData>> datums;
  // Is this a wildcard read?
  if (member_id != 0) {
    keys.resize(1);
    datums.resize(1);
    RETURN_IF_BFRT_ERROR(table->keyAllocate(&keys[0]));
    RETURN_IF_BFRT_ERROR(table->dataAllocate(&datums[0]));
    // Key: $ACTION_MEMBER_ID
    RETURN_IF_ERROR(SetField(keys[0].get(), "$ACTION_MEMBER_ID", member_id));
    RETURN_IF_BFRT_ERROR(table->tableEntryGet(
        *real_session->bfrt_session_, bf_dev_tgt, *keys[0],
        bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, datums[0].get()));
  } else {
    RETURN_IF_ERROR(GetAllEntries(real_session->bfrt_session_, bf_dev_tgt,
                                  table, &keys, &datums));
  }

  member_ids->resize(0);
  table_datas->resize(0);
  for (size_t i = 0; i < keys.size(); ++i) {
    // Key: $sid
    uint64 member_id;
    RETURN_IF_ERROR(GetField(*keys[i], "$ACTION_MEMBER_ID", &member_id));
    member_ids->push_back(member_id);

    // Data: action params
    auto td = absl::make_unique<TableData>(std::move(datums[i]));
    table_datas->push_back(std::move(td));
  }

  CHECK_EQ(member_ids->size(), keys.size());
  CHECK_EQ(table_datas->size(), keys.size());

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::WriteActionProfileGroup(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 table_id, int group_id, int max_group_size,
    const std::vector<uint32>& member_ids,
    const std::vector<bool>& member_status, bool insert) {
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));

  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));

  // Key: $SELECTOR_GROUP_ID
  RETURN_IF_ERROR(SetField(table_key.get(), "$SELECTOR_GROUP_ID", group_id));
  // Data: $ACTION_MEMBER_ID
  RETURN_IF_ERROR(SetField(table_data.get(), "$ACTION_MEMBER_ID", member_ids));
  // Data: $ACTION_MEMBER_STATUS
  RETURN_IF_ERROR(
      SetField(table_data.get(), "$ACTION_MEMBER_STATUS", member_status));
  // Data: $MAX_GROUP_SIZE
  RETURN_IF_ERROR(
      SetField(table_data.get(), "$MAX_GROUP_SIZE", max_group_size));

  auto bf_dev_tgt = GetDeviceTarget(device);
  if (insert) {
    RETURN_IF_BFRT_ERROR(table->tableEntryAdd(
        *real_session->bfrt_session_, bf_dev_tgt, *table_key, *table_data));
  } else {
    RETURN_IF_BFRT_ERROR(table->tableEntryMod(
        *real_session->bfrt_session_, bf_dev_tgt, *table_key, *table_data));
  }

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::InsertActionProfileGroup(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 table_id, int group_id, int max_group_size,
    const std::vector<uint32>& member_ids,
    const std::vector<bool>& member_status) {
  ::absl::ReaderMutexLock l(&data_lock_);
  return WriteActionProfileGroup(device, session, table_id, group_id,
                                 max_group_size, member_ids, member_status,
                                 true);
}

::util::Status BfSdeWrapper::ModifyActionProfileGroup(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 table_id, int group_id, int max_group_size,
    const std::vector<uint32>& member_ids,
    const std::vector<bool>& member_status) {
  ::absl::ReaderMutexLock l(&data_lock_);
  return WriteActionProfileGroup(device, session, table_id, group_id,
                                 max_group_size, member_ids, member_status,
                                 false);
}

::util::Status BfSdeWrapper::DeleteActionProfileGroup(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 table_id, int group_id) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));
  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  // Key: $SELECTOR_GROUP_ID
  RETURN_IF_ERROR(SetField(table_key.get(), "$SELECTOR_GROUP_ID", group_id));
  auto bf_dev_tgt = GetDeviceTarget(device);
  RETURN_IF_BFRT_ERROR(table->tableEntryDel(*real_session->bfrt_session_,
                                            bf_dev_tgt, *table_key));

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::GetActionProfileGroups(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 table_id, int group_id, std::vector<int>* group_ids,
    std::vector<int>* max_group_sizes,
    std::vector<std::vector<uint32>>* member_ids,
    std::vector<std::vector<bool>>* member_status) {
  CHECK_RETURN_IF_FALSE(group_ids);
  CHECK_RETURN_IF_FALSE(max_group_sizes);
  CHECK_RETURN_IF_FALSE(member_ids);
  CHECK_RETURN_IF_FALSE(member_status);
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  auto bf_dev_tgt = GetDeviceTarget(device);
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));
  std::vector<std::unique_ptr<bfrt::BfRtTableKey>> keys;
  std::vector<std::unique_ptr<bfrt::BfRtTableData>> datums;
  // Is this a wildcard read?
  if (group_id != 0) {
    keys.resize(1);
    datums.resize(1);
    RETURN_IF_BFRT_ERROR(table->keyAllocate(&keys[0]));
    RETURN_IF_BFRT_ERROR(table->dataAllocate(&datums[0]));
    // Key: $SELECTOR_GROUP_ID
    RETURN_IF_ERROR(SetField(keys[0].get(), "$SELECTOR_GROUP_ID", group_id));
    RETURN_IF_BFRT_ERROR(table->tableEntryGet(
        *real_session->bfrt_session_, bf_dev_tgt, *keys[0],
        bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, datums[0].get()));
  } else {
    RETURN_IF_ERROR(GetAllEntries(real_session->bfrt_session_, bf_dev_tgt,
                                  table, &keys, &datums));
  }

  group_ids->resize(0);
  max_group_sizes->resize(0);
  member_ids->resize(0);
  member_status->resize(0);
  for (size_t i = 0; i < keys.size(); ++i) {
    const std::unique_ptr<bfrt::BfRtTableData>& table_data = datums[i];
    const std::unique_ptr<bfrt::BfRtTableKey>& table_key = keys[i];
    // Key: $SELECTOR_GROUP_ID
    uint64 group_id;
    RETURN_IF_ERROR(GetField(*table_key, "$SELECTOR_GROUP_ID", &group_id));
    group_ids->push_back(group_id);

    // Data: $MAX_GROUP_SIZE
    uint64 max_group_size;
    RETURN_IF_ERROR(GetField(*table_data, "$MAX_GROUP_SIZE", &max_group_size));
    max_group_sizes->push_back(max_group_size);

    // Data: $ACTION_MEMBER_ID
    std::vector<uint32> members;
    RETURN_IF_ERROR(GetField(*table_data, "$ACTION_MEMBER_ID", &members));
    member_ids->push_back(members);

    // Data: $ACTION_MEMBER_STATUS
    std::vector<bool> member_enabled;
    RETURN_IF_ERROR(
        GetField(*table_data, "$ACTION_MEMBER_STATUS", &member_enabled));
    member_status->push_back(member_enabled);
  }

  CHECK_EQ(group_ids->size(), keys.size());
  CHECK_EQ(max_group_sizes->size(), keys.size());
  CHECK_EQ(member_ids->size(), keys.size());
  CHECK_EQ(member_status->size(), keys.size());

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::InsertTableEntry(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 table_id, const TableKeyInterface* table_key,
    const TableDataInterface* table_data) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);
  auto real_table_key = dynamic_cast<const TableKey*>(table_key);
  CHECK_RETURN_IF_FALSE(real_table_key);
  auto real_table_data = dynamic_cast<const TableData*>(table_data);
  CHECK_RETURN_IF_FALSE(real_table_data);

  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));
  auto bf_dev_tgt = GetDeviceTarget(device);
  RETURN_IF_BFRT_ERROR(table->tableEntryAdd(
      *real_session->bfrt_session_, bf_dev_tgt, *real_table_key->table_key_,
      *real_table_data->table_data_));

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::ModifyTableEntry(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 table_id, const TableKeyInterface* table_key,
    const TableDataInterface* table_data) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);
  auto real_table_key = dynamic_cast<const TableKey*>(table_key);
  CHECK_RETURN_IF_FALSE(real_table_key);
  auto real_table_data = dynamic_cast<const TableData*>(table_data);
  CHECK_RETURN_IF_FALSE(real_table_data);
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));

  auto bf_dev_tgt = GetDeviceTarget(device);
  RETURN_IF_BFRT_ERROR(table->tableEntryMod(
      *real_session->bfrt_session_, bf_dev_tgt, *real_table_key->table_key_,
      *real_table_data->table_data_));

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::DeleteTableEntry(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 table_id, const TableKeyInterface* table_key) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);
  auto real_table_key = dynamic_cast<const TableKey*>(table_key);
  CHECK_RETURN_IF_FALSE(real_table_key);
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));

  auto bf_dev_tgt = GetDeviceTarget(device);
  RETURN_IF_BFRT_ERROR(table->tableEntryDel(
      *real_session->bfrt_session_, bf_dev_tgt, *real_table_key->table_key_));

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::GetTableEntry(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 table_id, const TableKeyInterface* table_key,
    TableDataInterface* table_data) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);
  auto real_table_key = dynamic_cast<const TableKey*>(table_key);
  CHECK_RETURN_IF_FALSE(real_table_key);
  auto real_table_data = dynamic_cast<const TableData*>(table_data);
  CHECK_RETURN_IF_FALSE(real_table_data);
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));

  auto bf_dev_tgt = GetDeviceTarget(device);
  RETURN_IF_BFRT_ERROR(table->tableEntryGet(
      *real_session->bfrt_session_, bf_dev_tgt, *real_table_key->table_key_,
      bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW,
      real_table_data->table_data_.get()));

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::GetAllTableEntries(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 table_id,
    std::vector<std::unique_ptr<TableKeyInterface>>* table_keys,
    std::vector<std::unique_ptr<TableDataInterface>>* table_datas) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));
  auto bf_dev_tgt = GetDeviceTarget(device);

  std::vector<std::unique_ptr<bfrt::BfRtTableKey>> keys;
  std::vector<std::unique_ptr<bfrt::BfRtTableData>> datums;
  RETURN_IF_ERROR(GetAllEntries(real_session->bfrt_session_, bf_dev_tgt, table,
                                &keys, &datums));

  table_keys->resize(0);
  table_datas->resize(0);

  for (size_t i = 0; i < keys.size(); ++i) {
    auto tk = absl::make_unique<TableKey>(std::move(keys[i]));
    auto td = absl::make_unique<TableData>(std::move(datums[i]));
    table_keys->push_back(std::move(tk));
    table_datas->push_back(std::move(td));
  }

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::SetDefaultTableEntry(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 table_id, const TableDataInterface* table_data) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);
  auto real_table_data = dynamic_cast<const TableData*>(table_data);
  CHECK_RETURN_IF_FALSE(real_table_data);
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));

  auto bf_dev_tgt = GetDeviceTarget(device);
  RETURN_IF_BFRT_ERROR(table->tableDefaultEntrySet(
      *real_session->bfrt_session_, bf_dev_tgt, *real_table_data->table_data_));

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::ResetDefaultTableEntry(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 table_id) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));

  auto bf_dev_tgt = GetDeviceTarget(device);
  RETURN_IF_BFRT_ERROR(
      table->tableDefaultEntryReset(*real_session->bfrt_session_, bf_dev_tgt));

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::GetDefaultTableEntry(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 table_id, TableDataInterface* table_data) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);
  auto real_table_data = dynamic_cast<const TableData*>(table_data);
  CHECK_RETURN_IF_FALSE(real_table_data);
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));

  auto bf_dev_tgt = GetDeviceTarget(device);
  RETURN_IF_BFRT_ERROR(table->tableDefaultEntryGet(
      *real_session->bfrt_session_, bf_dev_tgt,
      bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW,
      real_table_data->table_data_.get()));

  return ::util::OkStatus();
}

::util::StatusOr<uint32> BfSdeWrapper::GetBfRtId(uint32 p4info_id) const {
  ::absl::ReaderMutexLock l(&data_lock_);
  return bfrt_id_mapper_->GetBfRtId(p4info_id);
}

::util::StatusOr<uint32> BfSdeWrapper::GetP4InfoId(uint32 bfrt_id) const {
  ::absl::ReaderMutexLock l(&data_lock_);
  return bfrt_id_mapper_->GetP4InfoId(bfrt_id);
}

::util::StatusOr<uint32> BfSdeWrapper::GetActionSelectorBfRtId(
    uint32 action_profile_id) const {
  ::absl::ReaderMutexLock l(&data_lock_);
  return bfrt_id_mapper_->GetActionSelectorBfRtId(action_profile_id);
}

::util::StatusOr<uint32> BfSdeWrapper::GetActionProfileBfRtId(
    uint32 action_selector_id) const {
  ::absl::ReaderMutexLock l(&data_lock_);
  return bfrt_id_mapper_->GetActionProfileBfRtId(action_selector_id);
}

::util::Status BfSdeWrapper::SynchronizeCounters(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 table_id, absl::Duration timeout) {
  ::absl::ReaderMutexLock l(&data_lock_);
  return DoSynchronizeCounters(device, session, table_id, timeout);
}

::util::Status BfSdeWrapper::DoSynchronizeCounters(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 table_id, absl::Duration timeout) {
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));

  auto bf_dev_tgt = GetDeviceTarget(device);
  // Sync table counter
  std::set<bfrt::TableOperationsType> supported_ops;
  RETURN_IF_BFRT_ERROR(table->tableOperationsSupported(&supported_ops));
  if (supported_ops.count(bfrt::TableOperationsType::COUNTER_SYNC)) {
    auto sync_notifier = std::make_shared<absl::Notification>();
    std::weak_ptr<absl::Notification> weak_ref(sync_notifier);
    std::unique_ptr<bfrt::BfRtTableOperations> table_op;
    RETURN_IF_BFRT_ERROR(table->operationsAllocate(
        bfrt::TableOperationsType::COUNTER_SYNC, &table_op));
    RETURN_IF_BFRT_ERROR(table_op->counterSyncSet(
        *real_session->bfrt_session_, bf_dev_tgt,
        [table_id, weak_ref](const bf_rt_target_t& dev_tgt, void* cookie) {
          if (auto notifier = weak_ref.lock()) {
            VLOG(1) << "Table counter for table " << table_id << " synced.";
            notifier->Notify();
          } else {
            VLOG(1) << "Notifier expired before table " << table_id
                    << " could be synced.";
          }
        },
        nullptr));
    RETURN_IF_BFRT_ERROR(table->tableOperationsExecute(*table_op.get()));
    // Wait until sync done or timeout.
    if (!sync_notifier->WaitForNotificationWithTimeout(timeout)) {
      return MAKE_ERROR(ERR_OPER_TIMEOUT)
             << "Timeout while syncing (indirect) table counters of table "
             << table_id << ".";
    }
  }

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::SynchronizeRegisters(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 table_id, absl::Duration timeout) {
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));

  auto bf_dev_tgt = GetDeviceTarget(device);
  // Sync table registers.
  std::set<bfrt::TableOperationsType> supported_ops;
  RETURN_IF_BFRT_ERROR(table->tableOperationsSupported(&supported_ops));
  if (supported_ops.count(bfrt::TableOperationsType::REGISTER_SYNC)) {
    auto sync_notifier = std::make_shared<absl::Notification>();
    std::weak_ptr<absl::Notification> weak_ref(sync_notifier);
    std::unique_ptr<bfrt::BfRtTableOperations> table_op;
    RETURN_IF_BFRT_ERROR(table->operationsAllocate(
        bfrt::TableOperationsType::REGISTER_SYNC, &table_op));
    RETURN_IF_BFRT_ERROR(table_op->registerSyncSet(
        *real_session->bfrt_session_, bf_dev_tgt,
        [table_id, weak_ref](const bf_rt_target_t& dev_tgt, void* cookie) {
          if (auto notifier = weak_ref.lock()) {
            VLOG(1) << "Table registers for table " << table_id << " synced.";
            notifier->Notify();
          } else {
            VLOG(1) << "Notifier expired before table " << table_id
                    << " could be synced.";
          }
        },
        nullptr));
    RETURN_IF_BFRT_ERROR(table->tableOperationsExecute(*table_op.get()));
    // Wait until sync done or timeout.
    if (!sync_notifier->WaitForNotificationWithTimeout(timeout)) {
      return MAKE_ERROR(ERR_OPER_TIMEOUT)
             << "Timeout while syncing (indirect) table registers of table "
             << table_id << ".";
    }
  }

  return ::util::OkStatus();
}

BfSdeWrapper* BfSdeWrapper::CreateSingleton() {
  absl::WriterMutexLock l(&init_lock_);
  if (!singleton_) {
    singleton_ = new BfSdeWrapper();
  }

  return singleton_;
}

BfSdeWrapper* BfSdeWrapper::GetSingleton() {
  absl::ReaderMutexLock l(&init_lock_);
  return singleton_;
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
