// This is a .p4 file that ControlInspectorTest uses to verify meter action
// decoding following a table hit.

#include <core.p4>
#include <v1model.p4>

#include "control_stubs.p4"
#include "simple_headers.p4"

// Tests meter color action following a table hit.  See also the control
// "meter_hit_hidden" in control_misc_test2.p4, which is a clone of this
// test with the table annotated into a hidden pipeline stage.
control meter_hit(inout headers hdr, inout test_metadata_t meta,
                  inout standard_metadata_t standard_metadata) {
  action hit1() {
  }
  action hit2() {
  }
  table hit_table {
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
    if (hit_table.apply().hit) {
      if (meta.enum_color == meter_color_t.COLOR_GREEN) {
        clone3(CloneType.I2E, 1024, {});
      }
    }
  }
}

// Tests meter color action following a table miss in else clause.
control meter_miss_else(inout headers hdr, inout test_metadata_t meta,
                        inout standard_metadata_t standard_metadata) {
  action hit1() {
  }
  action hit2() {
  }
  table miss_else_table {
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
    if (!miss_else_table.apply().hit) {
      meta.smaller_metadata = 0;
    } else if (meta.enum_color == meter_color_t.COLOR_GREEN) {
      clone3(CloneType.I2E, 1024, {});
    }
  }
}

V1Switch(parser_stub(), verify_checksum_stub(), meter_hit(), meter_miss_else(),
         compute_checksum_stub(), deparser_stub()) main;
