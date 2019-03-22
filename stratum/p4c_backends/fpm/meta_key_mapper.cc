// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "stratum/p4c_backends/fpm/meta_key_mapper.h"

#include "stratum/glue/logging.h"
#include "stratum/hal/lib/p4/p4_table_map.pb.h"
#include "stratum/p4c_backends/fpm/utils.h"

namespace stratum {
namespace p4c_backends {

void MetaKeyMapper::FindMetaKeys(const RepeatedP4InfoTables& p4_info_tables,
                                 TableMapGenerator* table_mapper) {
  for (const auto& p4_table : p4_info_tables) {
    for (const auto& match_field : p4_table.match_fields()) {
      const hal::P4FieldDescriptor* field_descriptor =
          FindFieldDescriptorOrNull(match_field.name(),
                                    table_mapper->generated_map());
      if (field_descriptor == nullptr) continue;
      if (!field_descriptor->is_local_metadata()) continue;
      hal::P4FieldDescriptor new_descriptor = *field_descriptor;
      new_descriptor.add_metadata_keys()->set_table_name(
          p4_table.preamble().name());
      table_mapper->ReplaceFieldDescriptor(match_field.name(), new_descriptor);
    }
  }
}

}  // namespace p4c_backends
}  // namespace stratum
