// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// The FieldDecoder processes IR nodes related to header types, header fields,
// and match fields.  It adds table map FieldDescriptor data to the backend's
// output P4PipelineConfig.  It also provides some decoded output for
// subsequent use in parser field mapping.

#ifndef STRATUM_P4C_BACKENDS_FPM_FIELD_DECODER_H_
#define STRATUM_P4C_BACKENDS_FPM_FIELD_DECODER_H_

#include <map>
#include <string>
#include <vector>
#include <utility>

#include "stratum/p4c_backends/fpm/header_path_inspector.h"
#include "stratum/p4c_backends/fpm/parser_map.pb.h"
#include "stratum/p4c_backends/fpm/table_map_generator.h"
#include "external/com_github_p4lang_p4c/ir/ir.h"

namespace stratum {
namespace p4c_backends {

class FieldDecoder {
 public:
  // This map provides FieldDecoder output that the ParserFieldMapper uses
  // to provide header field details in its own output.  The key is the
  // header field type from the P4 program, such as "ethernet_t".  The
  // value contains names and bit widths for individual fields.  The field
  // order in the vector is the same as the order of the fields relative
  // to the start of the header.
  typedef std::map<std::string, std::vector<ParserExtractField>>
      DecodedHeaderFieldMap;

  // The table_mapper handles the conversion of decoded IR data into
  // field descriptors in the P4PipelineConfig table map.  The caller retains
  // pointer ownership.  The shared instance of P4ModelNames should be setup
  // before calling the constructor.
  explicit FieldDecoder(TableMapGenerator* table_mapper);
  virtual ~FieldDecoder() {}

  // Converts the header fields represented by the IR inputs into field
  // descriptor entries in the P4PipelineConfig table map.  The inputs come
  // from initial IR ProgramInspector and HeaderPathInspector passes.
  // ConvertHeaderFields also accumulates header field data for later merging
  // with data about parser-extracted headers.  This data is available via
  // the extracted_fields_per_type() accessor upon return from this method.
  void ConvertHeaderFields(
      const std::vector<const IR::Type_Typedef*>& p4_typedefs,
      const std::vector<const IR::Type_Enum*>& p4_enums,
      const std::vector<const IR::Type_StructLike*>& struct_likes,
      const std::vector<const IR::Type_Header*>& header_types,
      const HeaderPathInspector::PathToHeaderTypeMap& path_to_header_type_map);

  // Processes the input IR match_keys, determines which field they reference,
  // and updates the corresponding P4PipelineConfig table map field descriptor
  // with mapping data to use for matching the field at switch runtime.
  // ConvertMatchKeys expects to find field descriptors written by
  // ConvertHeaderFields in the table_mapper_ output.
  void ConvertMatchKeys(const std::vector<const IR::KeyElement*>& match_keys);

  const DecodedHeaderFieldMap& extracted_fields_per_type() const {
    return extracted_fields_per_type_;
  }

  // FieldDecoder is neither copyable nor movable.
  FieldDecoder(const FieldDecoder&) = delete;
  FieldDecoder& operator=(const FieldDecoder&) = delete;

 private:
  // These types manage P4 field type indications from @switchstack annotations.
  // The AnnotatedFieldTypeMap maps from a <header-type, field-name> string pair
  // to the annotated field type.
  typedef std::pair<std::string, std::string> AnnotatedFieldTypeMapKey;
  typedef std::map<AnnotatedFieldTypeMapKey, P4FieldType> AnnotatedFieldTypeMap;

  // Evaluates P4 program typedefs, restricting them to types that define
  // bit fields, and storing valid types in path_end_types_.
  bool DecodeP4Typedef(const IR::Type_Typedef& p4_typedef);

  // Evaluates P4 program enums, restricting them to types that define
  // at least one member, and storing valid types in path_end_types_.
  bool DecodeP4Enum(const IR::Type_Enum& p4_enum);

  // Checks whether the input field is a simple type that terminates the
  // header field path.  Simple types are IR::Type_Bits, IR::Typedef, and
  // IR::Type_Enum, none of which can define additional fields that extend
  // the header path name.  If the type qualifies, DecodePathEndField appends
  // bit_field_list with an entry containing the field's name and bit width,
  // and then it returns true.  If the input field is some other IR type,
  // the return value is false, and bit_field_list is unchanged.
  bool DecodePathEndField(const IR::StructField& field,
                          std::vector<ParserExtractField>* bit_field_list);

  // Updates the table_mapper_ field descriptor with data from the inputs.
  // If the input field has a field type annotation, UpdateFieldMapData
  // includes the annotated type in the table_mapper_ changes.  The fully-
  // qualified field name (e.g. the lookup key for the field descriptor) is
  // given by fq_field_name, header_type_name identifies the field's header
  // type (e.g. "packet_in_header_t"), field_name is the name of the field
  // within the header type (e.g. "ingress_physical_port"), annotated_types
  // contains field type annotations stored by StoreFieldTypeAnnotation, and
  // bit_offset and bit_width describe the field's position with its
  // surrounding header.
  void UpdateFieldMapData(const std::string& fq_field_name,
                          const std::string& header_type_name,
                          const std::string& field_name,
                          const AnnotatedFieldTypeMap& annotated_types,
                          uint32_t bit_offset, uint32_t bit_width,
                          const std::string& parent_header_key);

  // Determines whether the input field has an annotated field type.  If the
  // annotation exists, StoreFieldTypeAnnotation parses the field type and
  // stores it in annotated_types for future use.
  static void StoreFieldTypeAnnotation(
      const IR::StructField& field, const std::string& header_type_name,
      AnnotatedFieldTypeMap* annotated_types);

  // Accumulates decoded IR field objects in the output table map, injected
  // by the caller of the constructor, not owned by this class.
  TableMapGenerator* table_mapper_;

  // This map accumulates a list of per-field data extracted for each header
  // type.  FieldDecoder stores this data so ParserFieldMapper can combine it
  // with ParserDecoder output.
  DecodedHeaderFieldMap extracted_fields_per_type_;

  // This map contains "simple" types for fields that terminate the header path.
  // Most such fields are defined as IR::Type_Bits, which directly specifies the
  // field width.  For fields typed as IR::Type_Typedef, this map provides
  // an indirection from the name of the typedef to the width when the typedef
  // itself is IR::Type_Bits.  For fields typed as IR::Type_Enum, this map
  // provides an internally-defined bit width.
  std::map<cstring, int> path_end_types_;

  bool headers_done_;     // Becomes true after ConvertHeaderFields runs.
  bool match_keys_done_;  // Becomes true after ConvertMatchKeys runs.
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // STRATUM_P4C_BACKENDS_FPM_FIELD_DECODER_H_
