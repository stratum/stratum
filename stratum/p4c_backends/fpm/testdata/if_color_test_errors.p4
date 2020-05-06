// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// This is a .p4 file that MeterColorMapperTest uses to verify transforms
// of IR::IfStatements with meter color conditions.  This file contains
// IfStatements that refer to meter colors, but the conditional expressions
// are unsupported by a Stratum switch.

#include <core.p4>
#include <v1model.p4>

#include "control_stubs.p4"
#include "simple_headers.p4"

// This test extern takes a meter_color_t as input and returns another
// meter_color_t value.
extern meter_color_t color_function(meter_color_t color);

// The IfStatements in this control are for tests that verify
// unsupported transforms of meter color conditions.
control ifs_with_errors(inout headers hdr, inout test_metadata_t meta,
                        inout standard_metadata_t standard_metadata) {
  // Initialization of the local variable becomes the first statement in the
  // apply block, so all offsets to IfStatements in the parameterized tests
  // need to be adjusted accordingly.
  meter_color_t local_color = meter_color_t.COLOR_RED;
  apply {
    // Condition is too complex.
    if (meta.enum_color == meter_color_t.COLOR_GREEN ||
        meta.enum_color == meter_color_t.COLOR_YELLOW) {
      mark_to_drop(standard_metadata);
      local_color = meta.enum_color;
    }

    // This one is unsupported, but in theory it should be possible for it to
    // work.  No use cases currently exist in Stratum P4 programs.  For the
    // local_color variable in the IR, the "left" of the condition goes
    // directly to a PathExpression, whereas meta.enum_color goes indirectly
    // to a PathExpression via an IR::Member.
    if (local_color == meter_color_t.COLOR_GREEN) {
      mark_to_drop(standard_metadata);
    }

    // Condition does not allow Stratum to determine the color in effect.
    if (local_color == meta.enum_color) {
      mark_to_drop(standard_metadata);
    }

    // The frontent transforms the extern call into a temporary variable
    // assignment, so this statement becomes equivalent to the one above.
    // Any calls to externs that potentially translate colors in an unknown
    // way should probably be rejected.
    //  if (color_function(meter_color_t.COLOR_RED) == meta.enum_color) {
    //    mark_to_drop(standard_metadata);
    //  }

    // The p4c frontend rejects these before they get to the backend,
    // so they can't be test inputs:
    //  if (meta.enum_color)
    //  if (meter_color_t.COLOR_GREEN)
    //  if (meta.enum_color == (meter_color_t.COLOR_GREEN + 1))
    //  if ((meta.enum_color + 1) == meter_color_t.COLOR_GREEN)
    //  if (meta.enum_color > meter_color_t.COLOR_GREEN)
    //  if (meta.smaller_metadata == meter_color_t.COLOR_GREEN)
  }
}

V1Switch(parser_stub(), compute_checksum_stub(), ifs_with_errors(),
         egress_stub(), compute_checksum_stub(), deparser_stub()) main;
