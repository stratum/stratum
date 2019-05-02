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

// This is a .p4 file that SliceCrossReferenceTest uses to verify various types
// of assignment statements with sliced field sources.

#include <core.p4>
#include <v1model.p4>

#include "control_stubs.p4"
#include "simple_headers.p4"

// This control contains actions to verify these forms of assignment statements:
// - Assigning literal constant to field.
// - Assigning the same constant to fields with different widths.
// - Assigning enum value constant to field.
// - Assigning action parameter to field.
// - Assigning the same action parameter to multiple fields.
// - Assigning a header field to another header field (of the same type).
// - Assigning an entire header to a matching header (header copy).
// - Assigning a slice of bits extracted from an action parameter.
// - Assigning a header field value which has been incremented.
// - Assigning an unsupported expression.
// Tests generally assert that the first statement in the action is an
// assignment.  Asserts may trigger mysteriously if an assignment expression
// is complex enough to make p4c simplify it in a midend transform.
control ingress(inout headers hdr, inout test_metadata_t meta,
                inout standard_metadata_t standard_metadata) {
  action assign_parameter_slice(bit<32> sliced_flags) {
    meta.smaller_metadata = sliced_flags[21:6];
  }
  action assign_metadata_slice() {
    meta.smaller_metadata = meta.other_metadata[31:16];
  }
  action assign_non_slice() {
    meta.color = 1;
  }
  action assign_field_slice_upper() {
    meta.other_metadata = hdr.ethernet.dstAddr[47:16];
  }
  action assign_field_slice_lower() {
    meta.smaller_metadata = hdr.ethernet.dstAddr[15:0];
  }
  action assign_field_slice_no_match() {
    meta.smaller_metadata = hdr.ethernet.dstAddr[31:16];
  }
  action assign_field_slice_to_field_slice() {
    meta.other_metadata[31:16] = hdr.ethernet.dstAddr[15:0];
  }
  @switchstack("pipeline_stage: VLAN_ACL")
  table acl_table {
    actions = {
      assign_parameter_slice;
      assign_metadata_slice;
      assign_non_slice;
      assign_field_slice_upper;
      assign_field_slice_lower;
      assign_field_slice_no_match;
      assign_field_slice_to_field_slice;
    }
    key = {
      hdr.ethernet.etherType: exact;
    }
  }
  apply {
    acl_table.apply();
  }
}

V1Switch(parser_stub(), verify_checksum_stub(), ingress(), egress_stub(),
         compute_checksum_stub(), deparser_stub()) main;
