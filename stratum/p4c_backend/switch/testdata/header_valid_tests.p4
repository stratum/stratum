// This is a .p4 file that HeaderValidInspectorTest uses to verify various
// valid and invalid uses of <header>.isValid().

#include <core.p4>
#include <v1model.p4>

#include "control_stubs.p4"
#include "simple_headers.p4"

// This control is intended for testing at the individual statement level.
// All statements individually represent supported uses of isValid().  However,
// the entire sequence is bad because tables reappear throughout the statements
// with different header validity conditions.
control good_statements(inout headers hdr, inout test_metadata_t meta,
                        inout standard_metadata_t standard_metadata) {
  action dummy_action() {
  }
  @switchstack("pipeline_stage: L3")
  table table1 {
    actions = {
      dummy_action;
    }
    key = {
      hdr.ethernet.etherType: exact;
    }
  }
  @switchstack("pipeline_stage: INGRESS_ACL")
  table table2 {
    actions = {
      dummy_action;
    }
    key = {
      hdr.ethernet.etherType: exact;
    }
  }
  apply {
    // In statements 0 - 4, the usage is supported, but no table map
    // output is necessary.
    // Statement #0.
    table1.apply();

    // Statement #1.
    table2.apply();

    // Statement #2.
    if (meta.other_metadata == 1) {
      table1.apply();
    } else {
      table2.apply();
    }

    // Statement #3.
    if (meta.smaller_metadata == 3) {
      if (hdr.ethernet.isValid()) {
        meta.other_metadata = 1;
      }
      table1.apply();
    }

    // Statement #4.
    if (meta.smaller_metadata == 4) {
      if (hdr.ethernet.isValid()) {
        meta.other_metadata = 1;
      } else {
        meta.other_metadata = 2;
      }
      table1.apply();
    }

    // In statements 5 - 10, the usage is supported, and table1 gets
    // updated to be valid only for hdr.ethernet.
    // Statement #5.
    if (hdr.ethernet.isValid()) {
      table1.apply();
    }

    // Statement #6.
    if (meta.smaller_metadata == 6) {
      if (hdr.ethernet.isValid()) {
        table1.apply();
      }
      table2.apply();
    }

    // Statement #7.
    if (hdr.ethernet.isValid()) {
      if (meta.smaller_metadata == 6) {
        table1.apply();
      }
    }

    // Statement #8.
    if (meta.smaller_metadata == 8) {
      if (!table2.apply().hit) {
        if (hdr.ethernet.isValid()) {
          table1.apply();
        }
      }
    }

    // Statement #9.
    if (hdr.ethernet2.isValid()) {
      meta.smaller_metadata = 9;
    } else {
      if (hdr.ethernet.isValid()) {
        table1.apply();
      }
    }

    // Statement #10.
    if (hdr.ethernet2.isValid()) {
      meta.smaller_metadata = 9;
    } else if (hdr.ethernet.isValid()) {
      table1.apply();
    }

    // In statements 11 - 14, the usage is supported, and table1 and table2 get
    // updated to be valid only for hdr.ethernet.
    // Statement #10.
    if (hdr.ethernet.isValid()) {
      table1.apply();
      table2.apply();
    }

    // Statement #12.
    if (hdr.ethernet.isValid()) {
      if (!table1.apply().hit) {
        table2.apply();
      }
    }

    // Statement #13.
    if (hdr.ethernet.isValid()) {
      if (!table2.apply().hit) {
        table1.apply();
      }
    }

    // Statement #14.
    if (hdr.ethernet.isValid()) {
      if (!table2.apply().hit) {
        if (meta.smaller_metadata == 14) {
          meta.other_metadata = 2;
        } else {
          table1.apply();
        }
      }
    }

    // In statements 15 - 18, the usage is supported, table1 is conditional
    // on one header, and table2 is conditional on both headers.
    // Statement #15.
    if (hdr.ethernet.isValid()) {
      if (!table1.apply().hit) {
        if (hdr.ethernet2.isValid()) {
          table2.apply();
        }
      }
    }

    // Statement #16.
    if (hdr.ethernet.isValid()) {
      if (hdr.ethernet2.isValid()) {
        table2.apply();
      }
      table1.apply();
    }

    // Statement #17.
    if (hdr.ethernet.isValid()) {
      if (hdr.ethernet2.isValid()) {
        table2.apply();
      } else if (meta.smaller_metadata == 17) {
        table1.apply();
      }
    }

    // Statement #18.
    if (hdr.ethernet.isValid()) {
      if (hdr.ethernet2.isValid()) {
        table2.apply();
      } else if (meta.smaller_metadata == 18) {
        meta.other_metadata = 1818;
      } else {
        table1.apply();
      }
    }
  }
}

// This control is intended for testing at the individual statement level.
// Each statement tests an unsupported use case.
control bad_statements(
    inout headers hdr, inout test_metadata_t meta,
    inout standard_metadata_t standard_metadata) {
  action dummy_action() {
  }
  @switchstack("pipeline_stage: VLAN_ACL")
  table table3 {
    actions = {
      dummy_action;
    }
    key = {
      hdr.ethernet.etherType: exact;
    }
  }
  @switchstack("pipeline_stage: L2")
  table table4 {
    actions = {
      dummy_action;
    }
    key = {
      hdr.ethernet.etherType: exact;
    }
  }
  apply {
    // In statements 0 - 2, table applies in the false block of an isValid()
    // test should produce errors since there are no other header-valid
    // conditions in effect.
    // Statement #0.
    if (hdr.ethernet.isValid()) {
      table3.apply();
    } else {
      table4.apply();
    }

    // Statement #1.
    if (hdr.ethernet.isValid()) {
      table3.apply();
    } else {
      if (!table4.apply().hit) {
        meta.other_metadata = 1;
      }
    }

    // Statement #2.
    if (meta.smaller_metadata == 2) {
      meta.other_metadata = 21;
    } else {
      if (hdr.ethernet.isValid()) {
        meta.other_metadata = 23;
      } else {
        table3.apply();
      }
    }

    // Statement #3 - isValid() condition with '!' operator is too complex.
    if (!hdr.ethernet.isValid()) {
      table4.apply();
    }

    // Statement #4 - redundant hdr.ethernet.isValid() nested inside the
    // same if statement.
    if (hdr.ethernet.isValid()) {
      if (meta.smaller_metadata == 2) {
        meta.other_metadata = 2;
      } else if (hdr.ethernet.isValid()) {
        table3.apply();
      }
    }

    // Statement #5 - isValid() condition with '&&' operator is too complex.
    if (hdr.ethernet.isValid() && hdr.ethernet2.isValid()) {
      table4.apply();
    }

    // In statements 6 - 8, the same table is applied with conflicting sets
    // of header conditions.
    // Statement #6.
    if (meta.smaller_metadata == 6) {
      if (hdr.ethernet.isValid()) {
        table3.apply();
      }
      table3.apply();
    }

    // Statement #7.
    if (meta.smaller_metadata == 7) {
      table3.apply();
      if (hdr.ethernet.isValid()) {
        table3.apply();
      }
    }

    // Statement #8.
    if (hdr.ethernet.isValid()) {
      table3.apply();
    } else if (hdr.ethernet2.isValid()) {
      table3.apply();
    }
  }
}

V1Switch(parser_stub(), verify_checksum_stub(), good_statements(),
         bad_statements(), compute_checksum_stub(), deparser_stub()) main;
