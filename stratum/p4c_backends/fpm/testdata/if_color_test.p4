// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

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
      mark_to_drop(standard_metadata);
    }
    if (meta.enum_color != meter_color_t.COLOR_GREEN) {
      mark_to_drop(standard_metadata);
    }
    if (meter_color_t.COLOR_RED == meta.enum_color) {
      mark_to_drop(standard_metadata);
    }
    if (meter_color_t.COLOR_RED != meta.enum_color) {
      mark_to_drop(standard_metadata);
    }

    // IfStatementColorInspector expects the p4c frontend and midend to
    // simplify these conditions into one of the forms above.
    if (!(meta.enum_color == meter_color_t.COLOR_YELLOW)) {
      mark_to_drop(standard_metadata);
    }
    if (!(meta.enum_color != meter_color_t.COLOR_YELLOW)) {
      mark_to_drop(standard_metadata);
    }

    // These statements appear in the IR as one IfStatement with the else
    // condition under the ifFalse block.  The tests can't see the else
    // clause to test.
    if (meta.enum_color == meter_color_t.COLOR_YELLOW) {
      mark_to_drop(standard_metadata);
    } else if (meta.enum_color == meter_color_t.COLOR_RED) {
      exit;
    }
  }
}

// This control contains IfStatements with assorted conditions that Stratum
// should accept without doing any meter color transforms.
control ifs_with_no_transforms(inout headers hdr, inout test_metadata_t meta,
                               inout standard_metadata_t standard_metadata) {
  action drop_packet() {
    mark_to_drop(standard_metadata);
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
      mark_to_drop(standard_metadata);
    }
    if (!hdr.ethernet.isValid()) {
      mark_to_drop(standard_metadata);
    }
    if (meta.other_metadata == 32w1) {
      mark_to_drop(standard_metadata);
    }
    if (meta.other_metadata != 32w2) {
      mark_to_drop(standard_metadata);
    }
    if (hdr.ethernet.etherType == 16w800) {
      mark_to_drop(standard_metadata);
    }
    if (hdr.ethernet.etherType != 16w900) {
      mark_to_drop(standard_metadata);
    }
    if (hdr.ethernet.etherType == meta.smaller_metadata) {
      mark_to_drop(standard_metadata);
    }
    if (hdr.ethernet.etherType != meta.smaller_metadata) {
      mark_to_drop(standard_metadata);
    }
    if (hdr.ethernet.etherType == 16w100 || hdr.ethernet.etherType == 16w200) {
      mark_to_drop(standard_metadata);
    }
    if (hdr.ethernet.etherType != 16w100 && hdr.ethernet.etherType != 16w200) {
      mark_to_drop(standard_metadata);
    }

    // p4c transforms each of these into two statements, assigning the hit
    // outcome to a temporary variable and replacing the IfStatement condition
    // with the temporary.  This needs to be taken into consideration when
    // assigning test parameters that reference these statements.
    if (!test_table.apply().hit) {
      mark_to_drop(standard_metadata);
    }
    if (test_table.apply().hit) {
      mark_to_drop(standard_metadata);
    }
  }
}

V1Switch(parser_stub(), compute_checksum_stub(), ifs_with_transforms(),
         ifs_with_no_transforms(), compute_checksum_stub(),
         deparser_stub()) main;
