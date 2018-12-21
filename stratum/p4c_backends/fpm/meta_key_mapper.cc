#include "platforms/networking/hercules/p4c_backend/switch/meta_key_mapper.h"

#include "base/logging.h"
#include "platforms/networking/hercules/hal/lib/p4/p4_table_map.host.pb.h"
#include "platforms/networking/hercules/p4c_backend/switch/utils.h"

namespace google {
namespace hercules {
namespace p4c_backend {

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

}  // namespace p4c_backend
}  // namespace hercules
}  // namespace google
