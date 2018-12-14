// This is a .p4 file that TableHitInspectorTest and SimpleHitInspectorTest
// use to verify various valid uses of temporary hit variables.

#include <core.p4>
#include <v1model.p4>

#include "control_stubs.p4"
#include "simple_headers.p4"

// This control is intended for testing at the individual statement level.
control hit_var_valid_statements(inout headers hdr, inout test_metadata_t meta,
                                 inout standard_metadata_t standard_metadata) {
  action dummy_action() {
  }
  @switchstack("pipeline_stage: INGRESS_ACL")
  table upper_table {
    actions = {
      dummy_action;
    }
    key = {
      hdr.ethernet.etherType: exact;
    }
  }
  @switchstack("pipeline_stage: INGRESS_ACL")
  table middle_table {
    actions = {
      dummy_action;
    }
    key = {
      hdr.ethernet.etherType: exact;
    }
  }
  @switchstack("pipeline_stage: INGRESS_ACL")
  table lower_table {
    actions = {
      dummy_action;
    }
    key = {
      hdr.ethernet.etherType: exact;
    }
  }
  apply {
    // Since the front end and midend deconstruct hits in IfStatement
    // conditions into multiple statements, the "isValid" statements below
    // guarantee that everything to be tested falls under the hierarchy
    // of a single statement.

    // Statement index 0: Nested 3 table miss sequence.
    if (hdr.ethernet.isValid()) {
      if (!upper_table.apply().hit) {
        if (!middle_table.apply().hit) {
          if (!lower_table.apply().hit) {
            meta.other_metadata = 1;
          }
        }
      }
    }

    // Statement index 1: Meter action after table hit.
    if (hdr.ethernet.isValid()) {
      if (upper_table.apply().hit) {
        if (meta.enum_color == meter_color_t.COLOR_RED) {
          mark_to_drop();
        }
      }
    }

    // Statement index 2: Meter action after table hit with intervening
    // statement.
    if (hdr.ethernet.isValid()) {
      if (upper_table.apply().hit) {
        meta.other_metadata = 1;
        if (meta.enum_color == meter_color_t.COLOR_RED) {
          mark_to_drop();
        }
      }
    }

    // Statement index 3: Meter action after table hit embedded inside table
    // miss.
    if (hdr.ethernet.isValid()) {
      if (!upper_table.apply().hit) {
        if (middle_table.apply().hit) {
          if (meta.enum_color == meter_color_t.COLOR_RED) {
            mark_to_drop();
          }
        }
      }
    }

    // Statement index 4: Multiple meter actions after single table hit.
    if (hdr.ethernet.isValid()) {
      if (upper_table.apply().hit) {
        if (meta.enum_color == meter_color_t.COLOR_GREEN) {
          clone3(CloneType.I2E, 1024, {});
        }
        if (meta.enum_color == meter_color_t.COLOR_RED) {
          mark_to_drop();
        }
      }
    }

    // Statement index 5: Meter action after table miss-miss-hit sequence.
    if (hdr.ethernet.isValid()) {
      if (!upper_table.apply().hit) {
        if (!middle_table.apply().hit) {
          if (lower_table.apply().hit) {
            if (meta.enum_color == meter_color_t.COLOR_RED) {
              mark_to_drop();
            }
          }
        }
      }
    }

    // Statement index 6: Meter action in "if (miss) else meter" sequence.
    if (hdr.ethernet.isValid()) {
      if (!upper_table.apply().hit) {
        meta.other_metadata = 1;
      } else {
        if (meta.enum_color == meter_color_t.COLOR_RED) {
          mark_to_drop();
        }
      }
    }

    // Statement index 7: Variation of statement 6.
    if (hdr.ethernet.isValid()) {
      if (!upper_table.apply().hit) {
        meta.other_metadata = 1;
      } else if (meta.enum_color == meter_color_t.COLOR_RED) {
        mark_to_drop();
      }
    }

    // Statement index 8: Multiple table apply sequence without hits/misses.
    if (hdr.ethernet.isValid()) {
      upper_table.apply();
      middle_table.apply();
      lower_table.apply();
    }

    // Statement index 9: Table hit plus meter action series.
    if (hdr.ethernet.isValid()) {
      if (upper_table.apply().hit) {
        if (meta.enum_color == meter_color_t.COLOR_GREEN) {
          clone3(CloneType.I2E, 1024, {});
        }
      }
      if (middle_table.apply().hit) {
        if (meta.enum_color == meter_color_t.COLOR_RED) {
          mark_to_drop();
        }
      }
    }

    // Statement index 10: Miss after hit replaces prior hit status.
    if (hdr.ethernet.isValid()) {
      if (upper_table.apply().hit) {
        meta.other_metadata = 1;
      }
      if (!middle_table.apply().hit) {
        if (lower_table.apply().hit) {
          if (meta.enum_color == meter_color_t.COLOR_RED) {
            mark_to_drop();
          }
        }
      }
    }

    // Statement index 11: Switch apply after miss.
    if (hdr.ethernet.isValid()) {
      if (!upper_table.apply().hit) {
        switch (middle_table.apply().action_run) {
          dummy_action: {
            if (meta.enum_color == meter_color_t.COLOR_YELLOW)
              mark_to_drop();
          }
        }
      }
    }
  }
}

// This control is intended for testing at the control body level.
control hit_var_scope_ok(inout headers hdr, inout test_metadata_t meta,
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
  table table2 {
    actions = {
      dummy_action;
    }
    key = {
      hdr.ethernet.etherType: exact;
    }
  }
  apply {
    bool hit1 = table1.apply().hit;
    bool hit2 = table2.apply().hit;
    if (!hit2) {
      meta.smaller_metadata = 1;
    }
  }
}

V1Switch(parser_stub(), verify_checksum_stub(), hit_var_valid_statements(),
         hit_var_scope_ok(), compute_checksum_stub(), deparser_stub()) main;
