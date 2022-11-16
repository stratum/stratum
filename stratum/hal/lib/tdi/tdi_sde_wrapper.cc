// Copyright 2019-present Barefoot Networks, Inc.
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// Target-agnostic TDI SDE wrapper methods.

#include "stratum/hal/lib/tdi/tdi_sde_wrapper.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <ostream>
#include <set>
#include <utility>

#include "absl/hash/hash.h"
#include "absl/strings/match.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "stratum/glue/gtl/stl_util.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/logging.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/barefoot/utils.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/p4/utils.h"
#include "stratum/hal/lib/tdi/macros.h"
#include "stratum/hal/lib/tdi/tdi_constants.h"
#include "stratum/lib/channel/channel.h"
#include "stratum/lib/utils.h"

#ifdef DPDK_TARGET
#include "tdi_rt/tdi_rt_defs.h"
#elif TOFINO_TARGET
#include "tdi_tofino/tdi_tofino_defs.h"
#else
#error "Unknown backend"
#endif

DEFINE_string(tdi_sde_config_dir, "/var/run/stratum/tdi_config",
              "The dir used by the SDE to load the device configuration.");

namespace stratum {
namespace hal {
namespace tdi {

constexpr absl::Duration TdiSdeWrapper::kWriteTimeout;
constexpr int32 TdiSdeWrapper::kBfDefaultMtu;
constexpr int TdiSdeWrapper::MAX_PORT_HDL_STRING_LEN;
constexpr int TdiSdeWrapper::_PI_UPDATE_MAX_NAME_SIZE;

using barefoot::NumBitsToNumBytes;

// Helper functions for dealing with the TDI API.
namespace {
// Convert kbit/s to bytes/s (* 1000 / 8).
inline constexpr uint64 KbitsToBytesPerSecond(uint64 kbps) {
  return kbps * 125;
}

// Convert bytes/s to kbit/s (/ 1000 * 8).
inline constexpr uint64 BytesPerSecondToKbits(uint64 bytes) {
  return bytes / 125;
}

::util::StatusOr<std::string> DumpTableMetadata(const ::tdi::Table* table) {
  std::string table_name = table->tableInfoGet()->nameGet().c_str();
  tdi_id_t table_id = table->tableInfoGet()->idGet();
  tdi_table_type_e table_type = table->tableInfoGet()->tableTypeGet();

  return absl::StrCat("table_name: ", table_name, ", table_id: ", table_id,
                      ", table_type: ", table_type);
}

::util::StatusOr<std::string> DumpTableKey(const ::tdi::TableKey* table_key) {
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(table_key->tableGet(&table));
  std::vector<tdi_id_t> key_field_ids;
  key_field_ids = table->tableInfoGet()->keyFieldIdListGet();

  std::string s;
  absl::StrAppend(&s, "tdi_table_key { ");
  for (const auto& field_id : key_field_ids) {
    const ::tdi::KeyFieldInfo* keyFieldInfo =
        table->tableInfoGet()->keyFieldGet(field_id);
    std::string field_name;
    tdi_match_type_core_e key_type;
    size_t field_size;

    RET_CHECK(keyFieldInfo);
    field_name = keyFieldInfo->nameGet().c_str();
    key_type =
        static_cast<tdi_match_type_core_e>((*keyFieldInfo).matchTypeGet());
    field_size = keyFieldInfo->sizeGet();
    std::string value;

    switch (key_type) {
      case TDI_MATCH_TYPE_EXACT: {
        std::string v(NumBitsToNumBytes(field_size), '\x00');
        const char* valueExact = reinterpret_cast<const char*>(v.data());
        size_t size = reinterpret_cast<size_t>(v.size());

        ::tdi::KeyFieldValueExact<const char*> exactKey(valueExact, size);
        RETURN_IF_TDI_ERROR(table_key->getValue(field_id, &exactKey));
        value = absl::StrCat("0x", StringToHex(v));
        break;
      }

      case TDI_MATCH_TYPE_TERNARY: {
        std::string v(NumBitsToNumBytes(field_size), '\x00');
        std::string m(NumBitsToNumBytes(field_size), '\x00');
        const char* valueTernary = reinterpret_cast<const char*>(v.data());
        const char* maskTernary = reinterpret_cast<const char*>(m.data());
        size_t sizeTernary = reinterpret_cast<size_t>(v.size());
        ::tdi::KeyFieldValueTernary<const char*> ternaryKey(
            valueTernary, maskTernary, sizeTernary);

        RETURN_IF_TDI_ERROR(table_key->getValue(field_id, &ternaryKey));
        value = absl::StrCat("0x", StringToHex(v), " & ", "0x", StringToHex(m));
        break;
      }

      case TDI_MATCH_TYPE_RANGE: {
        std::string l(NumBitsToNumBytes(field_size), '\x00');
        std::string h(NumBitsToNumBytes(field_size), '\x00');
        const char* lowRange = reinterpret_cast<const char*>(l.data());
        const char* highRange = reinterpret_cast<const char*>(h.data());
        size_t sizeRange = reinterpret_cast<size_t>(l.size());
        ::tdi::KeyFieldValueRange<const char*> rangeKey(lowRange, highRange,
                                                        sizeRange);
        RETURN_IF_TDI_ERROR(table_key->getValue(field_id, &rangeKey));
        value = absl::StrCat("0x", StringToHex(l), " - ", "0x", StringToHex(h));
        break;
      }

      case TDI_MATCH_TYPE_LPM: {
        std::string v(NumBitsToNumBytes(field_size), '\x00');
        uint16 prefix_length = 0;
        const char* valueLpm = reinterpret_cast<const char*>(v.data());
        size_t sizeLpm = reinterpret_cast<size_t>(v.size());
        ::tdi::KeyFieldValueLPM<const char*> lpmKey(valueLpm, prefix_length,
                                                    sizeLpm);
        RETURN_IF_TDI_ERROR(table_key->getValue(field_id, &lpmKey));
        value = absl::StrCat("0x", StringToHex(v), "/", prefix_length);
        break;
      }
      default:
        return MAKE_ERROR(ERR_INTERNAL)
               << "Unknown key_type: " << static_cast<int>(key_type) << ".";
    }

    absl::StrAppend(&s, field_name, " { field_id: ", field_id,
                    " key_type: ", static_cast<int>(key_type),
                    " field_size: ", field_size, " value: ", value, " } ");
  }
  absl::StrAppend(&s, "}");
  return s;
}

::util::StatusOr<std::string> DumpTableData(
    const ::tdi::TableData* table_data) {
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(table_data->getParent(&table));

  std::string s;
  absl::StrAppend(&s, "tdi_table_data { ");
  std::vector<tdi_id_t> data_field_ids;

  tdi_id_t action_id = table_data->actionIdGet();
  absl::StrAppend(&s, "action_id: ", action_id, " ");
  data_field_ids = table->tableInfoGet()->dataFieldIdListGet(action_id);

  for (const auto& field_id : data_field_ids) {
    std::string field_name;
    tdi_field_data_type_e data_type;
    size_t field_size;
    bool is_active;
    const ::tdi::DataFieldInfo* dataFieldInfo;
    dataFieldInfo = table->tableInfoGet()->dataFieldGet(field_id, action_id);
    RET_CHECK(dataFieldInfo);

    field_name = dataFieldInfo->nameGet().c_str();
    data_type = dataFieldInfo->dataTypeGet();
    field_size = dataFieldInfo->sizeGet();
    RETURN_IF_TDI_ERROR(table_data->isActive(field_id, &is_active));

    std::string value;
    switch (data_type) {
      case TDI_FIELD_DATA_TYPE_UINT64: {
        uint64 v;
        RETURN_IF_TDI_ERROR(table_data->getValue(field_id, &v));
        value = std::to_string(v);
        break;
      }
      case TDI_FIELD_DATA_TYPE_BYTE_STREAM: {
        std::string v(NumBitsToNumBytes(field_size), '\x00');
        RETURN_IF_TDI_ERROR(table_data->getValue(
            field_id, v.size(),
            reinterpret_cast<uint8*>(gtl::string_as_array(&v))));
        value = absl::StrCat("0x", StringToHex(v));
        break;
      }
      case TDI_FIELD_DATA_TYPE_INT_ARR: {
        // TODO(max): uint32 seems to be the most common type, but we could
        // differentiate based on field_size, if needed.
        std::vector<uint32> v;
        RETURN_IF_TDI_ERROR(table_data->getValue(field_id, &v));
        value = PrintVector(v, ",");
        break;
      }
      case TDI_FIELD_DATA_TYPE_BOOL_ARR: {
        std::vector<bool> bools;
        RETURN_IF_TDI_ERROR(table_data->getValue(field_id, &bools));
        std::vector<uint16> bools_as_ints;
        for (bool b : bools) {
          bools_as_ints.push_back(b);
        }
        value = PrintVector(bools_as_ints, ",");
        break;
      }
      default:
        return MAKE_ERROR(ERR_INTERNAL)
               << "Unknown data_type: " << static_cast<int>(data_type) << ".";
    }

    absl::StrAppend(&s, field_name, " { field_id: ", field_id,
                    " data_type: ", static_cast<int>(data_type),
                    " field_size: ", field_size, " value: ", value,
                    " is_active: ", is_active, " } ");
  }
  absl::StrAppend(&s, "}");

  return s;
}

::util::Status GetFieldExact(const ::tdi::TableKey& table_key,
                             std::string field_name, uint32* field_value) {
  tdi_id_t field_id;
  const ::tdi::Table* table;
  tdi_field_data_type_e data_type;
  RETURN_IF_TDI_ERROR(table_key.tableGet(&table));
  ::tdi::KeyFieldValueExact<uint64_t> key_field_value(*field_value);
  const ::tdi::KeyFieldInfo* keyFieldInfo =
      table->tableInfoGet()->keyFieldGet(field_name);
  RET_CHECK(keyFieldInfo);

  field_id = keyFieldInfo->idGet();
  data_type = keyFieldInfo->dataTypeGet();

  RET_CHECK(data_type == TDI_FIELD_DATA_TYPE_UINT64)
      << "Requested uint64 but field " << field_name << " has type "
      << static_cast<int>(data_type);

  RETURN_IF_TDI_ERROR(table_key.getValue(field_id, &key_field_value));

  *field_value = key_field_value.value_;

  return ::util::OkStatus();
}

::util::Status SetFieldExact(::tdi::TableKey* table_key, std::string field_name,
                             uint64 field_value) {
  ::tdi::KeyFieldValueExact<uint64_t> key_field_value(field_value);
  const ::tdi::Table* table;
  tdi_id_t field_id;
  tdi_field_data_type_e data_type;
  RETURN_IF_TDI_ERROR(table_key->tableGet(&table));
  const ::tdi::KeyFieldInfo* keyFieldInfo =
      table->tableInfoGet()->keyFieldGet(field_name);
  RET_CHECK(keyFieldInfo);

  field_id = keyFieldInfo->idGet();
  data_type = keyFieldInfo->dataTypeGet();

  RET_CHECK(data_type == TDI_FIELD_DATA_TYPE_UINT64)
      << "Setting uint64 but field " << field_name << " has type "
      << static_cast<int>(data_type);
  RETURN_IF_TDI_ERROR(table_key->setValue(field_id, key_field_value));
  return ::util::OkStatus();
}

::util::Status SetField(::tdi::TableKey* table_key, std::string field_name,
                        ::tdi::KeyFieldValue value) {
  tdi_id_t field_id;
  const ::tdi::Table* table;
  tdi_field_data_type_e data_type;
  RETURN_IF_TDI_ERROR(table_key->tableGet(&table));
  const ::tdi::KeyFieldInfo* keyFieldInfo =
      table->tableInfoGet()->keyFieldGet(field_name);
  RET_CHECK(keyFieldInfo);

  field_id = keyFieldInfo->idGet();
  data_type = keyFieldInfo->dataTypeGet();

  RET_CHECK(data_type == TDI_FIELD_DATA_TYPE_UINT64)
      << "Setting uint64 but field " << field_name << " has type "
      << static_cast<int>(data_type);
  RETURN_IF_TDI_ERROR(table_key->setValue(field_id, value));

  return ::util::OkStatus();
}

::util::Status GetField(const ::tdi::TableData& table_data,
                        std::string field_name, uint64* field_value) {
  tdi_id_t field_id;
  const ::tdi::Table* table;
  tdi_field_data_type_e data_type;
  const ::tdi::DataFieldInfo* dataFieldInfo;
  RETURN_IF_TDI_ERROR(table_data.getParent(&table));

  tdi_id_t action_id = table_data.actionIdGet();
  dataFieldInfo = table->tableInfoGet()->dataFieldGet(field_name, action_id);
  RET_CHECK(dataFieldInfo);
  field_id = dataFieldInfo->idGet();
  data_type = dataFieldInfo->dataTypeGet();

  RET_CHECK(data_type == TDI_FIELD_DATA_TYPE_UINT64)
      << "Requested uint64 but field " << field_name << " has type "
      << static_cast<int>(data_type);
  RETURN_IF_TDI_ERROR(table_data.getValue(field_id, field_value));

  return ::util::OkStatus();
}

::util::Status GetField(const ::tdi::TableData& table_data,
                        std::string field_name, std::string* field_value) {
  tdi_id_t field_id;
  const ::tdi::Table* table;
  tdi_field_data_type_e data_type;
  const ::tdi::DataFieldInfo* dataFieldInfo;
  RETURN_IF_TDI_ERROR(table_data.getParent(&table));

  tdi_id_t action_id = table_data.actionIdGet();
  dataFieldInfo = table->tableInfoGet()->dataFieldGet(field_name, action_id);
  RET_CHECK(dataFieldInfo);
  field_id = dataFieldInfo->idGet();
  data_type = dataFieldInfo->dataTypeGet();

  RET_CHECK(data_type == TDI_FIELD_DATA_TYPE_STRING)
      << "Requested string but field " << field_name << " has type "
      << static_cast<int>(data_type);
  RETURN_IF_TDI_ERROR(table_data.getValue(field_id, field_value));

  return ::util::OkStatus();
}

::util::Status GetFieldBool(const ::tdi::TableData& table_data,
                            std::string field_name, bool* field_value) {
  tdi_id_t field_id;
  const ::tdi::Table* table;
  tdi_field_data_type_e data_type;
  const ::tdi::DataFieldInfo* dataFieldInfo;
  RETURN_IF_TDI_ERROR(table_data.getParent(&table));

  tdi_id_t action_id = table_data.actionIdGet();
  dataFieldInfo = table->tableInfoGet()->dataFieldGet(field_name, action_id);
  RET_CHECK(dataFieldInfo);
  field_id = dataFieldInfo->idGet();
  data_type = dataFieldInfo->dataTypeGet();

  RET_CHECK(data_type == TDI_FIELD_DATA_TYPE_BOOL)
      << "Requested bool but field " << field_name << " has type "
      << static_cast<int>(data_type);
  RETURN_IF_TDI_ERROR(table_data.getValue(field_id, field_value));

  return ::util::OkStatus();
}
template <typename T>
::util::Status GetField(const ::tdi::TableData& table_data,
                        std::string field_name, std::vector<T>* field_values) {
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(table_data.getParent(&table));

  const ::tdi::DataFieldInfo* dataFieldInfo;
  tdi_id_t action_id = table_data.actionIdGet();
  dataFieldInfo = table->tableInfoGet()->dataFieldGet(field_name, action_id);
  RET_CHECK(dataFieldInfo);

  auto field_id = dataFieldInfo->idGet();
  auto data_type = dataFieldInfo->dataTypeGet();
  RET_CHECK(data_type == TDI_FIELD_DATA_TYPE_INT_ARR ||
            data_type == TDI_FIELD_DATA_TYPE_BOOL_ARR)
      << "Requested array but field has type " << static_cast<int>(data_type);
  RETURN_IF_TDI_ERROR(table_data.getValue(field_id, field_values));

  return ::util::OkStatus();
}

::util::Status SetField(::tdi::TableData* table_data, std::string field_name,
                        const uint64& value) {
  tdi_id_t field_id;
  const ::tdi::Table* table;
  tdi_field_data_type_e data_type;
  const ::tdi::DataFieldInfo* dataFieldInfo;
  RETURN_IF_TDI_ERROR(table_data->getParent(&table));

  tdi_id_t action_id = table_data->actionIdGet();
  dataFieldInfo = table->tableInfoGet()->dataFieldGet(field_name, action_id);
  RET_CHECK(dataFieldInfo);
  field_id = dataFieldInfo->idGet();
  data_type = dataFieldInfo->dataTypeGet();

  RET_CHECK(data_type == TDI_FIELD_DATA_TYPE_UINT64)
      << "Setting uint64 but field " << field_name << " has type "
      << static_cast<int>(data_type);
  RETURN_IF_TDI_ERROR(table_data->setValue(field_id, value));

  return ::util::OkStatus();
}

::util::Status SetField(::tdi::TableData* table_data, std::string field_name,
                        const std::string& field_value) {
  tdi_id_t field_id;
  const ::tdi::Table* table;
  tdi_field_data_type_e data_type;
  const ::tdi::DataFieldInfo* dataFieldInfo;
  RETURN_IF_TDI_ERROR(table_data->getParent(&table));

  tdi_id_t action_id = table_data->actionIdGet();
  dataFieldInfo = table->tableInfoGet()->dataFieldGet(field_name, action_id);
  RET_CHECK(dataFieldInfo);
  field_id = dataFieldInfo->idGet();
  data_type = dataFieldInfo->dataTypeGet();

  RET_CHECK(data_type == TDI_FIELD_DATA_TYPE_STRING)
      << "Setting string but field " << field_name << " has type "
      << static_cast<int>(data_type);
  RETURN_IF_TDI_ERROR(table_data->setValue(field_id, field_value));

  return ::util::OkStatus();
}

::util::Status SetFieldBool(::tdi::TableData* table_data,
                            std::string field_name, const bool& field_value) {
  tdi_id_t field_id;
  const ::tdi::Table* table;
  tdi_field_data_type_e data_type;
  const ::tdi::DataFieldInfo* dataFieldInfo;
  RETURN_IF_TDI_ERROR(table_data->getParent(&table));

  tdi_id_t action_id = table_data->actionIdGet();
  dataFieldInfo = table->tableInfoGet()->dataFieldGet(field_name, action_id);
  RET_CHECK(dataFieldInfo);
  field_id = dataFieldInfo->idGet();
  data_type = dataFieldInfo->dataTypeGet();

  RET_CHECK(data_type == TDI_FIELD_DATA_TYPE_BOOL)
      << "Setting bool but field " << field_name << " has type "
      << static_cast<int>(data_type);
  RETURN_IF_TDI_ERROR(table_data->setValue(field_id, field_value));

  return ::util::OkStatus();
}

template <typename T>
::util::Status SetField(::tdi::TableData* table_data, std::string field_name,
                        const std::vector<T>& field_value) {
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(table_data->getParent(&table));

  auto action_id = table_data->actionIdGet();
  auto dataFieldInfo =
      table->tableInfoGet()->dataFieldGet(field_name, action_id);
  RET_CHECK(dataFieldInfo);

  auto field_id = dataFieldInfo->idGet();
  auto data_type = dataFieldInfo->dataTypeGet();
  RET_CHECK(data_type == TDI_FIELD_DATA_TYPE_INT_ARR ||
            data_type == TDI_FIELD_DATA_TYPE_BOOL_ARR)
      << "Requested array but field has type " << static_cast<int>(data_type);

  RETURN_IF_TDI_ERROR(table_data->setValue(field_id, field_value));

  return ::util::OkStatus();
}

// TDI does not provide a target-neutral way for us to determine whether a
// table is preallocated, so we provide our own means of detection.
bool IsPreallocatedTable(const ::tdi::Table& table) {
#ifdef DPDK_TARGET
  auto table_type =
      static_cast<tdi_rt_table_type_e>(table.tableInfoGet()->tableTypeGet());
  return table_type == TDI_RT_TABLE_TYPE_COUNTER ||
         table_type == TDI_RT_TABLE_TYPE_METER;
#elif TOFINO_TARGET
  auto table_type = static_cast<tdi_tofino_table_type_e>(
      table.tableInfoGet()->tableTypeGet());
  return table_type == TDI_TOFINO_TABLE_TYPE_COUNTER ||
         table_type == TDI_TOFINO_TABLE_TYPE_METER;
#else
#error "Unsupported backend"
#endif
}

::util::Status GetAllEntries(
    std::shared_ptr<::tdi::Session> tdi_session, ::tdi::Target tdi_dev_target,
    const ::tdi::Table* table,
    std::vector<std::unique_ptr<::tdi::TableKey>>* table_keys,
    std::vector<std::unique_ptr<::tdi::TableData>>* table_values) {
  RET_CHECK(table_keys) << "table_keys is null";
  RET_CHECK(table_values) << "table_values is null";

  // Get number of entries. Some types of tables are preallocated and are always
  // "full". The SDE does not support querying the usage on these.
  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(0, &device);
  ::tdi::Flags* flags = new ::tdi::Flags(0);
  uint32 entries = 0;
  if (IsPreallocatedTable(*table)) {
    size_t table_size;
    RETURN_IF_TDI_ERROR(
        table->sizeGet(*tdi_session, tdi_dev_target, *flags, &table_size));
    entries = table_size;
  } else {
    RETURN_IF_TDI_ERROR(
        table->usageGet(*tdi_session, tdi_dev_target, *flags, &entries));
  }

  table_keys->resize(0);
  table_values->resize(0);
  if (entries == 0) return ::util::OkStatus();

  // Get first entry.
  {
    std::unique_ptr<::tdi::TableKey> table_key;
    std::unique_ptr<::tdi::TableData> table_data;
    RETURN_IF_TDI_ERROR(table->keyAllocate(&table_key));
    RETURN_IF_TDI_ERROR(table->dataAllocate(&table_data));
    RETURN_IF_TDI_ERROR(table->entryGetFirst(*tdi_session, tdi_dev_target,
                                             *flags, table_key.get(),
                                             table_data.get()));

    table_keys->push_back(std::move(table_key));
    table_values->push_back(std::move(table_data));
  }
  if (entries == 1) return ::util::OkStatus();

  // Get all entries following the first.
  {
    std::vector<std::unique_ptr<::tdi::TableKey>> keys(entries - 1);
    std::vector<std::unique_ptr<::tdi::TableData>> data(keys.size());
    ::tdi::Table::keyDataPairs pairs;
    for (size_t i = 0; i < keys.size(); ++i) {
      RETURN_IF_TDI_ERROR(table->keyAllocate(&keys[i]));
      RETURN_IF_TDI_ERROR(table->dataAllocate(&data[i]));
      pairs.push_back(std::make_pair(keys[i].get(), data[i].get()));
    }
    uint32 actual = 0;
    RETURN_IF_TDI_ERROR(table->entryGetNextN(*tdi_session, tdi_dev_target,
                                             *flags, *(*table_keys)[0],
                                             pairs.size(), &pairs, &actual));

    table_keys->insert(table_keys->end(), std::make_move_iterator(keys.begin()),
                       std::make_move_iterator(keys.end()));
    table_values->insert(table_values->end(),
                         std::make_move_iterator(data.begin()),
                         std::make_move_iterator(data.end()));
  }

  CHECK(table_keys->size() == table_values->size());
  CHECK(table_keys->size() == entries);

  return ::util::OkStatus();
}
}  // namespace

::util::Status TableKey::SetExact(int id, const std::string& value) {
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(table_key_->tableGet(&table));
  size_t field_size_bits;
  auto tableInfo = table->tableInfoGet();
  const ::tdi::KeyFieldInfo* keyFieldInfo =
      tableInfo->keyFieldGet(static_cast<tdi_id_t>(id));
  RET_CHECK(keyFieldInfo);

  field_size_bits = keyFieldInfo->sizeGet();
  std::string v = P4RuntimeByteStringToPaddedByteString(
      value, NumBitsToNumBytes(field_size_bits));

  const char* valueExact = reinterpret_cast<const char*>(v.data());
  size_t size = reinterpret_cast<size_t>(v.size());

  ::tdi::KeyFieldValueExact<const char*> exactKey(valueExact, size);

  RETURN_IF_TDI_ERROR(
      table_key_->setValue(static_cast<tdi_id_t>(id), exactKey));

  return ::util::OkStatus();
}

::util::Status TableKey::SetTernary(int id, const std::string& value,
                                    const std::string& mask) {
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(table_key_->tableGet(&table));
  size_t field_size_bits;
  auto tableInfo = table->tableInfoGet();
  const ::tdi::KeyFieldInfo* keyFieldInfo =
      tableInfo->keyFieldGet(static_cast<tdi_id_t>(id));
  RET_CHECK(keyFieldInfo);

  field_size_bits = keyFieldInfo->sizeGet();
  std::string v = P4RuntimeByteStringToPaddedByteString(
      value, NumBitsToNumBytes(field_size_bits));
  std::string m = P4RuntimeByteStringToPaddedByteString(
      mask, NumBitsToNumBytes(field_size_bits));
  DCHECK_EQ(v.size(), m.size());
  const char* valueTernary = reinterpret_cast<const char*>(v.data());
  const char* maskTernary = reinterpret_cast<const char*>(m.data());
  size_t sizeTernary = reinterpret_cast<size_t>(v.size());

  ::tdi::KeyFieldValueTernary<const char*> ternaryKey(valueTernary, maskTernary,
                                                      sizeTernary);

  RETURN_IF_TDI_ERROR(
      table_key_->setValue(static_cast<tdi_id_t>(id), ternaryKey));
  return ::util::OkStatus();
}

::util::Status TableKey::SetLpm(int id, const std::string& prefix,
                                uint16 prefix_length) {
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(table_key_->tableGet(&table));
  size_t field_size_bits;
  auto tableInfo = table->tableInfoGet();
  const ::tdi::KeyFieldInfo* keyFieldInfo =
      tableInfo->keyFieldGet(static_cast<tdi_id_t>(id));
  RET_CHECK(keyFieldInfo);

  field_size_bits = keyFieldInfo->sizeGet();
  std::string p = P4RuntimeByteStringToPaddedByteString(
      prefix, NumBitsToNumBytes(field_size_bits));

  const char* valueLpm = reinterpret_cast<const char*>(p.data());
  size_t sizeLpm = reinterpret_cast<size_t>(p.size());
  ::tdi::KeyFieldValueLPM<const char*> lpmKey(valueLpm, prefix_length, sizeLpm);
  RETURN_IF_TDI_ERROR(table_key_->setValue(static_cast<tdi_id_t>(id), lpmKey));

  return ::util::OkStatus();
}

::util::Status TableKey::SetRange(int id, const std::string& low,
                                  const std::string& high) {
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(table_key_->tableGet(&table));
  size_t field_size_bits;
  auto tableInfo = table->tableInfoGet();
  const ::tdi::KeyFieldInfo* keyFieldInfo =
      tableInfo->keyFieldGet(static_cast<tdi_id_t>(id));
  RET_CHECK(keyFieldInfo);

  field_size_bits = keyFieldInfo->sizeGet();
  std::string l = P4RuntimeByteStringToPaddedByteString(
      low, NumBitsToNumBytes(field_size_bits));
  std::string h = P4RuntimeByteStringToPaddedByteString(
      high, NumBitsToNumBytes(field_size_bits));
  DCHECK_EQ(l.size(), h.size());

  const char* lowRange = reinterpret_cast<const char*>(l.data());
  const char* highRange = reinterpret_cast<const char*>(h.data());
  size_t sizeRange = reinterpret_cast<size_t>(l.size());
  ::tdi::KeyFieldValueRange<const char*> rangeKey(lowRange, highRange,
                                                  sizeRange);
  RETURN_IF_TDI_ERROR(
      table_key_->setValue(static_cast<tdi_id_t>(id), rangeKey));
  return ::util::OkStatus();
}

::util::Status TableKey::SetPriority(uint64 priority) {
  return SetFieldExact(table_key_.get(), kMatchPriority, priority);
}

::util::Status TableKey::GetExact(int id, std::string* value) const {
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(table_key_->tableGet(&table));
  size_t field_size_bits;
  auto tableInfo = table->tableInfoGet();
  const ::tdi::KeyFieldInfo* keyFieldInfo =
      tableInfo->keyFieldGet(static_cast<tdi_id_t>(id));
  RET_CHECK(keyFieldInfo);

  field_size_bits = keyFieldInfo->sizeGet();
  value->clear();
  value->resize(NumBitsToNumBytes(field_size_bits));

  const char* valueExact = reinterpret_cast<const char*>(value->data());
  size_t size = reinterpret_cast<size_t>(value->size());

  ::tdi::KeyFieldValueExact<const char*> exactKey(valueExact, size);

  RETURN_IF_TDI_ERROR(
      table_key_->getValue(static_cast<tdi_id_t>(id), &exactKey));
  *value = ByteStringToP4RuntimeByteString(*value);

  return ::util::OkStatus();
}

::util::Status TableKey::GetTernary(int id, std::string* value,
                                    std::string* mask) const {
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(table_key_->tableGet(&table));
  size_t field_size_bits;
  auto tableInfo = table->tableInfoGet();
  const ::tdi::KeyFieldInfo* keyFieldInfo =
      tableInfo->keyFieldGet(static_cast<tdi_id_t>(id));
  RET_CHECK(keyFieldInfo);

  field_size_bits = keyFieldInfo->sizeGet();
  value->clear();
  value->resize(NumBitsToNumBytes(field_size_bits));
  mask->clear();
  mask->resize(NumBitsToNumBytes(field_size_bits));
  DCHECK_EQ(value->size(), mask->size());

  const char* valueTernary = reinterpret_cast<const char*>(value->data());
  const char* maskTernary = reinterpret_cast<const char*>(mask->data());
  size_t sizeTernary = reinterpret_cast<size_t>(value->size());

  ::tdi::KeyFieldValueTernary<const char*> ternaryKey(valueTernary, maskTernary,
                                                      sizeTernary);
  RETURN_IF_TDI_ERROR(
      table_key_->getValue(static_cast<tdi_id_t>(id), &ternaryKey));

  *value = ByteStringToP4RuntimeByteString(*value);
  *mask = ByteStringToP4RuntimeByteString(*mask);

  return ::util::OkStatus();
}

::util::Status TableKey::GetLpm(int id, std::string* prefix,
                                uint16* prefix_length) const {
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(table_key_->tableGet(&table));
  size_t field_size_bits;
  auto tableInfo = table->tableInfoGet();
  const ::tdi::KeyFieldInfo* keyFieldInfo =
      tableInfo->keyFieldGet(static_cast<tdi_id_t>(id));
  RET_CHECK(keyFieldInfo);

  field_size_bits = keyFieldInfo->sizeGet();
  prefix->clear();
  prefix->resize(NumBitsToNumBytes(field_size_bits));

  const char* valueLpm = reinterpret_cast<const char*>(prefix->data());
  size_t sizeLpm = reinterpret_cast<size_t>(prefix->size());
  ::tdi::KeyFieldValueLPM<const char*> lpmKey(valueLpm, *prefix_length,
                                              sizeLpm);

  RETURN_IF_TDI_ERROR(table_key_->getValue(static_cast<tdi_id_t>(id), &lpmKey));

  *prefix = ByteStringToP4RuntimeByteString(*prefix);

  return ::util::OkStatus();
}

::util::Status TableKey::GetRange(int id, std::string* low,
                                  std::string* high) const {
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(table_key_->tableGet(&table));
  size_t field_size_bits;
  auto tableInfo = table->tableInfoGet();
  const ::tdi::KeyFieldInfo* keyFieldInfo =
      tableInfo->keyFieldGet(static_cast<tdi_id_t>(id));
  RET_CHECK(keyFieldInfo);

  field_size_bits = keyFieldInfo->sizeGet();
  low->clear();
  low->resize(NumBitsToNumBytes(field_size_bits));
  high->clear();
  high->resize(NumBitsToNumBytes(field_size_bits));

  const char* lowRange = reinterpret_cast<const char*>(low->data());
  const char* highRange = reinterpret_cast<const char*>(high->data());
  size_t sizeRange = reinterpret_cast<size_t>(low->size());
  ::tdi::KeyFieldValueRange<const char*> rangeKey(lowRange, highRange,
                                                  sizeRange);

  RETURN_IF_TDI_ERROR(
      table_key_->getValue(static_cast<tdi_id_t>(id), &rangeKey));
  *low = ByteStringToP4RuntimeByteString(*low);
  *high = ByteStringToP4RuntimeByteString(*high);

  return ::util::OkStatus();
}

::util::Status TableKey::GetPriority(uint32* priority) const {
  uint32 value = 0;
  GetFieldExact(*(table_key_.get()), kMatchPriority, &value);
  *priority = value;
  return ::util::OkStatus();
}

::util::StatusOr<std::unique_ptr<TdiSdeInterface::TableKeyInterface>>
TableKey::CreateTableKey(const ::tdi::TdiInfo* tdi_info_, int table_id) {
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(table_id, &table));
  std::unique_ptr<::tdi::TableKey> table_key;
  RETURN_IF_TDI_ERROR(table->keyAllocate(&table_key));
  auto key = std::unique_ptr<TdiSdeInterface::TableKeyInterface>(
      new TableKey(std::move(table_key)));
  return key;
}

::util::Status TableData::SetParam(int id, const std::string& value) {
  tdi_id_t action_id = 0;
  size_t field_size_bits = 0;
  const ::tdi::DataFieldInfo* dataFieldInfo;
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(table_data_->getParent(&table));

  action_id = table_data_->actionIdGet();
  dataFieldInfo = table->tableInfoGet()->dataFieldGet(id, action_id);
  RET_CHECK(dataFieldInfo);
  field_size_bits = dataFieldInfo->sizeGet();

  std::string p = P4RuntimeByteStringToPaddedByteString(
      value, NumBitsToNumBytes(field_size_bits));
  RETURN_IF_TDI_ERROR(table_data_->setValue(
      id, reinterpret_cast<const uint8*>(p.data()), p.size()));

  return ::util::OkStatus();
}

::util::Status TableData::GetParam(int id, std::string* value) const {
  size_t field_size_bits;
  const ::tdi::DataFieldInfo* dataFieldInfo;
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(table_data_->getParent(&table));

  tdi_id_t action_id = table_data_->actionIdGet();
  dataFieldInfo = table->tableInfoGet()->dataFieldGet(id, action_id);
  RET_CHECK(dataFieldInfo);
  field_size_bits = dataFieldInfo->sizeGet();

  value->clear();
  value->resize(NumBitsToNumBytes(field_size_bits));
  RETURN_IF_TDI_ERROR(table_data_->getValue(
      id, value->size(),
      reinterpret_cast<uint8*>(gtl::string_as_array(value))));
  *value = ByteStringToP4RuntimeByteString(*value);
  return ::util::OkStatus();
}

::util::Status TableData::SetActionMemberId(uint64 action_member_id) {
  return SetField(table_data_.get(), kActionMemberId, action_member_id);
}

::util::Status TableData::GetActionMemberId(uint64* action_member_id) const {
  return GetField(*(table_data_.get()), kActionMemberId, action_member_id);
}

::util::Status TableData::SetSelectorGroupId(uint64 selector_group_id) {
  return SetField(table_data_.get(), kSelectorGroupId, selector_group_id);
}

::util::Status TableData::GetSelectorGroupId(uint64* selector_group_id) const {
  return GetField(*(table_data_.get()), kSelectorGroupId, selector_group_id);
}

// The P4Runtime `CounterData` message has no mechanism to differentiate between
// byte-only, packet-only or both counter types. This make it impossible to
// recognize a counter reset (set, e.g., bytes to zero) request from a set
// request for a packet-only counter. Therefore we have to be careful when
// making set calls for those fields against the SDE.

::util::Status TableData::SetCounterData(uint64 bytes, uint64 packets) {
  std::vector<tdi_id_t> data_field_ids;
  tdi_id_t field_id_bytes = 0;
  tdi_id_t field_id_packets = 0;
  const ::tdi::DataFieldInfo* dataFieldInfoPackets;
  const ::tdi::DataFieldInfo* dataFieldInfoBytes;
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(table_data_->getParent(&table));

  tdi_id_t action_id = table_data_->actionIdGet();
  dataFieldInfoPackets =
      table->tableInfoGet()->dataFieldGet(kCounterPackets, action_id);
  dataFieldInfoBytes =
      table->tableInfoGet()->dataFieldGet(kCounterBytes, action_id);
  RET_CHECK(dataFieldInfoPackets);
  RET_CHECK(dataFieldInfoBytes);
  field_id_packets = dataFieldInfoPackets->idGet();
  field_id_bytes = dataFieldInfoBytes->idGet();

  RETURN_IF_TDI_ERROR(table_data_->setValue(field_id_bytes, bytes));
  RETURN_IF_TDI_ERROR(table_data_->setValue(field_id_packets, packets));

  return ::util::OkStatus();
}

::util::Status TableData::GetCounterData(uint64* bytes, uint64* packets) const {
  RET_CHECK(bytes);
  RET_CHECK(packets);
  tdi_id_t field_id_bytes = 0;
  tdi_id_t field_id_packets = 0;
  const ::tdi::DataFieldInfo* dataFieldInfoPackets;
  const ::tdi::DataFieldInfo* dataFieldInfoBytes;
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(table_data_->getParent(&table));

  tdi_id_t action_id = table_data_->actionIdGet();
  dataFieldInfoPackets =
      table->tableInfoGet()->dataFieldGet(kCounterPackets, action_id);
  dataFieldInfoBytes =
      table->tableInfoGet()->dataFieldGet(kCounterBytes, action_id);
  RET_CHECK(dataFieldInfoPackets);
  RET_CHECK(dataFieldInfoBytes);
  field_id_packets = dataFieldInfoPackets->idGet();
  field_id_bytes = dataFieldInfoBytes->idGet();

  // Clear values in case we set only one of them later.
  *bytes = 0;
  *packets = 0;

  RETURN_IF_TDI_ERROR(table_data_->getValue(field_id_bytes, bytes));
  RETURN_IF_TDI_ERROR(table_data_->getValue(field_id_packets, packets));

  return ::util::OkStatus();
}

::util::Status TableData::GetActionId(int* action_id) const {
  RET_CHECK(action_id);
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(table_data_->getParent(&table));
  *action_id = table_data_->actionIdGet();

  return ::util::OkStatus();
}

::util::Status TableData::Reset(int action_id) {
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(table_data_->getParent(&table));
  if (action_id) {
    RETURN_IF_TDI_ERROR(table->dataReset(action_id, table_data_.get()));
  } else {
    RETURN_IF_TDI_ERROR(table->dataReset(table_data_.get()));
  }

  return ::util::OkStatus();
}

::util::StatusOr<std::unique_ptr<TdiSdeInterface::TableDataInterface>>
TableData::CreateTableData(const ::tdi::TdiInfo* tdi_info_, int table_id,
                           int action_id) {
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(table_id, &table));
  std::unique_ptr<::tdi::TableData> table_data;
  if (action_id) {
    RETURN_IF_TDI_ERROR(table->dataAllocate(action_id, &table_data));
  } else {
    RETURN_IF_TDI_ERROR(table->dataAllocate(&table_data));
  }
  auto data = std::unique_ptr<TdiSdeInterface::TableDataInterface>(
      new TableData(std::move(table_data)));
  return data;
}

TdiSdeWrapper* TdiSdeWrapper::singleton_ = nullptr;
ABSL_CONST_INIT absl::Mutex TdiSdeWrapper::init_lock_(absl::kConstInit);

TdiSdeWrapper::TdiSdeWrapper() : port_status_event_writer_(nullptr) {}

::util::Status TdiSdeWrapper::OnPortStatusEvent(int device, int port, bool up,
                                                absl::Time timestamp) {
  // Create PortStatusEvent message.
  PortState state = up ? PORT_STATE_UP : PORT_STATE_DOWN;
  PortStatusEvent event = {device, port, state, timestamp};

  {
    absl::ReaderMutexLock l(&port_status_event_writer_lock_);
    if (!port_status_event_writer_) {
      return ::util::OkStatus();
    }
    return port_status_event_writer_->Write(event, kWriteTimeout);
  }
}

::util::Status TdiSdeWrapper::UnregisterPortStatusEventWriter() {
  absl::WriterMutexLock l(&port_status_event_writer_lock_);
  port_status_event_writer_ = nullptr;
  return ::util::OkStatus();
}

// Create and start an new session.
::util::StatusOr<std::shared_ptr<TdiSdeInterface::SessionInterface>>
TdiSdeWrapper::CreateSession() {
  return Session::CreateSession();
}

::util::StatusOr<std::unique_ptr<TdiSdeInterface::TableKeyInterface>>
TdiSdeWrapper::CreateTableKey(int table_id) {
  ::absl::ReaderMutexLock l(&data_lock_);
  return TableKey::CreateTableKey(tdi_info_, table_id);
}

::util::StatusOr<std::unique_ptr<TdiSdeInterface::TableDataInterface>>
TdiSdeWrapper::CreateTableData(int table_id, int action_id) {
  ::absl::ReaderMutexLock l(&data_lock_);
  return TableData::CreateTableData(tdi_info_, table_id, action_id);
}

::util::Status TdiSdeWrapper::RegisterPacketReceiveWriter(
    int device, std::unique_ptr<ChannelWriter<std::string>> writer) {
  absl::WriterMutexLock l(&packet_rx_callback_lock_);
  device_to_packet_rx_writer_[device] = std::move(writer);
  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::UnregisterPacketReceiveWriter(int device) {
  absl::WriterMutexLock l(&packet_rx_callback_lock_);
  device_to_packet_rx_writer_.erase(device);
  return ::util::OkStatus();
}

// PRE
namespace {

::util::Status PrintMcGroupEntry(const ::tdi::Table* table,
                                 const ::tdi::TableKey* table_key,
                                 const ::tdi::TableData* table_data) {
  std::vector<uint32> mc_node_list;
  std::vector<bool> l1_xid_valid_list;
  std::vector<uint32> l1_xid_list;
  uint32 multicast_group_id;

  // Key: $MGID
  RETURN_IF_ERROR(GetFieldExact(*table_key, kMgid, &multicast_group_id));
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

::util::Status PrintMcNodeEntry(const ::tdi::Table* table,
                                const ::tdi::TableKey* table_key,
                                const ::tdi::TableData* table_data) {
  // Key: $MULTICAST_NODE_ID (24 bit)
  uint32 node_id;
  RETURN_IF_ERROR(GetFieldExact(*table_key, kMcNodeId, &node_id));
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

::util::Status TdiSdeWrapper::DumpPreState(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session) {
  if (VLOG_IS_ON(2)) {
    auto real_session = std::dynamic_pointer_cast<Session>(session);
    RET_CHECK(real_session);

    const ::tdi::Table* table;
    const ::tdi::Device* device = nullptr;
    ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
    std::unique_ptr<::tdi::Target> dev_tgt;
    device->createTarget(&dev_tgt);

    // Dump group table
    LOG(INFO) << "#### $pre.mgid ####";
    RETURN_IF_TDI_ERROR(tdi_info_->tableFromNameGet(kPreMgidTable, &table));
    std::vector<std::unique_ptr<::tdi::TableKey>> keys;
    std::vector<std::unique_ptr<::tdi::TableData>> datums;
    RETURN_IF_TDI_ERROR(tdi_info_->tableFromNameGet(kPreMgidTable, &table));
    RETURN_IF_ERROR(GetAllEntries(real_session->tdi_session_, *dev_tgt, table,
                                  &keys, &datums));
    for (size_t i = 0; i < keys.size(); ++i) {
      const std::unique_ptr<::tdi::TableData>& table_data = datums[i];
      const std::unique_ptr<::tdi::TableKey>& table_key = keys[i];
      PrintMcGroupEntry(table, table_key.get(), table_data.get());
    }
    LOG(INFO) << "###################";

    // Dump node table
    LOG(INFO) << "#### $pre.node ####";
    RETURN_IF_TDI_ERROR(tdi_info_->tableFromNameGet(kPreNodeTable, &table));
    RETURN_IF_ERROR(GetAllEntries(real_session->tdi_session_, *dev_tgt, table,
                                  &keys, &datums));
    for (size_t i = 0; i < keys.size(); ++i) {
      const std::unique_ptr<::tdi::TableData>& table_data = datums[i];
      const std::unique_ptr<::tdi::TableKey>& table_key = keys[i];
      PrintMcNodeEntry(table, table_key.get(), table_data.get());
    }
    LOG(INFO) << "###################";
  }
  return ::util::OkStatus();
}

::util::StatusOr<uint32> TdiSdeWrapper::GetFreeMulticastNodeId(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session) {
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);

  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);

  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags* flags = new ::tdi::Flags(0);
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromNameGet(kPreNodeTable, &table));
  size_t table_size;

  RETURN_IF_TDI_ERROR(table->sizeGet(*real_session->tdi_session_, *dev_tgt,
                                     *flags, &table_size));
  uint32 usage;
  RETURN_IF_TDI_ERROR(
      table->usageGet(*real_session->tdi_session_, *dev_tgt, *flags, &usage));
  std::unique_ptr<::tdi::TableKey> table_key;
  std::unique_ptr<::tdi::TableData> table_data;
  RETURN_IF_TDI_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_TDI_ERROR(table->dataAllocate(&table_data));
  uint32 id = usage;
  for (size_t _ = 0; _ < table_size; ++_) {
    // Key: $MULTICAST_NODE_ID
    RETURN_IF_ERROR(SetFieldExact(table_key.get(), kMcNodeId, id));
    bf_status_t status;
    status = table->entryGet(*real_session->tdi_session_, *dev_tgt, *flags,
                             *table_key, table_data.get());
    if (status == BF_OBJECT_NOT_FOUND) {
      return id;
    } else if (status == BF_SUCCESS) {
      id++;
      continue;
    } else {
      RETURN_IF_TDI_ERROR(status);
    }
  }

  return MAKE_ERROR(ERR_TABLE_FULL) << "Could not find free multicast node id.";
}

::util::StatusOr<uint32> TdiSdeWrapper::CreateMulticastNode(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    int mc_replication_id, const std::vector<uint32>& mc_lag_ids,
    const std::vector<uint32>& ports) {
  ::absl::ReaderMutexLock l(&data_lock_);

  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);

  const ::tdi::Table* table;  // PRE node table.
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromNameGet(kPreNodeTable, &table));
  auto table_id = table->tableInfoGet()->idGet();

