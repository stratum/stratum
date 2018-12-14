// This is a .p4 file that MeterColorMapperTest uses to test transforms
// of IR::IfStatements with meter color conditions.  It involves meter
// conditions that attempt to execute unsupported statements.

#include <core.p4>
#include <v1model.p4>

#include "control_stubs.p4"
#include "simple_headers.p4"

// Tests a meter condition that tries to perform an assignment.
control meter_assign(inout headers hdr, inout test_metadata_t meta,
                      inout standard_metadata_t standard_metadata) {
  apply {
    if (meta.enum_color == meter_color_t.COLOR_GREEN) {
      clone3(CloneType.I2E, 1024, {});
      meta.smaller_metadata = 1;  // Unsupported assignment.
    }
  }
}

// Tests a meter condition that applies a table.
control meter_and_apply(inout headers hdr, inout test_metadata_t meta,
                        inout standard_metadata_t standard_metadata) {
  action nop_green() {
  }
  @switchstack("pipeline_stage: INGRESS_ACL")
  table green_table {
    actions = {
      nop_green;
    }
    key = {
      hdr.ethernet.etherType: exact;
    }
  }
  apply {
    if (meta.enum_color == meter_color_t.COLOR_GREEN) {
      green_table.apply();
    }
  }
}

// Tests a valid meter transform appearing after an unsupported condition.
control meter_valid_after_unsupported(
    inout headers hdr, inout test_metadata_t meta) {
  apply {
    if (meta.enum_color == meter_color_t.COLOR_YELLOW) {
      meta.smaller_metadata = 1;  // Unsupported assignment.
    }
    if (meta.enum_color == meter_color_t.COLOR_GREEN) {
      clone3(CloneType.I2E, 1024, {});
    }
  }
}


V1Switch(parser_stub(), meter_valid_after_unsupported(), meter_assign(),
         meter_and_apply(), compute_checksum_stub(), deparser_stub()) main;
