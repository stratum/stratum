// This file implements the FieldDecoder class in the Hercules p4c backend.

#include "platforms/networking/hercules/p4c_backend/switch/field_decoder.h"

#include <vector>

#include "base/logging.h"
#include "platforms/networking/hercules/p4c_backend/switch/field_name_inspector.h"
#include "platforms/networking/hercules/p4c_backend/switch/p4_model_names.host.pb.h"
#include "platforms/networking/hercules/p4c_backend/switch/table_map_generator.h"
#include "platforms/networking/hercules/p4c_backend/switch/utils.h"

namespace google {
namespace hercules {
namespace p4c_backend {

FieldDecoder::FieldDecoder(TableMapGenerator* table_mapper)
    : table_mapper_(ABSL_DIE_IF_NULL(table_mapper)),
      headers_done_(false),
      match_keys_done_(false) {}

void FieldDecoder::ConvertHeaderFields(
    const std::vector<const IR::Type_Typedef*>& p4_typedefs,
    const std::vector<const IR::Type_Enum*>& p4_enums,
    const std::vector<const IR::Type_StructLike*>& struct_likes,
    const std::vector<const IR::Type_Header*>& header_types,
    const HeaderPathInspector::PathToHeaderTypeMap& path_to_header_type_map) {
  if (headers_done_) {
    LOG(INFO) << __func__ << " was called multiple times";
    return;
  }

  // FieldDecoder extends the input path_to_header_type_map with additional
  // entries for packet IO metadata mapping, so it creates this local copy.
  // It uses annotated_types to record field types it finds in @switchstack
  // annotations.
  HeaderPathInspector::PathToHeaderTypeMap local_header_type_map =
      path_to_header_type_map;
  AnnotatedFieldTypeMap annotated_types;

  // The first step in header field conversion is to process any typedefs
  // and enums in the P4 program.
  for (auto p4_typedef : p4_typedefs) {
    if (!DecodeP4Typedef(*p4_typedef)) {
      LOG(WARNING) << "Unsupported typedef syntax in "
                   << p4_typedef->externalName();
    }
  }
  for (auto p4_enum : p4_enums) {
    if (!DecodeP4Enum(*p4_enum)) {
      LOG(WARNING) << "Unsupported enum syntax in " << p4_enum->externalName();
    }
  }

  // The next step in header field conversion is to iterate header_types to:
  // - Create a map from type name to a list of fields within the type.
  // - Update local_header_type_map with entries for controller packet metadata.
  //   Unlike other P4Info fields, controller metadata fields do not use fully
  //   qualified names, so these entries enable the subsequent creation of
  //   table map field descriptors with a unique key.  FieldDecoder creates
  //   these entries mapping the metadata name from the @controller_header
  //   annotation to the metadata header type.
  for (auto h_type : header_types) {
    const std::string header_type_name(h_type->externalName());
    VLOG(1) << "Converting header_type " << header_type_name;
    const std::string metadata_name = GetControllerHeaderAnnotation(*h_type);
    if (!metadata_name.empty()) {
      auto insert_status = local_header_type_map.insert(std::make_pair(
          metadata_name, header_type_name));
      if (insert_status.second == false) {
        LOG(WARNING) << "Packet IO metadata name " << metadata_name
                     << " is defined multiple times";
      }
    }
    std::vector<ParserExtractField> field_list;
    for (auto field : h_type->fields) {
      if (DecodePathEndField(*field, &field_list)) {
        StoreFieldTypeAnnotation(*field, header_type_name, &annotated_types);
        VLOG(1) << "Converting struct field name " << field->externalName()
                << " in " << header_type_name;
      } else {
        LOG(WARNING) << "Expected Type_Bits for field " << field->externalName()
                     << " in header type " << header_type_name;
      }
    }
    extracted_fields_per_type_[header_type_name] = field_list;
  }

  // This step iterates IR struct-like types to complete the entries in
  // extracted_fields_per_type_.  The IR typically represents metadata types
  // with struct-like nodes.
  for (auto s_like : struct_likes) {
    const std::string struct_type_name(s_like->name.toString());
    VLOG(1) << "Converting struct_like " << struct_type_name;
    std::vector<ParserExtractField> field_list;
    for (auto field : s_like->fields) {
      if (DecodePathEndField(*field, &field_list)) {
        StoreFieldTypeAnnotation(*field, struct_type_name, &annotated_types);
        // In addition to individual field names found within header_types,
        // the IR also defines field names in some struct_likes.
        VLOG(1) << "Converting struct field name " << field->externalName()
                << " in struct-like " << struct_type_name;
        continue;
      }
    }
    if (!field_list.empty())
      extracted_fields_per_type_[struct_type_name] = field_list;
  }

  // With the type of each header known plus the fields within each type,
  // the table_mapper_ can create field descriptors.
  for (auto iter : local_header_type_map) {
    auto find_iter = extracted_fields_per_type_.find(iter.second);
    if (find_iter != extracted_fields_per_type_.end()) {
      table_mapper_->AddHeader(iter.first);
      auto& type_fields = find_iter->second;
      for (auto& field_in_type : type_fields) {
        const std::string header_field_name =
            iter.first + "." + field_in_type.name();
        table_mapper_->AddField(header_field_name);
        UpdateFieldMapData(header_field_name, iter.second, field_in_type.name(),
                           annotated_types, field_in_type.bit_offset(),
                           field_in_type.bit_width());
        field_in_type.add_full_field_names(header_field_name);
        VLOG(1) << "Mapped header field name: " << header_field_name;
      }
    } else {
      LOG(WARNING) << "No known header fields for type " << iter.second;
    }
  }

  headers_done_ = true;
}

void FieldDecoder::ConvertMatchKeys(
    const std::vector<const IR::KeyElement*>& match_keys) {
  if (match_keys_done_) {
    LOG(INFO) << __func__ << " was called multiple times";
    return;
  }
  if (!headers_done_) {
    LOG(ERROR) << __func__ << " is unable to convert match keys with "
               << "no prior header decoding";
    return;
  }

  for (auto match_key : match_keys) {
    // The checks below assure that match_key has the expected structure
    // in the IR.
    if (!match_key->expression->is<IR::Member>()) {
      LOG(WARNING) << "Expected match_key expression " << match_key
                   << " to be an IR::Member";
      continue;
    }
    auto match_member = match_key->expression->to<IR::Member>();
    if (!match_member->type->is<IR::Type_Bits>()) {
      LOG(WARNING) << "Expected match_key expression type " << match_key
                   << " to be IR::Type_Bits";
      continue;
    }
    if (!match_key->matchType->type->is<IR::Type_MatchKind>()) {
      LOG(WARNING) << "Unexpected match type for " << match_key;
      continue;
    }
    auto match_member_type = match_member->type->to<IR::Type_Bits>();
    int match_field_width = match_member_type->width_bits();

    // The inspector extracts the field name of the match key and confirms
    // that the match expression is supported by the switch.
    FieldNameInspector header_inspector;
    header_inspector.ExtractName(*match_key->expression);
    const std::string match_field_key = header_inspector.field_name();
    if (match_field_key.empty()) {
      // TODO(teverman): Work out how to deal with unsupported match expressions
      //                 between this code and the FieldNameInspector.
      LOG(WARNING) << "Unable to map match key field name " << match_field_key;
      continue;
    }

    VLOG(1) << "Match Key " << match_field_key << " "
            << match_key->matchType->path->name.toString() << " width "
            << match_field_width;
    if (VLOG_IS_ON(2)) ::dump(match_key);

    // The table_mapper_ appends the match attributes to the field_descriptor.
    table_mapper_->AddFieldMatch(
        match_field_key,
        std::string(match_key->matchType->path->name.toString()),
        match_field_width);
  }

  match_keys_done_ = true;
}

bool FieldDecoder::DecodeP4Typedef(const IR::Type_Typedef& p4_typedef) {
  if (p4_typedef.type->is<IR::Type_Bits>()) {
    int type_width = p4_typedef.type->to<IR::Type_Bits>()->width_bits();
    path_end_types_[p4_typedef.externalName()] = type_width;
  } else {
    return false;
  }
  return true;
}

// Since enums have no specified size, they get assigned a width of 0
// to distinguish them from typedefs in path_end_types_.
bool FieldDecoder::DecodeP4Enum(const IR::Type_Enum& p4_enum) {
  if (p4_enum.members.empty())
    return false;
  path_end_types_[p4_enum.externalName()] = 0;
  return true;
}

// A field may be directly encoded as a bit field (IR::Type_Bits), or it may
// be based on some typedef that defines a bit field.  Enum-based fields are
// also treated as bit fields with width 0.  Enum and typedef fields are
// identified by their type's presence in path_end_types_.
bool FieldDecoder::DecodePathEndField(
    const IR::StructField& field,
    std::vector<ParserExtractField>* bit_field_list) {
  bool is_path_end = false;
  uint32 bit_width = 0;
  if (field.type->is<IR::Type_Bits>()) {
    is_path_end = true;
    bit_width = field.type->to<IR::Type_Bits>()->width_bits();
  } else if (field.type->is<IR::Type_Name>()) {
    auto typedef_name = field.type->to<IR::Type_Name>()->path->name.toString();
    const auto& iter = path_end_types_.find(typedef_name);
    if (iter != path_end_types_.end()) {
      is_path_end = true;
      bit_width = iter->second;
    }
  }

  if (is_path_end) {
    ParserExtractField bit_field;
    bit_field.set_name(field.externalName());
    bit_field.set_bit_width(bit_width);
    bit_field_list->push_back(bit_field);
  }

  return is_path_end;
}

void FieldDecoder::UpdateFieldMapData(
    const std::string& fq_field_name, const std::string& header_type_name,
    const std::string& field_name, const AnnotatedFieldTypeMap& annotated_types,
    uint32_t bit_offset, uint32_t bit_width) {
  AnnotatedFieldTypeMapKey key = std::make_pair(header_type_name, field_name);
  const auto& iter = annotated_types.find(key);

  // When field_type is P4_FIELD_TYPE_UNKNOWN, the table_mapper_ won't change
  // the type if no annotated type exists.
  P4FieldType field_type = P4_FIELD_TYPE_UNKNOWN;
  if (iter != annotated_types.end()) {
    field_type = iter->second;
  }
  table_mapper_->SetFieldAttributes(
      fq_field_name, field_type, P4_HEADER_UNKNOWN, bit_offset, bit_width);
  if (header_type_name == GetP4ModelNames().local_metadata_type_name()) {
    table_mapper_->SetFieldLocalMetadataFlag(fq_field_name);
  }
}

void FieldDecoder::StoreFieldTypeAnnotation(
    const IR::StructField& field, const std::string& header_type_name,
    AnnotatedFieldTypeMap* annotated_types) {
  P4Annotation p4_annotation;
  if (GetSwitchStackAnnotation(field, &p4_annotation) &&
      p4_annotation.field_type() != P4_FIELD_TYPE_UNKNOWN) {
    AnnotatedFieldTypeMapKey key = std::make_pair(
        header_type_name, std::string(field.externalName()));
    annotated_types->insert(std::make_pair(key, p4_annotation.field_type()));
  }
}

}  // namespace p4c_backend
}  // namespace hercules
}  // namespace google