  std::unique_ptr<::tdi::TableKey> table_key;
  std::unique_ptr<::tdi::TableData> table_data;
  RETURN_IF_TDI_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_TDI_ERROR(table->dataAllocate(&table_data));

  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags* flags = new ::tdi::Flags(0);

  ASSIGN_OR_RETURN(uint64 mc_node_id, GetFreeMulticastNodeId(dev_id, session));

  // Key: $MULTICAST_NODE_ID
  RETURN_IF_ERROR(SetFieldExact(table_key.get(), kMcNodeId, mc_node_id));
  // Data: $MULTICAST_RID (16 bit)
  RETURN_IF_ERROR(
      SetField(table_data.get(), kMcReplicationId, mc_replication_id));
  // Data: $MULTICAST_LAG_ID
  RETURN_IF_ERROR(SetField(table_data.get(), kMcNodeLagId, mc_lag_ids));
  // Data: $DEV_PORT
  RETURN_IF_ERROR(SetField(table_data.get(), kMcNodeDevPort, ports));
  RETURN_IF_TDI_ERROR(table->entryAdd(*real_session->tdi_session_, *dev_tgt,
                                      *flags, *table_key, *table_data));

  return mc_node_id;
}

::util::StatusOr<std::vector<uint32>> TdiSdeWrapper::GetNodesInMulticastGroup(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 group_id) {
  ::absl::ReaderMutexLock l(&data_lock_);

  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);

  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags* flags = new ::tdi::Flags(0);
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromNameGet(kPreMgidTable, &table));

  std::unique_ptr<::tdi::TableKey> table_key;
  std::unique_ptr<::tdi::TableData> table_data;
  RETURN_IF_TDI_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_TDI_ERROR(table->dataAllocate(&table_data));
  // Key: $MGID
  RETURN_IF_ERROR(SetFieldExact(table_key.get(), kMgid, group_id));
  RETURN_IF_TDI_ERROR(table->entryGet(*real_session->tdi_session_, *dev_tgt,
                                      *flags, *table_key, table_data.get()));
  // Data: $MULTICAST_NODE_ID
  std::vector<uint32> mc_node_list;
  RETURN_IF_ERROR(GetField(*table_data, kMcNodeId, &mc_node_list));

  return mc_node_list;
}

