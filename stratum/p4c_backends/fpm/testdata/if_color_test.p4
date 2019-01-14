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

// This is a .p4 file that MeterColorMapperTest uses to test transforms
// of IR::IfStatements with meter color conditions.

#include <core.p4>
#include <v1model.p4>

#include "control_stubs.p4"
#include "simple_headers.p4"

// The IfStatements in this control are for tests that verify
// valid transforms of meter color conditions.
control ifs_with_transforms(inout headers hdr, inout test_metadata_t meta,
                            inout standard_metadata_t standard_metadata) {
  apply {
    // The first four statements represent the basic color conditions
    // allowed by MeterColorMapper and IfStatementColorInspector.
    if (meta.enum_color == meter_color_t.COLOR_GREEN) {
      mark_to_drop();
    }
    if (meta.enum_color != meter_color_t.COLOR_GREEN) {
      mark_to_drop();
    }
    if (meter_color_t.COLOR_RED == meta.enum_color) {
      mark_to_drop();
    }
    if (meter_color_t.COLOR_RED != meta.enum_color) {
      mark_to_drop();
    }

    // IfStatementColorInspector expects the p4c frontend and midend to
    // simplify these conditions into one of the forms above.
    if (!(meta.enum_color == meter_color_t.COLOR_YELLOW)) {
      mark_to_drop();
    }
    if (!(meta.enum_color != meter_color_t.COLOR_YELLOW)) {
      mark_to_drop();
    }

    // These statements appear in the IR as one IfStatement with the else
    // condition under the ifFalse block.  The tests can't see the else
    // clause to test.
    if (meta.enum_color == meter_color_t.COLOR_YELLOW) {
      mark_to_drop();
    } else if (meta.enum_color == meter_color_t.COLOR_RED) {
      exit;
    }
  }
}

// This control contains IfStatements with assorted conditions that Hercules
// should accept without doing any meter color transforms.
control ifs_with_no_transforms(inout headers hdr, inout test_metadata_t meta,
                               inout standard_metadata_t standard_metadata) {
  action drop_packet() {
    mark_to_drop();
  }
  table test_table {
    key = {
      hdr.ethernet.etherType : exact;
    }
    actions = {
      drop_packet;
    }
  }
  apply {
    if (hdr.ethernet.isValid()) {
      mark_to_drop();
    }
    if (!hdr.ethernet.isValid()) {
      mark_to_drop();
    }
    if (meta.other_metadata == 32w1) {
      mark_to_drop();
    }
    if (meta.other_metadata != 32w2) {
      mark_to_drop();
    }
    if (hdr.ethernet.etherType == 16w800) {
      mark_to_drop();
    }
    if (hdr.ethernet.etherType != 16w900) {
      mark_to_drop();
    }
    if (hdr.ethernet.etherType == meta.smaller_metadata) {
      mark_to_drop();
    }
    if (hdr.ethernet.etherType != meta.smaller_metadata) {
      mark_to_drop();
    }
    if (hdr.ethernet.etherType == 16w100 || hdr.ethernet.etherType == 16w200) {
      mark_to_drop();
    }
    if (hdr.ethernet.etherType != 16w100 && hdr.ethernet.etherType != 16w200) {
      mark_to_drop();
    }

    // p4c transforms each of these into two statements, assigning the hit
    // outcome to a temporary variable and replacing the IfStatement condition
    // with the temporary.  This needs to be taken into consideration when
    // assigning test parameters that reference these statements.
    if (!test_table.apply().hit) {
      mark_to_drop();
    }
    if (test_table.apply().hit) {
      mark_to_drop();
    }
  }
}

V1Switch(parser_stub(), compute_checksum_stub(), ifs_with_transforms(),
         ifs_with_no_transforms(), compute_checksum_stub(),
         deparser_stub()) main;
