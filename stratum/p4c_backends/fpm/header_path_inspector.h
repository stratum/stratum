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

// A HeaderPathInspector is a p4c Inspector subclass that visits the node
// hierarchy under an IR PathExpression to extract a header type, a header name,
// and any control parameter names, nested header names, or other qualifiers
// that prefix the header name. In other words, given an IR PathExpression that
// represents p.h.<f>, where <f> is a list of one or more fields in header h of
// type h_t, the HeaderPathInspector output includes the strings "p.h" for the
// path name (inclusive of the header name "h") and the type of h itself, "h_t".

#ifndef THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_HEADER_PATH_INSPECTOR_H_
#define THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_HEADER_PATH_INSPECTOR_H_

#include <deque>
#include <map>
#include <string>
#include "p4lang_p4c/frontends/p4/coreLibrary.h"

namespace stratum {
namespace p4c_backends {

// A single HeaderPathInspector instance operates on one IR::PathExpression to
// produce an output map associating header path names with header types.
// Typical usage is to construct a HeaderPathInspector, call the Inspect
// method with the PathExpression of interest, and then use the output provided
// by the path_to_header_type_map accessor.
// TODO: Given current usage where SwitchP4cBackend accumulates output
// from multiple HeaderPathInspector instances, it may be more optimum to allow
// the Inspect method to run repeatedly in one HeaderPathInspector instance.
class HeaderPathInspector : public Inspector {
 public:
  // A HeaderPathInspector creates a PathToHeaderTypeMap as its output.  The
  // key is the header path name, and the value is the header type name.
  // Examples from tor.p4:
  // - hdr.ethernet -> ethernet_t
  // - hdr.ipv4 -> ipv4_t
  // In cases where ignored_path_prefixes contains "hdr" for P4_14 support:
  // - ethernet -> ethernet_t
  // - ipv4 -> ipv4_t
  // For nested header and metadata types, the output map contains multiple
  // entries per expression, with each entry referring to the type at the
  // end of the path name:
  // - meta.m_outer.m_inner -> m_inner_t
  // - meta.m_outer -> m_outer_t
  // For stacked headers, the output map contains one entry per valid stack
  // index:
  // - hdr.vlan_tag[0] -> vlan_tag_t
  // - hdr.vlan_tag[1] -> vlan_tag_t
  typedef std::map<std::string, std::string> PathToHeaderTypeMap;

  // The shared instance of P4ModelNames should be setup before calling the
  // constructor.  It should contain any prefixes to ignore in the header path
  // via the strip_path_prefixes field.  Its typical usage is for P4_14 program
  // compatibility, where the p4c IR internally adds the "hdr" prefix, but the
  // prefix does not appear externally in the P4Info output.
  HeaderPathInspector();

  // Applies the p4c Inspector methods to the input expression.  The
  // HeaderPathInspector expects the input to have type IR::Type_Struct.
  // Inspect only runs once per HeaderPathInspector.  Upon successful return,
  // the mapped output is available via the path_to_header_type_map accessor.
  bool Inspect(const IR::PathExpression& expression);

  // These methods override the IR::Inspector class to visit the node
  // hierarchy under a PathExpression.
  bool preorder(const IR::PathExpression* path) override;
  bool preorder(const IR::Type_Header* header) override;
  bool preorder(const IR::Type_HeaderUnion* header_union) override;
  bool preorder(const IR::StructField* field) override;
  bool preorder(const IR::Type_Struct* field) override;

  // Accessor for outputs.
  const PathToHeaderTypeMap& path_to_header_type_map() const {
    return path_to_header_type_map_;
  }

  // HeaderPathInspector is neither copyable nor movable.
  HeaderPathInspector(const HeaderPathInspector&) = delete;
  HeaderPathInspector& operator=(const HeaderPathInspector&) = delete;

 private:
  // This struct saves path expression context as the Inspector visits the
  // node hierarchy under the PathExpression:
  // header_name - records the name of the path field at the current IR
  //     context depth.
  // header_type - records the type of the path field at the current IR
  //     context depth.
  // depth - records the context level of the IR for this entry, as provided
  //     by getContextDepth().
  // Example: when processing the ethernet header, the HeaderPathInspector
  // has two active PathContextEntrys in a context stack.  The first entry
  // has {header_name="hdr", header_type="headers", depth=1} and the second
  // entry has {header_name="ethernet", header_type="ethernet_t", depth=3}.
  // (The depth values are not necessarily sequential, but they are always
  // mononically increasing.)
  struct PathContextEntry {
    PathContextEntry() : depth(0) {}

    std::string header_name;
    std::string header_type;
    int depth;
  };

  // Adds path_to_header_type_map_ entries for the current path context stack.
  void MapPathsToHeaderType();

  // Pushes a new path context stack entry representing the input path_name.
  void PushPathContext(const std::string& path_name);

  // Pops all path context stack entries up to and including the current
  // context depth reported by the IR.
  void PopPathContexts();

  // Updates the PathContextEntry at the top of the context stack with the
  // given header_type.
  void UpdatePathHeaderType(const std::string& header_type);

  // Returns a string representing the header names in the path context stack
  // in the form "<h0>.<h1>...<hN>", where <hN> represents the header_name at
  // the corresponding stack depth.
  std::string GetPathString();

  // The path_to_header_type_map_ accumulates the output as the Inspect method
  // visits child nodes.
  PathToHeaderTypeMap path_to_header_type_map_;

  // The path_context_stack_ tracks the PathExpression node hierarchy as the
  // Inspector runs through its node visitation sequence.
  std::deque<PathContextEntry> path_context_stack_;

  // Records the size of a header stack if the Inspector encounters a stacked
  // header type.
  int header_stack_size_;
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_HEADER_PATH_INSPECTOR_H_