::util::Status TdiSdeWrapper::DeleteMulticastNodes(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    const std::vector<uint32>& mc_node_ids) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);

  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags* flags = new ::tdi::Flags(0);
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromNameGet(kPreNodeTable, &table));
  auto table_id = table->tableInfoGet()->idGet();

  // TODO(max): handle partial delete failures
  for (const auto& mc_node_id : mc_node_ids) {
    std::unique_ptr<::tdi::TableKey> table_key;
    RETURN_IF_TDI_ERROR(table->keyAllocate(&table_key));
    RETURN_IF_ERROR(SetFieldExact(table_key.get(), kMcNodeId, mc_node_id));
    RETURN_IF_TDI_ERROR(table->entryDel(*real_session->tdi_session_, *dev_tgt,
                                        *flags, *table_key));
  }

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::GetMulticastNode(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 mc_node_id, int* replication_id, std::vector<uint32>* lag_ids,
    std::vector<uint32>* ports) {
  RET_CHECK(replication_id);
  RET_CHECK(lag_ids);
  RET_CHECK(ports);
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);

  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags* flags = new ::tdi::Flags(0);
  const ::tdi::Table* table;  // PRE node table.
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromNameGet(kPreNodeTable, &table));
  auto table_id = table->tableInfoGet()->idGet();

  std::unique_ptr<::tdi::TableKey> table_key;
  std::unique_ptr<::tdi::TableData> table_data;
  RETURN_IF_TDI_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_TDI_ERROR(table->dataAllocate(&table_data));
  // Key: $MULTICAST_NODE_ID
  RETURN_IF_ERROR(SetFieldExact(table_key.get(), kMcNodeId, mc_node_id));
  RETURN_IF_TDI_ERROR(table->entryGet(*real_session->tdi_session_, *dev_tgt,
                                      *flags, *table_key, table_data.get()));
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

