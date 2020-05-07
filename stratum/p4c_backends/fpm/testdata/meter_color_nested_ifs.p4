// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// This is a .p4 file that MeterColorMapperTest uses to test transforms
// of IR::IfStatements with meter color conditions.  It involves nested
// IR::IfStatement tests.

#include <core.p4>
#include <v1model.p4>

#include "control_stubs.p4"
#include "simple_headers.p4"

// Tests a meter condition validly nested inside another if.
control meter_if_in_if(inout headers hdr, inout test_metadata_t meta,
                       inout standard_metadata_t standard_metadata) {
  action clone_green() {
  }
  action drop_not_green() {
  }
  @switchstack("pipeline_stage: INGRESS_ACL")
  table meter_table {
    actions = {
      clone_green;
      drop_not_green;
    }
    key = {
      hdr.ethernet.etherType: exact;
    }
  }
  apply {
    if (meter_table.apply().hit) {
      if (meta.enum_color == meter_color_t.COLOR_GREEN) {
        clone3(CloneType.I2E, 1024, {});
      }
    }
  }
}

// Tests a second IfStatement nested inside an IfStatement that acts on
// a meter condition, which is unsupported by the Stratum backend.
control if_in_meter_if(inout headers hdr, inout test_metadata_t meta,
                       inout standard_metadata_t standard_metadata) {
  apply {
    if (meta.enum_color != meter_color_t.COLOR_GREEN) {
      clone3(CloneType.I2E, 1024, {});
      if (meta.enum_color == meter_color_t.COLOR_RED) {
        mark_to_drop(standard_metadata);
      }
    }
  }
}

V1Switch(parser_stub(), verify_checksum_stub(), meter_if_in_if(),
         if_in_meter_if(), compute_checksum_stub(), deparser_stub()) main;
