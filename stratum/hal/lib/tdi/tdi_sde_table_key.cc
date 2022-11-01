// Copyright 2019-present Barefoot Networks, Inc.
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// Target-agnostic SDE wrapper Table Key methods.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "gflags/gflags_declare.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/p4/utils.h"
#include "stratum/hal/lib/tdi/macros.h"
#include "stratum/hal/lib/tdi/tdi_constants.h"
#include "stratum/hal/lib/tdi/tdi_sde_common.h"
#include "stratum/hal/lib/tdi/tdi_sde_helpers.h"
#include "stratum/hal/lib/tdi/tdi_sde_wrapper.h"
#include "stratum/hal/lib/tdi/utils.h"

DECLARE_bool(incompatible_enable_tdi_legacy_bytestring_responses);

namespace stratum {
namespace hal {
namespace tdi {

using namespace stratum::hal::tdi::helpers;

::util::Status TableKey::SetExact(int id, const std::string& value) {
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(table_key_->tableGet(&table));
  size_t field_size_bits;
  auto tableInfo = table->tableInfoGet();
  const ::tdi::KeyFieldInfo* keyFieldInfo =
      tableInfo->keyFieldGet(static_cast<tdi_id_t>(id));
  RETURN_IF_NULL(keyFieldInfo);

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
  RETURN_IF_NULL(keyFieldInfo);

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
  RETURN_IF_NULL(keyFieldInfo);

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
  RETURN_IF_NULL(keyFieldInfo);

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
  RETURN_IF_NULL(keyFieldInfo);

  field_size_bits = keyFieldInfo->sizeGet();
  value->clear();
  value->resize(NumBitsToNumBytes(field_size_bits));

  const char* valueExact = reinterpret_cast<const char*>(value->data());
  size_t size = reinterpret_cast<size_t>(value->size());

  ::tdi::KeyFieldValueExact<const char*> exactKey(valueExact, size);

  RETURN_IF_TDI_ERROR(
      table_key_->getValue(static_cast<tdi_id_t>(id), &exactKey));

  if (!FLAGS_incompatible_enable_tdi_legacy_bytestring_responses) {
    *value = ByteStringToP4RuntimeByteString(*value);
  }

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
  RETURN_IF_NULL(keyFieldInfo);

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

  if (!FLAGS_incompatible_enable_tdi_legacy_bytestring_responses) {
    *value = ByteStringToP4RuntimeByteString(*value);
    *mask = ByteStringToP4RuntimeByteString(*mask);
  }

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
  RETURN_IF_NULL(keyFieldInfo);

  field_size_bits = keyFieldInfo->sizeGet();
  prefix->clear();
  prefix->resize(NumBitsToNumBytes(field_size_bits));

  const char* valueLpm = reinterpret_cast<const char*>(prefix->data());
  size_t sizeLpm = reinterpret_cast<size_t>(prefix->size());
  ::tdi::KeyFieldValueLPM<const char*> lpmKey(valueLpm, *prefix_length,
                                              sizeLpm);

  RETURN_IF_TDI_ERROR(table_key_->getValue(static_cast<tdi_id_t>(id), &lpmKey));

  if (!FLAGS_incompatible_enable_tdi_legacy_bytestring_responses) {
    *prefix = ByteStringToP4RuntimeByteString(*prefix);
  }

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
  RETURN_IF_NULL(keyFieldInfo);

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
  if (!FLAGS_incompatible_enable_tdi_legacy_bytestring_responses) {
    *low = ByteStringToP4RuntimeByteString(*low);
    *high = ByteStringToP4RuntimeByteString(*high);
  }
  return ::util::OkStatus();
}

::util::Status TableKey::GetPriority(uint32* priority) const {
  uint32_t value = 0;
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

}  // namespace tdi
}  // namespace hal
}  // namespace stratum