::util::Status TdiSdeWrapper::WriteMulticastGroup(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 group_id, const std::vector<uint32>& mc_node_ids, bool insert) {
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);

  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags* flags = new ::tdi::Flags(0);
  const ::tdi::Table* table = nullptr;  // PRE MGID table.
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromNameGet(kPreMgidTable, &table));
  auto table_id = table->tableInfoGet()->idGet();
  std::unique_ptr<::tdi::TableKey> table_key;
  std::unique_ptr<::tdi::TableData> table_data;
  RETURN_IF_TDI_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_TDI_ERROR(table->dataAllocate(&table_data));

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
  RETURN_IF_ERROR(SetFieldExact(table_key.get(), kMgid, group_id));
  // Data: $MULTICAST_NODE_ID
  RETURN_IF_ERROR(SetField(table_data.get(), kMcNodeId, mc_node_list));
  // Data: $MULTICAST_NODE_L1_XID_VALID
  RETURN_IF_ERROR(
      SetField(table_data.get(), kMcNodeL1XidValid, l1_xid_valid_list));
  // Data: $MULTICAST_NODE_L1_XID
  RETURN_IF_ERROR(SetField(table_data.get(), kMcNodeL1Xid, l1_xid_list));

  if (insert) {
    RETURN_IF_TDI_ERROR(table->entryAdd(*real_session->tdi_session_, *dev_tgt,
                                        *flags, *table_key, *table_data));

  } else {
    RETURN_IF_TDI_ERROR(table->entryMod(*real_session->tdi_session_, *dev_tgt,
                                        *flags, *table_key, *table_data));
  }

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::InsertMulticastGroup(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 group_id, const std::vector<uint32>& mc_node_ids) {
  ::absl::ReaderMutexLock l(&data_lock_);
  return WriteMulticastGroup(dev_id, session, group_id, mc_node_ids, true);
}

