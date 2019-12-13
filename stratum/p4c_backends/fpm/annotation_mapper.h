/*
 * Copyright 2019 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// The AnnotationMapper is a Stratum switch p4c backend class that runs as
// the final step of P4PipelineConfig output.  It supplements the config's
// table map with any data that can only be determined from annotations and
// name strings within the P4Info objects.

#ifndef STRATUM_P4C_BACKENDS_FPM_ANNOTATION_MAPPER_H_
#define STRATUM_P4C_BACKENDS_FPM_ANNOTATION_MAPPER_H_

#include <map>
#include <string>
#include <unordered_map>
#include "stratum/glue/logging.h"
#include "stratum/hal/lib/p4/p4_info_manager.h"
#include "stratum/hal/lib/p4/p4_pipeline_config.pb.h"
#include "stratum/p4c_backends/fpm/annotation_map.pb.h"
#include "stratum/public/proto/p4_annotation.pb.h"
#include "absl/container/node_hash_map.h"

namespace stratum {
namespace p4c_backends {

class AnnotationMapper {
 public:
  AnnotationMapper();
  virtual ~AnnotationMapper() {}

  // An AnnotationMapper has two ways to initialize:
  // Init() - reads the P4AnnotationMap from files specified by command-line
  //          flag.  This is the normal Init method for production switch setup.
  //          The annotations files can be tuned by switch type, i.e TOR vs.
  //          spine, etc.
  // InitFromP4AnnotationMap() - takes the P4AnnotationMap from the input
  //          parameter.  This Init method is convenient for tests that need
  //          special P4AnnotationMap setup.  It may also be useful in cases
  //          where the P4AnnotationMap is simple enough to be built in to the
  //          backend code.
  // Both methods return true if properly initialized.  The two methods are
  // mutually exclusive.
  virtual bool Init();
  virtual bool InitFromP4AnnotationMap(const P4AnnotationMap& annotation_map);

  // The AnnotationMapper traverses the action, table, and field descriptors
  // in the p4_pipeline_config as well as P4Info objects managed by the
  // p4_info_manager.  For each object, it searches for P4AnnotationMap matches
  // with the object's annotation strings and name.  Upon finding a match, it
  // updates the corresponding P4 table map descriptor in p4_pipeline_config
  // with the matching information.  ProcessAnnotations also directly interprets
  // the "@switchstack" annotations it finds in the P4Info and updates the
  // p4_pipeline_config descriptors as indicated; no matching P4AnnotationMap
  // entry is expected for these annotations.  ProcessAnnotations returns
  // true upon successful completion of annotation processing.  It returns
  // false if this instance is uninitialized, if it finds some anomaly between
  // the input data and the P4AnnotationMap data, or if it fails to parse
  // any "@switchstack" annotations.
  //
  // Under ideal circumstances, where the previous p4c backend stages have done
  // a thorough table mapping job, ProcessAnnotations will have nothing to do.
  // No P4 object annotations or names will match any P4AnnotationMap entries,
  // and the output p4_pipeline_config will be unchanged.  ProcessAnnotations
  // considers these circumstances to be a success, and it returns true.
  //
  // In typical circumstances, annotation mapping is required for some subset
  // of P4 objects that need special treatment.  ProcessAnnotations does not
  // expect every P4 object to be annotated, so it does not report errors for
  // P4 objects that fail to yield a P4AnnotationMap match.
  virtual bool ProcessAnnotations(const hal::P4InfoManager& p4_info_manager,
                                  hal::P4PipelineConfig* p4_pipeline_config);

  // Accesses the initialized P4AnnotationMap.
  const P4AnnotationMap& annotation_map() const { return annotation_map_; }

  // AnnotationMapper is neither copyable nor movable.
  AnnotationMapper(const AnnotationMapper&) = delete;
  AnnotationMapper& operator=(const AnnotationMapper&) = delete;

 private:
  // This private class maintains mappings from field/table/action addenda
  // name to the corresponding addenda data in the P4AnnotationMap.
  template <class T> class AddendaLookupMap {
   public:
    AddendaLookupMap() {}
    virtual ~AddendaLookupMap() {}

    // Populates the addenda_lookup_ map for this instance.
    bool BuildMap(
      const ::google::protobuf::RepeatedPtrField<T>& addenda_fields) {
      bool map_ok = true;
      for (const auto& addendum : addenda_fields) {
        if (!addendum.name().empty()) {
          auto result = addenda_lookup_.emplace(addendum.name(), &addendum);
          if (!result.second) {
            LOG(ERROR) << "P4AnnotationMap unexpected duplicate addenda name "
                       << addendum.name();
            map_ok = false;
          }
        } else {
          LOG(ERROR) << "P4AnnotationMap addenda is missing name "
                     << addendum.DebugString();
          map_ok = false;
        }
      }
      return map_ok;
    }

    const T* FindAddenda(const std::string& addenda_name) {
      const auto iter = addenda_lookup_.find(addenda_name);
      if (iter == addenda_lookup_.end()) {
        LOG(ERROR) << "Addenda name " << addenda_name << " has no lookup entry";
        return nullptr;
      }
      return iter->second;
    }

   private:
    // Maps the field/table/action addenda name to the corresponding
    // P4AnnotationMap entry.
    absl::node_hash_map<std::string, const T*> addenda_lookup_;
  };

  bool InitInternal();  // Common initialization for both public Init methods.

  // These two methods process any annotations in the given field descriptor,
  // adjusting the field_descriptor as specified by any annotation mappings
  // found.
  bool HandleFieldAnnotations(const std::string& field_name,
                              hal::P4FieldDescriptor* field_descriptor);
  bool MapFieldAnnotation(const std::string& annotation,
                          hal::P4FieldDescriptor* field_descriptor);

  // These two methods process any annotations in the given table descriptor,
  // adjusting the table_descriptor as specified by any annotation mappings
  // found.
  bool HandleTableAnnotations(const std::string& table_name,
                              const hal::P4InfoManager& p4_info_manager,
                              hal::P4TableDescriptor* table_descriptor);
  bool MapTableAnnotation(const std::string& annotation,
                          hal::P4TableDescriptor* table_descriptor);

  // These two methods process any annotations in the given action descriptor,
  // adjusting the action_descriptor as specified by any annotation mappings
  // found.
  bool HandleActionAnnotations(const std::string& action_name,
                              const hal::P4InfoManager& p4_info_manager,
                              hal::P4ActionDescriptor* action_descriptor);
  bool MapActionAnnotation(const std::string& annotation,
                           hal::P4ActionDescriptor* action_descriptor);

  bool initialized_;  // Becomes true after one of the init methods runs.

  // This P4AnnotationMap contains the initialized annotations mapping data
  // from input text files or InitFromP4AnnotationMap input.
  P4AnnotationMap annotation_map_;

  // These maps assist in the annotation lookup process when AnnotationMapper
  // needs to find annotation_map_ entries.
  AddendaLookupMap<P4FieldAddenda> field_lookup_;
  AddendaLookupMap<P4TableAddenda> table_lookup_;
  AddendaLookupMap<P4ActionAddenda> action_lookup_;

  // This map keeps track of forwarding pipeline stage usage based on
  // the @switchstack annotations.  The map value is true if at least one
  // pipeline_stage annotation refers to the map key's stage.
  std::map<P4Annotation::PipelineStage, bool> pipeline_stage_usage_;
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // STRATUM_P4C_BACKENDS_FPM_ANNOTATION_MAPPER_H_
