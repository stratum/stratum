// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// The MetaKeyMapper looks for local metadata fields that appear at least once
// as a table match key.  It updates the P4PipelineConfig field descriptors
// of affected fields to indicate for which tables the field participates
// in the match key.

#ifndef STRATUM_P4C_BACKENDS_FPM_META_KEY_MAPPER_H_
#define STRATUM_P4C_BACKENDS_FPM_META_KEY_MAPPER_H_

#include "p4/config/v1/p4info.pb.h"
#include "stratum/hal/lib/p4/p4_pipeline_config.pb.h"
#include "stratum/p4c_backends/fpm/table_map_generator.h"
#include "stratum/public/proto/p4_table_defs.pb.h"

namespace stratum {
namespace p4c_backends {

// Aside from the constructor and destructor, a MetaKeyMapper instance has one
// public interface.  See the FindMetaKeys comments below for usage.
class MetaKeyMapper {
 public:
  typedef ::google::protobuf::RepeatedPtrField<::p4::config::v1::Table>
      RepeatedP4InfoTables;

  MetaKeyMapper() {}
  virtual ~MetaKeyMapper() {}

  // FindMetakeys looks at all the P4 tables in the p4_info_tables input,
  // which is generally the repeated tables field in the P4Info from the
  // pipeline configuration.  It evaluates the match fields for each table
  // against the P4PipelineConfig field descriptors being formed by the
  // table_mapper.  For each match field that is defined as local metadata
  // in the P4 program, FindMetakeys updates table_mapper's field descriptor
  // to indicate the field's usage as a part of the table match key.
  void FindMetaKeys(const RepeatedP4InfoTables& p4_info_tables,
                    TableMapGenerator* table_mapper);

  // MetaKeyMapper is neither copyable nor movable.
  MetaKeyMapper(const MetaKeyMapper&) = delete;
  MetaKeyMapper& operator=(const MetaKeyMapper&) = delete;
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // STRATUM_P4C_BACKENDS_FPM_META_KEY_MAPPER_H_