::util::Status TdiSdeWrapper::ModifyMulticastGroup(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 group_id, const std::vector<uint32>& mc_node_ids) {
  ::absl::ReaderMutexLock l(&data_lock_);
  return WriteMulticastGroup(dev_id, session, group_id, mc_node_ids, false);
}

::util::Status TdiSdeWrapper::DeleteMulticastGroup(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 group_id) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);

  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags* flags = new ::tdi::Flags(0);
  const ::tdi::Table* table;  // PRE MGID table.
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromNameGet(kPreMgidTable, &table));
  std::unique_ptr<::tdi::TableKey> table_key;
  RETURN_IF_TDI_ERROR(table->keyAllocate(&table_key));
  // Key: $MGID
  RETURN_IF_ERROR(SetFieldExact(table_key.get(), kMgid, group_id));
  RETURN_IF_TDI_ERROR(table->entryDel(*real_session->tdi_session_, *dev_tgt,
                                      *flags, *table_key));

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::GetMulticastGroups(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 group_id, std::vector<uint32>* group_ids,
    std::vector<std::vector<uint32>>* mc_node_ids) {
  RET_CHECK(group_ids);
  RET_CHECK(mc_node_ids);
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);

  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags* flags = new ::tdi::Flags(0);
  const ::tdi::Table* table;  // PRE MGID table.
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromNameGet(kPreMgidTable, &table));
  std::vector<std::unique_ptr<::tdi::TableKey>> keys;
  std::vector<std::unique_ptr<::tdi::TableData>> datums;
  // Is this a wildcard read?
  if (group_id != 0) {
    keys.resize(1);
    datums.resize(1);
    RETURN_IF_TDI_ERROR(table->keyAllocate(&keys[0]));
    RETURN_IF_TDI_ERROR(table->dataAllocate(&datums[0]));
    // Key: $MGID
    RETURN_IF_ERROR(SetFieldExact(keys[0].get(), kMgid, group_id));
    RETURN_IF_TDI_ERROR(table->entryGet(*real_session->tdi_session_, *dev_tgt,
                                        *flags, *keys[0], datums[0].get()));
  } else {
    RETURN_IF_ERROR(GetAllEntries(real_session->tdi_session_, *dev_tgt, table,
                                  &keys, &datums));
  }

  group_ids->resize(0);
  mc_node_ids->resize(0);
  for (size_t i = 0; i < keys.size(); ++i) {
    const std::unique_ptr<::tdi::TableData>& table_data = datums[i];
    const std::unique_ptr<::tdi::TableKey>& table_key = keys[i];
    ::p4::v1::MulticastGroupEntry result;
    // Key: $MGID
    uint32 group_id = 0;
    RETURN_IF_ERROR(GetFieldExact(*table_key, kMgid, &group_id));
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

::util::Status TdiSdeWrapper::WriteCloneSession(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 session_id, int egress_port, int cos, int max_pkt_len, bool insert) {
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);

  const ::tdi::Table* table;
  const ::tdi::Device* device = nullptr;
  const ::tdi::DataFieldInfo* dataFieldInfo;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags* flags = new ::tdi::Flags(0);
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromNameGet(kMirrorConfigTable, &table));
  std::unique_ptr<::tdi::TableKey> table_key;
  std::unique_ptr<::tdi::TableData> table_data;
  RETURN_IF_TDI_ERROR(table->keyAllocate(&table_key));
  tdi_id_t action_id;
  dataFieldInfo = table->tableInfoGet()->dataFieldGet("$normal");
  RET_CHECK(dataFieldInfo);
  action_id = dataFieldInfo->idGet();
  RETURN_IF_TDI_ERROR(table->dataAllocate(action_id, &table_data));

  // Key: $sid
  RETURN_IF_ERROR(SetFieldExact(table_key.get(), "$sid", session_id));
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

  if (insert) {
    RETURN_IF_TDI_ERROR(table->entryAdd(*real_session->tdi_session_, *dev_tgt,
                                        *flags, *table_key, *table_data));
  } else {
    RETURN_IF_TDI_ERROR(table->entryMod(*real_session->tdi_session_, *dev_tgt,
                                        *flags, *table_key, *table_data));
  }

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::InsertCloneSession(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 session_id, int egress_port, int cos, int max_pkt_len) {
  ::absl::ReaderMutexLock l(&data_lock_);
  return WriteCloneSession(dev_id, session, session_id, egress_port, cos,
                           max_pkt_len, true);
}

::util::Status TdiSdeWrapper::ModifyCloneSession(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 session_id, int egress_port, int cos, int max_pkt_len) {
  ::absl::ReaderMutexLock l(&data_lock_);
  return WriteCloneSession(dev_id, session, session_id, egress_port, cos,
                           max_pkt_len, false);
}

