// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// This is a .p4 file that ControlInspectorTest uses to verify miscellaneous
// statements that aren't covered by the other control test files.

#include <core.p4>
#include <v1model.p4>

#include "control_stubs.p4"
#include "simple_headers.p4"

// This control contains a meter decision that should become a NOP
// in the P4Control message output.
control control_nop_if(inout headers hdr, inout test_metadata_t meta,
                       inout standard_metadata_t standard_metadata) {
  action dummy_action() {
  }
  @switchstack("pipeline_stage: INGRESS_ACL")
  table table1 {
    actions = {
      dummy_action;
    }
    key = {
      hdr.ethernet.etherType: exact;
    }
  }
  apply {
    if (table1.apply().hit) {
      if (meta.enum_color == meter_color_t.COLOR_RED) {
        mark_to_drop(standard_metadata);
      }
    }
  }
}

// This control is equivalent to "meter_hit" in control_hit_meter.p4, except
// that the applied table here is in a hidden pipeline stage.
// TODO(unknown): Is it feasible for tests to add annotations to test IR
// nodes, eliminating the need for a separate test control?  This seems like
// a feature that would need to be implemented via a custom IR::Transform.
control meter_hit_hidden(inout headers hdr, inout test_metadata_t meta,
                         inout standard_metadata_t standard_metadata) {
  action hit1() {
  }
  action hit2() {
  }
  @switchstack("pipeline_stage: HIDDEN")
  table hidden_table {
    actions = {
      hit1;
      hit2;
    }
    key = {
      hdr.ethernet.dstAddr: exact;
      hdr.ethernet.etherType: exact;
    }
  }
  apply {
    if (hidden_table.apply().hit) {
      if (meta.enum_color == meter_color_t.COLOR_GREEN) {
        clone3(CloneType.I2E, 1024, {});
      }
    }
  }
}

V1Switch(parser_stub(), verify_checksum_stub(), control_nop_if(),
         meter_hit_hidden(), compute_checksum_stub(), deparser_stub()) main;
