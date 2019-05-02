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

// This is a .p4 file that ActionDecoderTest and ExpressionInspectorTest use to
// verify various types of assignment statements and expressions.

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
  action assign_constant() {
    meta.color = 1;
  }
  action assign_constant_multi_width() {
    meta.other_metadata = 123;
    meta.smaller_metadata = 123;
  }
  action assign_enum() {
    meta.enum_color = meter_color_t.COLOR_YELLOW;
  }
  action assign_param(bit<48> dmac) {
    hdr.ethernet.dstAddr = dmac;
  }
  action assign_param_multiple(bit<48> mac) {
    hdr.ethernet.dstAddr = mac;
    hdr.ethernet.srcAddr = mac;
  }
  action assign_field_to_field() {
    hdr.ethernet.dstAddr = hdr.ethernet.srcAddr;
  }
  action assign_header_copy() {
    hdr.ethernet2 = hdr.ethernet;
  }
  action assign_parameter_slice(bit<16> sliced_flags) {
    meta.other_metadata[1:0] = sliced_flags[4:3];
  }
  action assign_metadata_slice(bit<16> sliced_flags) {
    meta.smaller_metadata = meta.other_metadata[31:16];
  }
  action assign_add(bit<48> dmac) {
    hdr.ethernet.dstAddr = dmac + 1;
  }
  action assign_header_stack() {
    hdr.ethernet_stack[1] = hdr.ethernet_stack[0];
  }
  action assign_non_const_array_index() {
    hdr.ethernet_stack[1] = hdr.ethernet_stack[meta.color];
  }
  action assign_temp_array() {
    ethernet_t[1] temp_stack;
    // This action generates a compiler warning because temp_stack[0] is not
    // initialized, but the statement below needs to be first in the action
    // in order to be recognized by the unit test.
    hdr.ethernet_stack[1] = temp_stack[0];
  }
  action assign_unsupported(bit<32> param) {
    meta.other_metadata = -param;  // Negate (IR::Neg) is unsupported.
  }
  @switchstack("pipeline_stage: VLAN_ACL")
  table acl_table {
    actions = {
      assign_constant;
      assign_constant_multi_width;
      assign_enum;
      assign_param;
      assign_param_multiple;
      assign_field_to_field;
      assign_header_copy;
      assign_parameter_slice;
      assign_metadata_slice;
      assign_add;
      assign_header_stack;
      assign_non_const_array_index;
      assign_temp_array;
      assign_unsupported;
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