::util::Status TdiSdeWrapper::DeleteCloneSession(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 session_id) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  const ::tdi::DataFieldInfo* dataFieldInfo;
  RET_CHECK(real_session);

  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromNameGet(kMirrorConfigTable, &table));
  std::unique_ptr<::tdi::TableKey> table_key;
  std::unique_ptr<::tdi::TableData> table_data;
  RETURN_IF_TDI_ERROR(table->keyAllocate(&table_key));
  tdi_id_t action_id;
  dataFieldInfo = table->tableInfoGet()->dataFieldGet("$normal");
  RET_CHECK(dataFieldInfo);
  action_id = dataFieldInfo->idGet();
  RETURN_IF_TDI_ERROR(table->dataAllocate(action_id, &table_data));
  // Key: $sid
  RETURN_IF_ERROR(SetFieldExact(table_key.get(), "$sid", session_id));

  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags* flags = new ::tdi::Flags(0);
  RETURN_IF_TDI_ERROR(table->entryDel(*real_session->tdi_session_, *dev_tgt,
                                      *flags, *table_key));

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::GetCloneSessions(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 session_id, std::vector<uint32>* session_ids,
    std::vector<int>* egress_ports, std::vector<int>* coss,
    std::vector<int>* max_pkt_lens) {
  RET_CHECK(session_ids);
  RET_CHECK(egress_ports);
  RET_CHECK(coss);
  RET_CHECK(max_pkt_lens);
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  const ::tdi::DataFieldInfo* dataFieldInfo;
  RET_CHECK(real_session);

  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags* flags = new ::tdi::Flags(0);
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromNameGet(kMirrorConfigTable, &table));
  tdi_id_t action_id;
  dataFieldInfo = table->tableInfoGet()->dataFieldGet("$normal");
  RET_CHECK(dataFieldInfo);
  action_id = dataFieldInfo->idGet();
  std::vector<std::unique_ptr<::tdi::TableKey>> keys;
  std::vector<std::unique_ptr<::tdi::TableData>> datums;
  // Is this a wildcard read?
  if (session_id != 0) {
    keys.resize(1);
    datums.resize(1);
    RETURN_IF_TDI_ERROR(table->keyAllocate(&keys[0]));
    RETURN_IF_TDI_ERROR(table->dataAllocate(action_id, &datums[0]));
    // Key: $sid
    RETURN_IF_ERROR(SetFieldExact(keys[0].get(), "$sid", session_id));
    RETURN_IF_TDI_ERROR(table->entryGet(*real_session->tdi_session_, *dev_tgt,
                                        *flags, *keys[0], datums[0].get()));
  } else {
    RETURN_IF_ERROR(GetAllEntries(real_session->tdi_session_, *dev_tgt, table,
                                  &keys, &datums));
  }

  session_ids->resize(0);
  egress_ports->resize(0);
  coss->resize(0);
  max_pkt_lens->resize(0);
  for (size_t i = 0; i < keys.size(); ++i) {
    const std::unique_ptr<::tdi::TableData>& table_data = datums[i];
    const std::unique_ptr<::tdi::TableKey>& table_key = keys[i];
    // Key: $sid
    uint32 session_id = 0;
    RETURN_IF_ERROR(GetFieldExact(*table_key, "$sid", &session_id));
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
    RETURN_IF_ERROR(
        GetFieldBool(*table_data, "$session_enable", &session_enable));
    RET_CHECK(session_enable) << "Found a session that is not enabled.";
    // Data: $ucast_egress_port_valid
    bool ucast_egress_port_valid;
    RETURN_IF_ERROR(GetFieldBool(*table_data, "$ucast_egress_port_valid",
                                 &ucast_egress_port_valid));
    RET_CHECK(ucast_egress_port_valid)
        << "Found a unicast egress port that is not set valid.";
  }

  CHECK_EQ(session_ids->size(), keys.size());
  CHECK_EQ(egress_ports->size(), keys.size());
  CHECK_EQ(coss->size(), keys.size());
  CHECK_EQ(max_pkt_lens->size(), keys.size());

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::WriteIndirectCounter(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 counter_id, int counter_index, absl::optional<uint64> byte_count,
    absl::optional<uint64> packet_count) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  const ::tdi::DataFieldInfo* dataFieldInfo;
  RET_CHECK(real_session);

  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(counter_id, &table));

  std::unique_ptr<::tdi::TableKey> table_key;
  std::unique_ptr<::tdi::TableData> table_data;
  RETURN_IF_TDI_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_TDI_ERROR(table->dataAllocate(&table_data));

  // Counter key: $COUNTER_INDEX
  RETURN_IF_ERROR(SetFieldExact(table_key.get(), kCounterIndex, counter_index));

  // Counter data: $COUNTER_SPEC_BYTES
  if (byte_count.has_value()) {
    tdi_id_t field_id;
    dataFieldInfo = table->tableInfoGet()->dataFieldGet(kCounterBytes);
    RET_CHECK(dataFieldInfo);
    field_id = dataFieldInfo->idGet();
    RETURN_IF_TDI_ERROR(table_data->setValue(field_id, byte_count.value()));
  }
  // Counter data: $COUNTER_SPEC_PKTS
  if (packet_count.has_value()) {
    tdi_id_t field_id;
    dataFieldInfo = table->tableInfoGet()->dataFieldGet(kCounterPackets);
    RET_CHECK(dataFieldInfo);
    field_id = dataFieldInfo->idGet();
    RETURN_IF_TDI_ERROR(table_data->setValue(field_id, packet_count.value()));
  }
  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags* flags = new ::tdi::Flags(0);
  if (byte_count.value() == 0 && packet_count.value() == 0) {
    LOG(INFO) << "Resetting counters";
    RETURN_IF_TDI_ERROR(
        table->clear(*real_session->tdi_session_, *dev_tgt, *flags));
  } else {
    RETURN_IF_TDI_ERROR(table->entryMod(*real_session->tdi_session_, *dev_tgt,
                                        *flags, *table_key, *table_data));
  }

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::ReadIndirectCounter(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 counter_id, absl::optional<uint32> counter_index,
    std::vector<uint32>* counter_indices,
    std::vector<absl::optional<uint64>>* byte_counts,
    std::vector<absl::optional<uint64>>* packet_counts,
    absl::Duration timeout) {
  RET_CHECK(counter_indices);
  RET_CHECK(byte_counts);
  RET_CHECK(packet_counts);
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);

  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags* flags = new ::tdi::Flags(0);
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(counter_id, &table));
  std::vector<std::unique_ptr<::tdi::TableKey>> keys;
  std::vector<std::unique_ptr<::tdi::TableData>> datums;

  RETURN_IF_ERROR(DoSynchronizeCounters(dev_id, session, counter_id, timeout));

  // Is this a wildcard read?
  if (counter_index) {
    keys.resize(1);
    datums.resize(1);
    RETURN_IF_TDI_ERROR(table->keyAllocate(&keys[0]));
    RETURN_IF_TDI_ERROR(table->dataAllocate(&datums[0]));

    // Key: $COUNTER_INDEX
    RETURN_IF_ERROR(
        SetFieldExact(keys[0].get(), kCounterIndex, counter_index.value()));
    RETURN_IF_TDI_ERROR(table->entryGet(*real_session->tdi_session_, *dev_tgt,
                                        *flags, *keys[0], datums[0].get()));

  } else {
    RETURN_IF_ERROR(GetAllEntries(real_session->tdi_session_, *dev_tgt, table,
                                  &keys, &datums));
  }

  counter_indices->resize(0);
  byte_counts->resize(0);
  packet_counts->resize(0);
  for (size_t i = 0; i < keys.size(); ++i) {
    const std::unique_ptr<::tdi::TableData>& table_data = datums[i];
    const std::unique_ptr<::tdi::TableKey>& table_key = keys[i];
    // Key: $COUNTER_INDEX
    uint32 tdi_counter_index = 0;
    RETURN_IF_ERROR(
        GetFieldExact(*table_key, kCounterIndex, &tdi_counter_index));
    counter_indices->push_back(tdi_counter_index);

    absl::optional<uint64> byte_count;
    absl::optional<uint64> packet_count;
    // Counter data: $COUNTER_SPEC_BYTES
    tdi_id_t field_id;

    if (table->tableInfoGet()->dataFieldGet(kCounterBytes)) {
      field_id = table->tableInfoGet()->dataFieldGet(kCounterBytes)->idGet();
      uint64 counter_bytes;
      RETURN_IF_TDI_ERROR(table_data->getValue(field_id, &counter_bytes));
      byte_count = counter_bytes;
    }
    byte_counts->push_back(byte_count);

    // Counter data: $COUNTER_SPEC_PKTS
    if (table->tableInfoGet()->dataFieldGet(kCounterPackets)) {
      field_id = table->tableInfoGet()->dataFieldGet(kCounterPackets)->idGet();
      uint64 counter_pkts;
      RETURN_IF_TDI_ERROR(table_data->getValue(field_id, &counter_pkts));
      packet_count = counter_pkts;
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
// RETURN_IF_TDI_ERROR(table->tableNameGet(&table_name));
// RETURN_IF_TDI_ERROR(
//     table->dataFieldIdGet(absl::StrCat(table_name, ".", "f1"), &field_id));
::util::StatusOr<tdi_id_t> GetRegisterDataFieldId(const ::tdi::Table* table) {
  std::vector<tdi_id_t> data_field_ids;
  const ::tdi::DataFieldInfo* dataFieldInfo;
  data_field_ids = table->tableInfoGet()->dataFieldIdListGet();
  for (const auto& field_id : data_field_ids) {
    std::string field_name;
    dataFieldInfo = table->tableInfoGet()->dataFieldGet(field_id);
    RET_CHECK(dataFieldInfo);
    field_name = dataFieldInfo->nameGet();
    if (absl::EndsWith(field_name, ".f1")) {
      return field_id;
    }
  }

  return MAKE_ERROR(ERR_INTERNAL) << "Could not find register data field id.";
}
}  // namespace

::util::Status TdiSdeWrapper::WriteRegister(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, absl::optional<uint32> register_index,
    const std::string& register_data) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);

  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(table_id, &table));

  std::unique_ptr<::tdi::TableKey> table_key;
  std::unique_ptr<::tdi::TableData> table_data;
  RETURN_IF_TDI_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_TDI_ERROR(table->dataAllocate(&table_data));

  // Register data: <register_name>.f1
  // The current bf-p4c compiler emits the fully-qualified field name, including
  // parent table and pipeline. We cannot use just "f1" as the field name.
  tdi_id_t field_id;
  ASSIGN_OR_RETURN(field_id, GetRegisterDataFieldId(table));
  size_t data_field_size_bits;
  const ::tdi::DataFieldInfo* dataFieldInfo;
  dataFieldInfo = table->tableInfoGet()->dataFieldGet(field_id);
  RET_CHECK(dataFieldInfo);
  data_field_size_bits = dataFieldInfo->sizeGet();
  // The SDE expects a string with the full width.
  std::string value = P4RuntimeByteStringToPaddedByteString(
      register_data, data_field_size_bits);
  RETURN_IF_TDI_ERROR(table_data->setValue(
      field_id, reinterpret_cast<const uint8*>(value.data()), value.size()));

  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags* flags = new ::tdi::Flags(0);
  if (register_index) {
    // Single index target.
    // Register key: $REGISTER_INDEX
    RETURN_IF_ERROR(
        SetFieldExact(table_key.get(), kRegisterIndex, register_index.value()));
    RETURN_IF_TDI_ERROR(table->entryMod(*real_session->tdi_session_, *dev_tgt,
                                        *flags, *table_key, *table_data));
  } else {
    // Wildcard write to all indices.
    size_t table_size;
    RETURN_IF_TDI_ERROR(table->sizeGet(*real_session->tdi_session_, *dev_tgt,
                                       *flags, &table_size));
    for (size_t i = 0; i < table_size; ++i) {
      // Register key: $REGISTER_INDEX
      RETURN_IF_ERROR(SetFieldExact(table_key.get(), kRegisterIndex, i));
      RETURN_IF_TDI_ERROR(table->entryMod(*real_session->tdi_session_, *dev_tgt,
                                          *flags, *table_key, *table_data));
    }
  }

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::ReadRegisters(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, absl::optional<uint32> register_index,
    std::vector<uint32>* register_indices, std::vector<uint64>* register_values,
    absl::Duration timeout) {
  RET_CHECK(register_indices);
  RET_CHECK(register_values);
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);

  RETURN_IF_ERROR(SynchronizeRegisters(dev_id, session, table_id, timeout));

  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags* flags = new ::tdi::Flags(0);
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(table_id, &table));
  std::vector<std::unique_ptr<::tdi::TableKey>> keys;
  std::vector<std::unique_ptr<::tdi::TableData>> datums;

  // Is this a wildcard read?
  if (register_index) {
    keys.resize(1);
    datums.resize(1);
    RETURN_IF_TDI_ERROR(table->keyAllocate(&keys[0]));
    RETURN_IF_TDI_ERROR(table->dataAllocate(&datums[0]));

    // Key: $REGISTER_INDEX
    RETURN_IF_ERROR(
        SetFieldExact(keys[0].get(), kRegisterIndex, register_index.value()));
    RETURN_IF_TDI_ERROR(table->entryGet(*real_session->tdi_session_, *dev_tgt,
                                        *flags, *keys[0], datums[0].get()));
  } else {
    RETURN_IF_ERROR(GetAllEntries(real_session->tdi_session_, *dev_tgt, table,
                                  &keys, &datums));
  }

  register_indices->resize(0);
  register_values->resize(0);
  for (size_t i = 0; i < keys.size(); ++i) {
    const std::unique_ptr<::tdi::TableData>& table_data = datums[i];
    const std::unique_ptr<::tdi::TableKey>& table_key = keys[i];
    // Key: $REGISTER_INDEX
    uint32 tdi_register_index = 0;
    RETURN_IF_ERROR(
        GetFieldExact(*table_key, kRegisterIndex, &tdi_register_index));
    register_indices->push_back(tdi_register_index);
    // Data: <register_name>.f1
    ASSIGN_OR_RETURN(auto f1_field_id, GetRegisterDataFieldId(table));

    tdi_field_data_type_e data_type;
    const ::tdi::DataFieldInfo* dataFieldInfo;
    dataFieldInfo = table->tableInfoGet()->dataFieldGet(f1_field_id);
    RET_CHECK(dataFieldInfo);
    data_type = dataFieldInfo->dataTypeGet();
    switch (data_type) {
      case TDI_FIELD_DATA_TYPE_BYTE_STREAM: {
        // Even though the data type says byte stream, the SDE can only allows
        // fetching the data in an uint64 vector with one entry per pipe.
        std::vector<uint64> register_data;
        RETURN_IF_TDI_ERROR(table_data->getValue(f1_field_id, &register_data));
        RET_CHECK(register_data.size() > 0);
        register_values->push_back(register_data[0]);
        break;
      }
      default:
        return MAKE_ERROR(ERR_INVALID_PARAM)
               << "Unsupported register data type "
               << static_cast<int>(data_type) << " for register in table "
               << table_id;
    }
  }

  CHECK_EQ(register_indices->size(), keys.size());
  CHECK_EQ(register_values->size(), keys.size());

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::WriteIndirectMeter(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, absl::optional<uint32> meter_index, bool in_pps,
    uint64 cir, uint64 cburst, uint64 pir, uint64 pburst) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);

  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(table_id, &table));

  std::unique_ptr<::tdi::TableKey> table_key;
  std::unique_ptr<::tdi::TableData> table_data;
  RETURN_IF_TDI_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_TDI_ERROR(table->dataAllocate(&table_data));

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

  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags* flags = new ::tdi::Flags(0);
  if (meter_index) {
    // Single index target.
    // Meter key: $METER_INDEX
    RETURN_IF_ERROR(
        SetFieldExact(table_key.get(), kMeterIndex, meter_index.value()));
    RETURN_IF_TDI_ERROR(table->entryMod(*real_session->tdi_session_, *dev_tgt,
                                        *flags, *table_key, *table_data));
  } else {
    // Wildcard write to all indices.
    size_t table_size;
    RETURN_IF_TDI_ERROR(table->sizeGet(*real_session->tdi_session_, *dev_tgt,
                                       *flags, &table_size));
    for (size_t i = 0; i < table_size; ++i) {
      // Meter key: $METER_INDEX
      RETURN_IF_ERROR(SetFieldExact(table_key.get(), kMeterIndex, i));
      RETURN_IF_TDI_ERROR(table->entryMod(*real_session->tdi_session_, *dev_tgt,
                                          *flags, *table_key, *table_data));
    }
  }

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::ReadIndirectMeters(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, absl::optional<uint32> meter_index,
    std::vector<uint32>* meter_indices, std::vector<uint64>* cirs,
    std::vector<uint64>* cbursts, std::vector<uint64>* pirs,
    std::vector<uint64>* pbursts, std::vector<bool>* in_pps) {
  RET_CHECK(meter_indices);
  RET_CHECK(cirs);
  RET_CHECK(cbursts);
  RET_CHECK(pirs);
  RET_CHECK(pbursts);
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);

  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags* flags = new ::tdi::Flags(0);
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(table_id, &table));
  std::vector<std::unique_ptr<::tdi::TableKey>> keys;
  std::vector<std::unique_ptr<::tdi::TableData>> datums;

  // Is this a wildcard read?
  if (meter_index) {
    keys.resize(1);
    datums.resize(1);
    RETURN_IF_TDI_ERROR(table->keyAllocate(&keys[0]));
    RETURN_IF_TDI_ERROR(table->dataAllocate(&datums[0]));
    // Key: $METER_INDEX
    RETURN_IF_ERROR(
        SetFieldExact(keys[0].get(), kMeterIndex, meter_index.value()));
    RETURN_IF_TDI_ERROR(table->entryGet(*real_session->tdi_session_, *dev_tgt,
                                        *flags, *keys[0], datums[0].get()));
  } else {
    RETURN_IF_ERROR(GetAllEntries(real_session->tdi_session_, *dev_tgt, table,
                                  &keys, &datums));
  }

  meter_indices->resize(0);
  cirs->resize(0);
  cbursts->resize(0);
  pirs->resize(0);
  pbursts->resize(0);
  in_pps->resize(0);
  for (size_t i = 0; i < keys.size(); ++i) {
    const std::unique_ptr<::tdi::TableData>& table_data = datums[i];
    const std::unique_ptr<::tdi::TableKey>& table_key = keys[i];
    // Key: $METER_INDEX
    uint32 tdi_meter_index = 0;
    RETURN_IF_ERROR(GetFieldExact(*table_key, kMeterIndex, &tdi_meter_index));
    meter_indices->push_back(tdi_meter_index);

    // Data: $METER_SPEC_*
    std::vector<tdi_id_t> data_field_ids;
    data_field_ids = table->tableInfoGet()->dataFieldIdListGet();
    for (const auto& field_id : data_field_ids) {
      std::string field_name;
      const ::tdi::DataFieldInfo* dataFieldInfo;
      dataFieldInfo = table->tableInfoGet()->dataFieldGet(field_id);
      RET_CHECK(dataFieldInfo);
      field_name = dataFieldInfo->nameGet();
      if (field_name == kMeterCirKbps) {  // kbits
        uint64 cir;
        RETURN_IF_TDI_ERROR(table_data->getValue(field_id, &cir));
        cirs->push_back(KbitsToBytesPerSecond(cir));
        in_pps->push_back(false);
      } else if (field_name == kMeterCommitedBurstKbits) {
        uint64 cburst;
        RETURN_IF_TDI_ERROR(table_data->getValue(field_id, &cburst));
        cbursts->push_back(KbitsToBytesPerSecond(cburst));
      } else if (field_name == kMeterPirKbps) {
        uint64 pir;
        RETURN_IF_TDI_ERROR(table_data->getValue(field_id, &pir));
        pirs->push_back(KbitsToBytesPerSecond(pir));
      } else if (field_name == kMeterPeakBurstKbits) {
        uint64 pburst;
        RETURN_IF_TDI_ERROR(table_data->getValue(field_id, &pburst));
        pbursts->push_back(KbitsToBytesPerSecond(pburst));
      } else if (field_name == kMeterCirPps) {  // Packets
        uint64 cir;
        RETURN_IF_TDI_ERROR(table_data->getValue(field_id, &cir));
        cirs->push_back(cir);
        in_pps->push_back(true);
      } else if (field_name == kMeterCommitedBurstPackets) {
        uint64 cburst;
        RETURN_IF_TDI_ERROR(table_data->getValue(field_id, &cburst));
        cbursts->push_back(cburst);
      } else if (field_name == kMeterPirPps) {
        uint64 pir;
        RETURN_IF_TDI_ERROR(table_data->getValue(field_id, &pir));
        pirs->push_back(pir);
      } else if (field_name == kMeterPeakBurstPackets) {
        uint64 pburst;
        RETURN_IF_TDI_ERROR(table_data->getValue(field_id, &pburst));
        pbursts->push_back(pburst);
      } else {
        return MAKE_ERROR(ERR_INVALID_PARAM)
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

::util::Status TdiSdeWrapper::WriteActionProfileMember(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, int member_id, const TableDataInterface* table_data,
    bool insert) {
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);
  auto real_table_data = dynamic_cast<const TableData*>(table_data);
  RET_CHECK(real_table_data);

  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(table_id, &table));

  std::unique_ptr<::tdi::TableKey> table_key;
  RETURN_IF_TDI_ERROR(table->keyAllocate(&table_key));

  // DumpTableMetadata(table);
  // DumpTableData(real_table_data->table_data_.get());
  auto dump_args = [&]() -> std::string {
    return absl::StrCat(
        DumpTableMetadata(table).ValueOr("<error reading table>"),
        ", member_id: ", member_id, ", ",
        DumpTableKey(table_key.get()).ValueOr("<error parsing key>"), ", ",
        DumpTableData(real_table_data->table_data_.get())
            .ValueOr("<error parsing data>"));
  };

  // Key: $ACTION_MEMBER_ID
  RETURN_IF_ERROR(SetFieldExact(table_key.get(), kActionMemberId, member_id));

  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags* flags = new ::tdi::Flags(0);
  if (insert) {
    RETURN_IF_TDI_ERROR(table->entryAdd(*real_session->tdi_session_, *dev_tgt,
                                        *flags, *table_key,
                                        *real_table_data->table_data_))
        << "Could not add action profile member with: " << dump_args();
  } else {
    RETURN_IF_TDI_ERROR(table->entryMod(*real_session->tdi_session_, *dev_tgt,
                                        *flags, *table_key,
                                        *real_table_data->table_data_))
        << "Could not modify action profile member with: " << dump_args();
  }

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::InsertActionProfileMember(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, int member_id, const TableDataInterface* table_data) {
  ::absl::ReaderMutexLock l(&data_lock_);
  return WriteActionProfileMember(dev_id, session, table_id, member_id,
                                  table_data, true);
}

::util::Status TdiSdeWrapper::ModifyActionProfileMember(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, int member_id, const TableDataInterface* table_data) {
  ::absl::ReaderMutexLock l(&data_lock_);
  return WriteActionProfileMember(dev_id, session, table_id, member_id,
                                  table_data, false);
}

::util::Status TdiSdeWrapper::DeleteActionProfileMember(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, int member_id) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);

  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(table_id, &table));

  std::unique_ptr<::tdi::TableKey> table_key;
  RETURN_IF_TDI_ERROR(table->keyAllocate(&table_key));

  auto dump_args = [&]() -> std::string {
    return absl::StrCat(
        DumpTableMetadata(table).ValueOr("<error reading table>"),
        ", member_id: ", member_id, ", ",
        DumpTableKey(table_key.get()).ValueOr("<error parsing key>"));
  };

  // Key: $ACTION_MEMBER_ID
  RETURN_IF_ERROR(SetFieldExact(table_key.get(), kActionMemberId, member_id));

  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags* flags = new ::tdi::Flags(0);
  RETURN_IF_TDI_ERROR(table->entryDel(*real_session->tdi_session_, *dev_tgt,
                                      *flags, *table_key))
      << "Could not delete action profile member with: " << dump_args();

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::GetActionProfileMembers(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, int member_id, std::vector<int>* member_ids,
    std::vector<std::unique_ptr<TableDataInterface>>* table_values) {
  RET_CHECK(member_ids);
  RET_CHECK(table_values);
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);

  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags* flags = new ::tdi::Flags(0);
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(table_id, &table));
  std::vector<std::unique_ptr<::tdi::TableKey>> keys;
  std::vector<std::unique_ptr<::tdi::TableData>> datums;
  // Is this a wildcard read?
  if (member_id != 0) {
    keys.resize(1);
    datums.resize(1);
    RETURN_IF_TDI_ERROR(table->keyAllocate(&keys[0]));
    RETURN_IF_TDI_ERROR(table->dataAllocate(&datums[0]));
    // Key: $ACTION_MEMBER_ID
    RETURN_IF_ERROR(SetFieldExact(keys[0].get(), kActionMemberId, member_id));
    RETURN_IF_TDI_ERROR(table->entryGet(*real_session->tdi_session_, *dev_tgt,
                                        *flags, *keys[0], datums[0].get()));
  } else {
    RETURN_IF_ERROR(GetAllEntries(real_session->tdi_session_, *dev_tgt, table,
                                  &keys, &datums));
  }

  member_ids->resize(0);
  table_values->resize(0);
  for (size_t i = 0; i < keys.size(); ++i) {
    // Key: $sid
    uint32 member_id = 0;
    RETURN_IF_ERROR(GetFieldExact(*keys[i], kActionMemberId, &member_id));
    member_ids->push_back(member_id);

    // Data: action params
    auto td = absl::make_unique<TableData>(std::move(datums[i]));
    table_values->push_back(std::move(td));
  }

  CHECK_EQ(member_ids->size(), keys.size());
  CHECK_EQ(table_values->size(), keys.size());

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::WriteActionProfileGroup(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, int group_id, int max_group_size,
    const std::vector<uint32>& member_ids,
    const std::vector<bool>& member_status, bool insert) {
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);

  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(table_id, &table));

  std::unique_ptr<::tdi::TableKey> table_key;
  std::unique_ptr<::tdi::TableData> table_data;
  RETURN_IF_TDI_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_TDI_ERROR(table->dataAllocate(&table_data));

  // We have to capture the std::unique_ptrs by reference [&] here.
  auto dump_args = [&]() -> std::string {
    return absl::StrCat(
        DumpTableMetadata(table).ValueOr("<error reading table>"),
        ", group_id: ", group_id, ", max_group_size: ", max_group_size,
        ", members: ", PrintVector(member_ids, ","), ", ",
        DumpTableKey(table_key.get()).ValueOr("<error parsing key>"), ", ",
        DumpTableData(table_data.get()).ValueOr("<error parsing data>"));
  };

  // Key: $SELECTOR_GROUP_ID
  RETURN_IF_ERROR(SetFieldExact(table_key.get(), kSelectorGroupId, group_id));
  // Data: $ACTION_MEMBER_ID
  RETURN_IF_ERROR(SetField(table_data.get(), kActionMemberId, member_ids));
  // Data: $ACTION_MEMBER_STATUS
  RETURN_IF_ERROR(
      SetField(table_data.get(), kActionMemberStatus, member_status));
  // Data: $MAX_GROUP_SIZE
  RETURN_IF_ERROR(
      SetField(table_data.get(), "$MAX_GROUP_SIZE", max_group_size));

  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags* flags = new ::tdi::Flags(0);
  if (insert) {
    RETURN_IF_TDI_ERROR(table->entryAdd(*real_session->tdi_session_, *dev_tgt,
                                        *flags, *table_key, *table_data))
        << "Could not add action profile group with: " << dump_args();
  } else {
    RETURN_IF_TDI_ERROR(table->entryMod(*real_session->tdi_session_, *dev_tgt,
                                        *flags, *table_key, *table_data))
        << "Could not modify action profile group with: " << dump_args();
  }

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::InsertActionProfileGroup(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, int group_id, int max_group_size,
    const std::vector<uint32>& member_ids,
    const std::vector<bool>& member_status) {
  ::absl::ReaderMutexLock l(&data_lock_);
  return WriteActionProfileGroup(dev_id, session, table_id, group_id,
                                 max_group_size, member_ids, member_status,
                                 true);
}

