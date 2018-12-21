// This is a .p4 file that MeterColorMapperTest uses to test transforms
// of IR::IfStatements with meter color conditions.  It involves IfStatement
// variations of if-else sequences.

#include <core.p4>
#include <v1model.p4>

#include "control_stubs.p4"
#include "simple_headers.p4"

// Tests a valid MeterColorStatement transform of this form:
//  if (color-condition) {
//    true-condition-operation;
//  } else {
//    false-condition-operation;
//  }
control meter_if_else(inout headers hdr, inout test_metadata_t meta,
                      inout standard_metadata_t standard_metadata) {
  apply {
    if (meta.enum_color == meter_color_t.COLOR_GREEN) {
      clone3(CloneType.I2E, 1024, {});
    } else {
      mark_to_drop();
    }
  }
}

// Tests an unsupported MeterColorStatement transform of this form:
//  if (color-condition) {
//    true-condition-operation;
//    unsupported-operation;
//  } else {
//    false-condition-operation;
//  }
control meter_if_else_true_bad(inout headers hdr, inout test_metadata_t meta,
                               inout standard_metadata_t standard_metadata) {
  apply {
    if (meta.enum_color == meter_color_t.COLOR_GREEN) {
      clone3(CloneType.I2E, 1024, {});
      meta.smaller_metadata = 1;  // Unsupported.
    } else {
      mark_to_drop();
    }
  }
}

// Tests an unsupported MeterColorStatement transform of this form:
//  if (color-condition) {
//    true-condition-operation;
//  } else {
//    false-condition-operation;
//    unsupported-operation;
//  }
control meter_if_else_false_bad(inout headers hdr, inout test_metadata_t meta) {
  apply {
    if (meta.enum_color == meter_color_t.COLOR_GREEN) {
      clone3(CloneType.I2E, 1024, {});
    } else {
      mark_to_drop();
      meta.smaller_metadata = 1;  // Unsupported.
    }
  }
}

// Tests an unsupported MeterColorStatement transform of this form:
//  if (color-condition1) {
//    true-condition1-operation;
//  } else if (color-condition2) {
//    true-condition2-operation;
//  } else {
//    false-condition1-operation;
//  }
// Note: In the p4c IR, this becomes equivalent to:
//  if (color-condition1) {
//    true-condition1-operation;
//  } else {
//    if (color-condition2) {
//      true-condition2-operation;
//    } else {
//      false-condition1-operation;
//    }
//  }
control meter_if_elseif_else(inout headers hdr, inout test_metadata_t meta) {
  apply {
    if (meta.enum_color == meter_color_t.COLOR_GREEN) {
      clone3(CloneType.I2E, 1024, {});
    } else if (meta.enum_color == meter_color_t.COLOR_YELLOW) {
      clone3(CloneType.I2E, 1025, {});
    } else {
      mark_to_drop();
    }
  }
}

V1Switch(parser_stub(), meter_if_else_false_bad(), meter_if_else(),
         meter_if_else_true_bad(), meter_if_elseif_else(),
         deparser_stub()) main;
