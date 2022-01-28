// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/bcm/bcm_udf_manager.h"

#include <algorithm>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/lib/macros.h"
#include "stratum/public/proto/p4_table_defs.pb.h"

namespace stratum {

namespace hal {
namespace bcm {

namespace {
// Returns the BcmPacketLayer that contains the P4HeaderType. Does not handle
// tunneling.
BcmUdfSet::PacketLayer HeaderTypeToPacketLayer(P4HeaderType header_type) {
  switch (header_type) {
    case P4_HEADER_ETHERNET:
      return BcmUdfSet::L2_HEADER;
    case P4_HEADER_ARP:
    case P4_HEADER_IPV4:
    case P4_HEADER_IPV6:
      return BcmUdfSet::L3_HEADER;
    case P4_HEADER_GRE:
    case P4_HEADER_ICMP:
    case P4_HEADER_TCP:
    case P4_HEADER_UDP:
      return BcmUdfSet::L4_HEADER;
    case P4_HEADER_PACKET_IN:
    case P4_HEADER_PACKET_OUT:
    case P4_HEADER_VLAN:
    case P4_HEADER_UNKNOWN:
    default:
      return BcmUdfSet::UNKNOWN;
  }
}
}  // namespace

template <typename T>
bool BcmUdfManager::UdfSet::AddChunks(const T& chunks) {
  std::vector<UdfChunk> inserted_chunks;
  for (auto chunk : chunks) {
    chunk.set_id(chunks_.size() + base_chunk_id_);
    auto result = chunks_.insert(std::move(chunk));
    if (result.second) {
      inserted_chunks.push_back(*result.first);
    }
  }
  // Roll back the operations if we went past the max allocation.
  if (chunks_.size() > max_chunks_) {
    for (const auto& chunk : inserted_chunks) {
      chunks_.erase(chunk);
    }
    return false;
  }
  return true;
}

std::vector<BcmUdfManager::UdfChunk> BcmUdfManager::UdfChunk::MappedFieldToUdfs(
    const MappedField& mapped_field, int chunk_size) {
  BcmUdfSet::PacketLayer packet_layer =
      HeaderTypeToPacketLayer(mapped_field.header_type());
  if (packet_layer == BcmUdfSet::UNKNOWN) return {};

  int first_bit = mapped_field.bit_offset();
  int last_bit = mapped_field.bit_offset() + mapped_field.bit_width() - 1;

  // Calculate the base offset of the chunk containing first_bit.
  int first_chunk_base = (first_bit / chunk_size) * chunk_size;

  std::vector<UdfChunk> udf_chunks;
  for (int chunk_offset = first_chunk_base; chunk_offset <= last_bit;
       chunk_offset += chunk_size) {
    udf_chunks.emplace_back(packet_layer, chunk_offset, chunk_size);
  }
  return udf_chunks;
}

BcmUdfManager::BcmUdfManager(
    BcmSdkInterface* bcm_sdk_interface,
    const BcmHardwareSpecs::ChipModelSpec::UdfSpec& udf_spec,
    int num_controller_sets, int unit, const P4TableMapper* p4_table_mapper,
    std::function<bool(const MappedField&, BcmAclStage)> is_udf_eligible)
    : bcm_sdk_interface_(bcm_sdk_interface),
      udf_sets_(),
      chunk_size_(udf_spec.chunk_bits()),
      chunks_per_set_(udf_spec.chunks_per_set()),
      unit_(unit),
      p4_table_mapper_(p4_table_mapper),
      is_udf_eligible_(std::move(is_udf_eligible)) {
  // Allocate UDF sets.
  int base_chunk_id = 1;
  for (int i = 1; i <= udf_spec.set_count(); ++i) {
    UdfSet udf_set(/*usage=*/i <= num_controller_sets ? kController : kStatic,
                   /*base_chunk_id=*/base_chunk_id,
                   /*max_chunks=*/udf_spec.chunks_per_set());
    udf_sets_.emplace(i, udf_set);
    base_chunk_id += udf_spec.chunks_per_set();
  }
}

::util::StatusOr<std::unique_ptr<BcmUdfManager>> BcmUdfManager::CreateInstance(
    BcmSdkInterface* bcm_sdk_interface,
    const BcmHardwareSpecs::ChipModelSpec::UdfSpec& udf_spec,
    int num_controller_sets, int unit, const P4TableMapper* p4_table_mapper,
    std::function<bool(const MappedField&, BcmAclStage)> is_udf_eligible) {
  if (num_controller_sets > udf_spec.set_count()) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Cannot allocate more controller UDF sets ("
           << num_controller_sets << ") than hardware UDF sets ("
           << udf_spec.set_count() << ").";
  }
  return absl::WrapUnique(
      new BcmUdfManager(bcm_sdk_interface, udf_spec, num_controller_sets, unit,
                        p4_table_mapper, std::move(is_udf_eligible)));
}

bool BcmUdfManager::DefaultIsUdfEligible(const MappedField& mapped_field,
                                         BcmAclStage stage) {
  // UDFs only apply to ACL tables.
  if (stage == BCM_ACL_STAGE_UNKNOWN) return false;
  switch (mapped_field.type()) {
    case P4_FIELD_TYPE_ARP_TPA:
      return true;
    // The following case exists in Sandcastle but may not be needed for
    // Stratum.
    // case P4_FIELD_TYPE_ETH_DST:
    //   return stage == BCM_ACL_STAGE_IFP;
    default:
      return false;
  }
}

::util::StatusOr<int> BcmUdfManager::AllocateUdfSet(
    const UdfSet& input_set, const std::vector<int>& destination_sets) {
  int best_candidate_id = -1;
  int best_candidate_impact = INT_MAX;
  for (int candidate_id : destination_sets) {
    // Calculate impact as the number of chunks in the input_set but not in the
    // candidate_set. If the impact would cause the candidate_set to grow too
    // large, skip it.
    const UdfSet* candidate_set = gtl::FindOrNull(udf_sets_, candidate_id);
    if (candidate_set == nullptr) {
      return MAKE_ERROR(ERR_INTERNAL)
             << "Failed to lookup destination set from the full UDF set map. "
                "This is a bug.";
    }
    UdfSet candidate_copy = *candidate_set;
    if (!candidate_copy.MergeFrom(input_set)) continue;
    int impact =
        candidate_copy.chunks().size() - candidate_set->chunks().size();

    // Allocate input_set to the least-impactful candidate set.
    if (impact < best_candidate_impact) {
      best_candidate_id = candidate_id;
      best_candidate_impact = impact;
    }
  }

  // Merge input_set to the best candidate set.
  UdfSet* best_candidate = gtl::FindOrNull(udf_sets_, best_candidate_id);
  if (best_candidate == nullptr) {
    return MAKE_ERROR(ERR_NO_RESOURCE)
           << "Hardware does not have enough remaining free chunks for the UDF "
              "set.";
  }
  if (!best_candidate->MergeFrom(input_set)) {
    return MAKE_ERROR(ERR_INTERNAL) << "Calculated candidate merging does not "
                                       "match actual merging. This is a bug.";
  }
  return best_candidate_id;
}

::util::StatusOr<BcmUdfManager::UdfSet> BcmUdfManager::StaticUdfSetFromAclTable(
    const AclTable& table, std::vector<uint32>* udf_match_fields) {
  UdfSet udf_set(chunks_per_set_);
  for (uint32 match_field : table.MatchFields()) {
    // Grab the field data.
    MappedField mapped_field;
    RETURN_IF_ERROR(p4_table_mapper_->MapMatchField(table.Id(), match_field,
                                                    &mapped_field));

    // Skip fields that should not be treated as UDF.
    if (!is_udf_eligible_(mapped_field, table.Stage())) continue;
    if (udf_match_fields != nullptr) {
      udf_match_fields->push_back(match_field);
    }

    std::vector<BcmUdfManager::UdfChunk> chunks =
        UdfChunk::MappedFieldToUdfs(mapped_field, chunk_size_);
    if (chunks.empty()) {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Table (" << table.Name() << ") field type: ("
             << P4FieldType_Name(mapped_field.type())
             << ") cannot be converted to UDF.";
    }

    // Map the fields to UDF.
    if (!udf_set.AddChunks(chunks)) {
      return MAKE_ERROR(ERR_NO_RESOURCE)
             << "Table (" << table.Name() << ") requires more than the maximum "
             << chunks_per_set_ << " UDF chunks per set.";
    }
  }
  return udf_set;
}

::util::Status BcmUdfManager::SetUpStaticUdfs(
    std::vector<AclTable>* acl_tables) {
  // Grab the set of UDFs that apply to each AclTable.
  // udf_sets_per_table only contains tables that require UDFs.
  absl::flat_hash_map<AclTable*, UdfSet> udf_sets_per_table;
  absl::flat_hash_map<AclTable*, std::vector<uint32>>
      udf_match_fields_per_table;
  for (AclTable& table : *acl_tables) {
    std::vector<uint32> udf_match_fields;
    ASSIGN_OR_RETURN(UdfSet udf_set,
                     StaticUdfSetFromAclTable(table, &udf_match_fields));
    if (!udf_set.chunks().empty()) {
      udf_sets_per_table.emplace(&table, std::move(udf_set));
      udf_match_fields_per_table.emplace(&table, std::move(udf_match_fields));
    }
  }

  // Do nothing if there are no UDFs to manage.
  if (udf_sets_per_table.empty()) {
    return ::util::OkStatus();
  }

  // Allocate each ACL table's UDF set to the static tables.
  std::vector<int> static_sets = UdfSetsByUsage(kStatic);
  const std::vector<uint32> empty_vector;
  for (const auto& pair : udf_sets_per_table) {
    ASSIGN_OR_RETURN(int udf_set_id, AllocateUdfSet(pair.second, static_sets));
    // FIXME error message: _ << " Failed to allocate UDF set for table "
    // << pair.first->Id() << "."
    for (uint32 match_field : gtl::FindWithDefault(udf_match_fields_per_table,
                                                   pair.first, empty_vector)) {
      RETURN_IF_ERROR(pair.first->MarkUdfMatchField(match_field, udf_set_id));
    }
  }

  // TODO(richardyu): Install the static UDFs into hardware.
  return ::util::OkStatus();
}

::util::Status BcmUdfManager::InstallUdfs() {
  BcmUdfSet bcm_udf_set;
  for (const auto& pair : udf_sets_) {
    const UdfSet& udf_set = pair.second;
    for (const UdfChunk& udf_chunk : udf_set.chunks()) {
      BcmUdfSet::UdfChunk* bcm_chunk = bcm_udf_set.add_chunks();
      bcm_chunk->set_id(udf_chunk.id());
      bcm_chunk->set_layer(udf_chunk.packet_layer());
      bcm_chunk->set_offset(udf_chunk.byte_offset());
    }
  }
  // Don't install if there are no chunks.
  if (!bcm_udf_set.chunks().empty()) {
    RETURN_IF_ERROR(bcm_sdk_interface_->SetAclUdfChunks(unit_, bcm_udf_set));
  }
  return ::util::OkStatus();
}

// Local-scope helper functions for converting MappedField values to UDF chunks.
namespace {

// Fills in the values of a buffer that may be offset or differently sized from
// the input buffer.
// Parameters:
//          input: Input buffer. Data is assumed to be right-justified with any
//                 extra leading bits set to 0.
//   input_offset: Offset of the input buffer from 0 in bits. Used to calculate
//                 relative offset.
//     input_size: Size of the input buffer in bits.
//  output_offset: Offset of the output buffer from 0 in bits. Used to calculate
//                 relative offset.
//    output_size: Size of the output buffer in bits. If output_size is not a
//                 multiple of 8, it will be rounded up to the nearest multiple
//                 of 8.
std::string OffsetBuffer(const std::string& input, size_t input_offset,
                         size_t input_size, size_t output_offset,
                         size_t output_size) {
  // Calculate the inclusive bit range of each buffer. All the buffers are
  // physically byte-aligned, but the sizes may not be. We assume all buffers
  // are right-justified and any out-of-range input buffer bits are zero.
  int right_end_offset = input_offset + input_size;
  std::pair<int, int> in_buffer_range = {right_end_offset - (input.size() * 8),
                                         right_end_offset - 1};
  const int output_buffer_size = (output_size + 7) / 8;
  std::pair<int, int> out_buffer_range = {
      output_offset, output_offset + (output_buffer_size * 8) - 1};

  // If the ranges do not overlap, return a zeroed buffer. We use output_offset
  // instead of out_buffer_range.first since it is more restrictive (always >=).
  std::string output(output_buffer_size, 0);
  LOG(INFO) << "output orig size: " << output.size();
  if (in_buffer_range.first > out_buffer_range.second ||
      in_buffer_range.second < output_offset) {
    return output;
  }

  // Fill in each output byte.
  for (uint32 byte_index = 0; byte_index < output_buffer_size; ++byte_index) {
    int offset_of_output_byte = byte_index * 8 + out_buffer_range.first;
    // Skip this byte if there is no overlap with the input buffer.
    if (offset_of_output_byte + 8 <= in_buffer_range.first ||
        offset_of_output_byte > in_buffer_range.second) {
      continue;
    }
    // Indices of the overlapping byte in the input buffer.
    int offset_from_input = offset_of_output_byte - in_buffer_range.first;
    int input_byte_index = offset_from_input / 8;

    // In this case, we take part of the first byte of the input buffer.
    if (offset_from_input < 0) {
      output[byte_index] =
          static_cast<uint8>(input.at(input_byte_index)) >> -offset_from_input;
      continue;
    }

    // Here, we take parts of the two overlapping bytes.
    uint16 bit_shift = offset_from_input & 7;
    output[byte_index] = static_cast<uint8>(input.at(input_byte_index))
                         << bit_shift;
    if (input_byte_index + 1 < input.size()) {
      output[byte_index] +=
          static_cast<uint8>(input.at(input_byte_index + 1)) >> (8 - bit_shift);
    }
  }

  return output;
}

// Fills a data buffer with the u32 or u64 data value in network order if
// applicable.
//
// Returns:
//   If value.u32: data_buffer.
//   If value.u64: data_buffer.
//   If   value.b: &value.b().
//   Else        : nullptr.
const std::string* GetDataBuffer(const MappedField::Value& value,
                                 std::string* data_buffer) {
  switch (value.data_case()) {
    case MappedField::Value::kU32:
      data_buffer->resize(sizeof(uint32), 0);
      for (int i = 0; i < sizeof(uint32); ++i) {
        data_buffer->at(data_buffer->size() - i - 1) =
            static_cast<uint8>((value.u32() >> (i * 8)) & 0xff);
      }
      break;
    case MappedField::Value::kU64:
      data_buffer->resize(sizeof(uint64), 0);
      for (int i = 0; i < sizeof(uint64); ++i) {
        data_buffer->at(data_buffer->size() - i - 1) =
            static_cast<uint8>((value.u64() >> (i * 8)) & 0xff);
      }
      break;
    case MappedField::Value::kB:
      return &value.b();
    default:
      return nullptr;
  }
  return data_buffer;
}

}  // namespace

::util::StatusOr<BcmField> BcmUdfManager::CreateBcmField(
    const UdfChunk& chunk, const MappedField& mapped_field) {
  if (chunk.bit_offset() + chunk.bit_size() <= mapped_field.bit_offset() ||
      chunk.bit_offset() >=
          mapped_field.bit_offset() + mapped_field.bit_width()) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "UdfChunk " << chunk.ToString()
           << " is outside the scope of MappedField "
           << mapped_field.ShortDebugString() << ".";
  }
  BcmField bcm_field;
  bcm_field.set_type(BcmField::UNKNOWN);
  bcm_field.set_udf_chunk_id(chunk.id());

  std::string buffer;
  const std::string* value = GetDataBuffer(mapped_field.value(), &buffer);
  if (value == nullptr) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Unable to map unsupported data type to UDF: "
           << mapped_field.value().ShortDebugString() << ".";
  }
  bcm_field.mutable_value()->set_b(
      OffsetBuffer(*value, mapped_field.bit_offset(), mapped_field.bit_width(),
                   chunk.bit_offset(), chunk.bit_size()));