::util::Status TdiSdeWrapper::ModifyActionProfileGroup(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, int group_id, int max_group_size,
    const std::vector<uint32>& member_ids,
    const std::vector<bool>& member_status) {
  ::absl::ReaderMutexLock l(&data_lock_);
  return WriteActionProfileGroup(dev_id, session, table_id, group_id,
                                 max_group_size, member_ids, member_status,
                                 false);
}

::util::Status TdiSdeWrapper::DeleteActionProfileGroup(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, int group_id) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);

  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(table_id, &table));
  std::unique_ptr<::tdi::TableKey> table_key;
  RETURN_IF_TDI_ERROR(table->keyAllocate(&table_key));

  auto dump_args = [&]() -> std::string {
    return absl::StrCat(
        DumpTableMetadata(table).ValueOr("<error reading table>"),
        ", group_id: ", group_id,
        DumpTableKey(table_key.get()).ValueOr("<error parsing key>"));
  };

  // Key: $SELECTOR_GROUP_ID
  RETURN_IF_ERROR(SetFieldExact(table_key.get(), kSelectorGroupId, group_id));

  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags* flags = new ::tdi::Flags(0);
  RETURN_IF_TDI_ERROR(table->entryDel(*real_session->tdi_session_, *dev_tgt,
                                      *flags, *table_key))
      << "Could not delete action profile group with: " << dump_args();

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::GetActionProfileGroups(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, int group_id, std::vector<int>* group_ids,
    std::vector<int>* max_group_sizes,
    std::vector<std::vector<uint32>>* member_ids,
    std::vector<std::vector<bool>>* member_status) {
  RET_CHECK(group_ids);
  RET_CHECK(max_group_sizes);
  RET_CHECK(member_ids);
  RET_CHECK(member_status);
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);

  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags* flags = new ::tdi::Flags(0);
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(table_id, &table));
  std::vector<std::unique_ptr<::tdi::TableKey>> keys;
  std::vector<std::unique_ptr<::tdi::TableData>> datums;
  // Is this a wildcard read?
  if (group_id != 0) {
    keys.resize(1);
    datums.resize(1);
    RETURN_IF_TDI_ERROR(table->keyAllocate(&keys[0]));
    RETURN_IF_TDI_ERROR(table->dataAllocate(&datums[0]));
    // Key: $SELECTOR_GROUP_ID
    RETURN_IF_ERROR(SetFieldExact(keys[0].get(), kSelectorGroupId, group_id));
    RETURN_IF_TDI_ERROR(table->entryGet(*real_session->tdi_session_, *dev_tgt,
                                        *flags, *keys[0], datums[0].get()));
  } else {
    RETURN_IF_ERROR(GetAllEntries(real_session->tdi_session_, *dev_tgt, table,
                                  &keys, &datums));
  }

  group_ids->resize(0);
  max_group_sizes->resize(0);
  member_ids->resize(0);
  member_status->resize(0);
  for (size_t i = 0; i < keys.size(); ++i) {
    const std::unique_ptr<::tdi::TableData>& table_data = datums[i];
    const std::unique_ptr<::tdi::TableKey>& table_key = keys[i];
    // Key: $SELECTOR_GROUP_ID
    uint32 group_id = 0;
    RETURN_IF_ERROR(GetFieldExact(*table_key, kSelectorGroupId, &group_id));
    group_ids->push_back(group_id);

    // Data: $MAX_GROUP_SIZE
    uint64 max_group_size;
    RETURN_IF_ERROR(GetField(*table_data, "$MAX_GROUP_SIZE", &max_group_size));
    max_group_sizes->push_back(max_group_size);

    // Data: $ACTION_MEMBER_ID
    std::vector<uint32> members;
    RETURN_IF_ERROR(GetField(*table_data, kActionMemberId, &members));
    member_ids->push_back(members);

    // Data: $ACTION_MEMBER_STATUS
    std::vector<bool> member_enabled;
    RETURN_IF_ERROR(
        GetField(*table_data, kActionMemberStatus, &member_enabled));
    member_status->push_back(member_enabled);
  }

  CHECK_EQ(group_ids->size(), keys.size());
  CHECK_EQ(max_group_sizes->size(), keys.size());
  CHECK_EQ(member_ids->size(), keys.size());
  CHECK_EQ(member_status->size(), keys.size());

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::InsertTableEntry(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, const TableKeyInterface* table_key,
    const TableDataInterface* table_data) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);
  auto real_table_key = dynamic_cast<const TableKey*>(table_key);
  RET_CHECK(real_table_key);
  auto real_table_data = dynamic_cast<const TableData*>(table_data);
  RET_CHECK(real_table_data);

  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(table_id, &table));

  auto dump_args = [&]() -> std::string {
    return absl::StrCat(
        DumpTableMetadata(table).ValueOr("<error reading table>"), ", ",
        DumpTableKey(real_table_key->table_key_.get())
            .ValueOr("<error parsing key>"),
        ", ",
        DumpTableData(real_table_data->table_data_.get())
            .ValueOr("<error parsing data>"));
  };

  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  /* Note: When multiple pipeline support is added, for device target
   * pipeline id also should be set
   */

  ::tdi::Flags* flags = new ::tdi::Flags(0);
  RETURN_IF_TDI_ERROR(table->entryAdd(*real_session->tdi_session_, *dev_tgt,
                                      *flags, *real_table_key->table_key_,
                                      *real_table_data->table_data_))
      << "Could not add table entry with: " << dump_args();

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::ModifyTableEntry(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, const TableKeyInterface* table_key,
    const TableDataInterface* table_data) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);
  auto real_table_key = dynamic_cast<const TableKey*>(table_key);
  RET_CHECK(real_table_key);
  auto real_table_data = dynamic_cast<const TableData*>(table_data);
  RET_CHECK(real_table_data);

  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(table_id, &table));

  auto dump_args = [&]() -> std::string {
    return absl::StrCat(
        DumpTableMetadata(table).ValueOr("<error reading table>"), ", ",
        DumpTableKey(real_table_key->table_key_.get())
            .ValueOr("<error parsing key>"),
        ", ",
        DumpTableData(real_table_data->table_data_.get())
            .ValueOr("<error parsing data>"));
  };

  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags* flags = new ::tdi::Flags(0);
  RETURN_IF_TDI_ERROR(table->entryMod(*real_session->tdi_session_, *dev_tgt,
                                      *flags, *real_table_key->table_key_,
                                      *real_table_data->table_data_))
      << "Could not modify table entry with: " << dump_args();

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::DeleteTableEntry(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, const TableKeyInterface* table_key) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);
  auto real_table_key = dynamic_cast<const TableKey*>(table_key);
  RET_CHECK(real_table_key);

  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(table_id, &table));

  auto dump_args = [&]() -> std::string {
    return absl::StrCat(
        DumpTableMetadata(table).ValueOr("<error reading table>"), ", ",
        DumpTableKey(real_table_key->table_key_.get())
            .ValueOr("<error parsing key>"));
  };

  // TDI comments; Hardcoding device = 0
  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags* flags = new ::tdi::Flags(0);
  RETURN_IF_TDI_ERROR(table->entryDel(*real_session->tdi_session_, *dev_tgt,
                                      *flags, *real_table_key->table_key_))
      << "Could not delete table entry with: " << dump_args();

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::GetTableEntry(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, const TableKeyInterface* table_key,
    TableDataInterface* table_data) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);
  auto real_table_key = dynamic_cast<const TableKey*>(table_key);
  RET_CHECK(real_table_key);
  auto real_table_data = dynamic_cast<const TableData*>(table_data);
  RET_CHECK(real_table_data);
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(table_id, &table));

  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags* flags = new ::tdi::Flags(0);
  RETURN_IF_TDI_ERROR(table->entryGet(*real_session->tdi_session_, *dev_tgt,
                                      *flags, *real_table_key->table_key_,
                                      real_table_data->table_data_.get()));
  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::GetAllTableEntries(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id,
    std::vector<std::unique_ptr<TableKeyInterface>>* table_keys,
    std::vector<std::unique_ptr<TableDataInterface>>* table_values) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(table_id, &table));

  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  std::vector<std::unique_ptr<::tdi::TableKey>> keys;
  std::vector<std::unique_ptr<::tdi::TableData>> datums;
  RETURN_IF_ERROR(GetAllEntries(real_session->tdi_session_, *dev_tgt, table,
                                &keys, &datums));
  table_keys->resize(0);
  table_values->resize(0);

  for (size_t i = 0; i < keys.size(); ++i) {
    auto tk = absl::make_unique<TableKey>(std::move(keys[i]));
    auto td = absl::make_unique<TableData>(std::move(datums[i]));
    table_keys->push_back(std::move(tk));
    table_values->push_back(std::move(td));
  }

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::SetDefaultTableEntry(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, const TableDataInterface* table_data) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);
  auto real_table_data = dynamic_cast<const TableData*>(table_data);
  RET_CHECK(real_table_data);
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(table_id, &table));

  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags* flags = new ::tdi::Flags(0);
  RETURN_IF_TDI_ERROR(table->defaultEntrySet(*real_session->tdi_session_,
                                             *dev_tgt, *flags,
                                             *real_table_data->table_data_));
  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::ResetDefaultTableEntry(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(table_id, &table));

  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags* flags = new ::tdi::Flags(0);
  RETURN_IF_TDI_ERROR(
      table->defaultEntryReset(*real_session->tdi_session_, *dev_tgt, *flags));

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::GetDefaultTableEntry(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, TableDataInterface* table_data) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);
  auto real_table_data = dynamic_cast<const TableData*>(table_data);
  RET_CHECK(real_table_data);
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(table_id, &table));

  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags* flags = new ::tdi::Flags(0);
  RETURN_IF_TDI_ERROR(
      table->defaultEntryGet(*real_session->tdi_session_, *dev_tgt, *flags,
                             real_table_data->table_data_.get()));

  return ::util::OkStatus();
}

::util::StatusOr<uint32> TdiSdeWrapper::GetTdiRtId(uint32 p4info_id) const {
  ::absl::ReaderMutexLock l(&data_lock_);
  return tdi_id_mapper_->GetTdiRtId(p4info_id);
}

::util::StatusOr<uint32> TdiSdeWrapper::GetP4InfoId(uint32 tdi_id) const {
  ::absl::ReaderMutexLock l(&data_lock_);
  return tdi_id_mapper_->GetP4InfoId(tdi_id);
}

::util::StatusOr<uint32> TdiSdeWrapper::GetActionSelectorTdiRtId(
    uint32 action_profile_id) const {
  ::absl::ReaderMutexLock l(&data_lock_);
  return tdi_id_mapper_->GetActionSelectorTdiRtId(action_profile_id);
}

::util::StatusOr<uint32> TdiSdeWrapper::GetActionProfileTdiRtId(
    uint32 action_selector_id) const {
  ::absl::ReaderMutexLock l(&data_lock_);
  return tdi_id_mapper_->GetActionProfileTdiRtId(action_selector_id);
}

::util::Status TdiSdeWrapper::SynchronizeCounters(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, absl::Duration timeout) {
  ::absl::ReaderMutexLock l(&data_lock_);
  return DoSynchronizeCounters(dev_id, session, table_id, timeout);
}

::util::Status TdiSdeWrapper::DoSynchronizeCounters(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, absl::Duration timeout) {
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);

  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(table_id, &table));

  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  // Sync table counter
  std::set<tdi_operations_type_e> supported_ops;
  supported_ops = table->tableInfoGet()->operationsSupported();
  // TODO TDI comments : Uncomment this after SDE exposes counterSyncSet
#if 0
  if (supported_ops.count(static_cast<tdi_operations_type_e>(
      tdi_rt_operations_type_e::COUNTER_SYNC))) {
    auto sync_notifier = std::make_shared<absl::Notification>();
    std::weak_ptr<absl::Notification> weak_ref(sync_notifier);
    std::unique_ptr<::tdi::TableOperations> table_op;
    RETURN_IF_TDI_ERROR(table->operationsAllocate(
        static_cast<tdi_operations_type_e>(
          tdi_rt_operations_type_e::COUNTER_SYNC), &table_op));
    RETURN_IF_TDI_ERROR(table_op->counterSyncSet(
        *real_session->tdi_session_, dev_tgt,
        [table_id, weak_ref](const ::tdi::Target& dev_tgt, void* cookie) {
          if (auto notifier = weak_ref.lock()) {
            VLOG(1) << "Table counter for table " << table_id << " synced.";
            notifier->Notify();
          } else {
            VLOG(1) << "Notifier expired before table " << table_id
                    << " could be synced.";
          }
        },
        nullptr));
    RETURN_IF_TDI_ERROR(table->tableOperationsExecute(*table_op.get()));
    // Wait until sync done or timeout.
    if (!sync_notifier->WaitForNotificationWithTimeout(timeout)) {
      return MAKE_ERROR(ERR_OPER_TIMEOUT)
             << "Timeout while syncing (indirect) table counters of table "
             << table_id << ".";
    }
  }
#endif
  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::SynchronizeRegisters(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, absl::Duration timeout) {
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);

  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(table_id, &table));

  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  // Sync table registers.
  // TDI comments ; its supposed to be tdi_rt_operations_type_e ??
  // const std::set<tdi_rt_operations_type_e> supported_ops;
  // supported_ops =
  // static_cast<tdi_rt_operations_type_e>(table->tableInfoGet()->operationsSupported());

  std::set<tdi_operations_type_e> supported_ops;
  supported_ops = table->tableInfoGet()->operationsSupported();
  // TODO TDI comments : Need to uncomment this after SDE exposes
  // registerSyncSet
#if 0
  if (supported_ops.count(static_cast<tdi_operations_type_e>(
          tdi_rt_operations_type_e::REGISTER_SYNC))) {
    auto sync_notifier = std::make_shared<absl::Notification>();
    std::weak_ptr<absl::Notification> weak_ref(sync_notifier);
    std::unique_ptr<::tdi::TableOperations> table_op;
    RETURN_IF_TDI_ERROR(
        table->operationsAllocate(static_cast<tdi_operations_type_e>(
                                      tdi_rt_operations_type_e::REGISTER_SYNC),
                                  &table_op));
    RETURN_IF_TDI_ERROR(table_op->registerSyncSet(
        *real_session->tdi_session_, dev_tgt,
        [table_id, weak_ref](const ::tdi::Target& dev_tgt, void* cookie) {
          if (auto notifier = weak_ref.lock()) {
            VLOG(1) << "Table registers for table " << table_id << " synced.";
            notifier->Notify();
          } else {
            VLOG(1) << "Notifier expired before table " << table_id
                    << " could be synced.";
          }
        },
        nullptr));
    RETURN_IF_TDI_ERROR(table->tableOperationsExecute(*table_op.get()));
    // Wait until sync done or timeout.
    if (!sync_notifier->WaitForNotificationWithTimeout(timeout)) {
      return MAKE_ERROR(ERR_OPER_TIMEOUT)
             << "Timeout while syncing (indirect) table registers of table "
             << table_id << ".";
    }
  }
#endif
  return ::util::OkStatus();
}

TdiSdeWrapper* TdiSdeWrapper::CreateSingleton() {
  absl::WriterMutexLock l(&init_lock_);
  if (!singleton_) {
    singleton_ = new TdiSdeWrapper();
  }

  return singleton_;
}

TdiSdeWrapper* TdiSdeWrapper::GetSingleton() {
  absl::ReaderMutexLock l(&init_lock_);
  return singleton_;
}

}  // namespace tdi
}  // namespace hal
}  // namespace stratum