  const std::string* mask = GetDataBuffer(mapped_field.mask(), &buffer);
  if (mask == nullptr) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Unable to map unsupported data type to UDF: "
           << mapped_field.mask().ShortDebugString() << ".";
  }
  bcm_field.mutable_mask()->set_b(
      OffsetBuffer(*mask, mapped_field.bit_offset(), mapped_field.bit_width(),
                   chunk.bit_offset(), chunk.bit_size()));
  return bcm_field;
}

::util::StatusOr<std::vector<BcmField>> BcmUdfManager::MappedFieldToBcmFields(
    int udf_set_id, const MappedField& mapped_field) {
  // Check for sanity.
  const UdfSet* udf_set = gtl::FindOrNull(udf_sets_, udf_set_id);
  if (udf_set == nullptr) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Unknown UDF set " << udf_set_id << ".";
  }
  std::vector<UdfChunk> reference_chunks =
      UdfChunk::MappedFieldToUdfs(mapped_field, chunk_size_);
  if (reference_chunks.empty()) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "UDF is not supported for mapped field: "
           << mapped_field.ShortDebugString() << ".";
  }
  std::vector<BcmField> bcm_fields;
  for (UdfChunk& reference_chunk : reference_chunks) {
    auto lookup = udf_set->chunks().find(reference_chunk);
    if (lookup == udf_set->chunks().end()) {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Required UdfChunk: " << reference_chunk.ToString()
             << " is not in UDF set " << udf_set_id << ".";
    }
    reference_chunk.set_id(lookup->id());
    auto create_bcm_field_result =
        CreateBcmField(reference_chunk, mapped_field);
    if (!create_bcm_field_result.ok()) {
      return MAKE_ERROR(ERR_INTERNAL)
             << "Failed to create BcmField for chunk "
             << reference_chunk.ToString() << ": "
             << create_bcm_field_result.status().error_message()
             << ". This is a bug.";
    }
    bcm_fields.push_back(create_bcm_field_result.ValueOrDie());
  }
  return bcm_fields;
}

}  // namespace bcm
}  // namespace hal

}  // namespace stratum
