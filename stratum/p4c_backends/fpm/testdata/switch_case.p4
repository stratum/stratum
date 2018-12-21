// This is a .p4 file that SwitchCaseDecoderTest uses to verify various
// types of IR::SwitchStatements.

#include <core.p4>
#include <v1model.p4>

#include "control_stubs.p4"
#include "simple_headers.p4"

// This test of common clone-on-green-else-drop logic replaces the ingress
// control.
control normal_clone_drop(inout headers hdr, inout test_metadata_t meta,
                          inout standard_metadata_t standard_metadata) {
  action case1_clone_green() {
  }
  action case2_drop_not_green() {
  }
  @switchstack("pipeline_stage: INGRESS_ACL")
  table action_switch_table {
    actions = {
      case1_clone_green;
      case2_drop_not_green;
    }
    key = {
      hdr.ethernet.etherType: exact;
    }
  }
  apply {
    // This switch statement and action combination clones green packets and
    // drops all others.
    switch (action_switch_table.apply().action_run) {
      case1_clone_green: {
        if (meta.enum_color == meter_color_t.COLOR_GREEN) {
          clone3(CloneType.I2E, 1024, {standard_metadata.ingress_port});
        }
      }
      case2_drop_not_green: {
        if (meta.enum_color != meter_color_t.COLOR_GREEN) {
          mark_to_drop();
        }
      }
    }
  }
}

// TODO(teverman): Given recent instability in the P4 checksum API and
// likely additional turbulence through PSA, consider moving these tests
// to some other control.
// The checksum verify control is overloaded to test what happens when
// IfStatement conditions are inverted by the "!" operator.  The p4c pre-backend
// passes should optimize conditions such as !(foo == bar) to (foo != bar).
control inverted_conditions(inout headers hdr, inout test_metadata_t meta) {
  action case1_drop_not_green() {
  }
  action case2_clone_not_not_green() {
  }
  @switchstack("pipeline_stage: INGRESS_ACL")
  table action_switch_table {
    actions = {
      case1_drop_not_green;
      case2_clone_not_not_green;
    }
    key = {
      hdr.ethernet.etherType: exact;
    }
  }
  apply {
    // This switch statement and action combination clones green packets and
    // drops all others.
    switch (action_switch_table.apply().action_run) {
      case1_drop_not_green: {
        if (!(meta.enum_color == meter_color_t.COLOR_GREEN)) {
          mark_to_drop();
        }
      }
      case2_clone_not_not_green: {
        if (!(meta.enum_color != meter_color_t.COLOR_GREEN)) {
          clone3(CloneType.I2E, 1024, {});
        }
      }
    }
  }
}

V1Switch(parser_stub(), inverted_conditions(), normal_clone_drop(),
         egress_stub(), compute_checksum_stub(), deparser_stub()) main;
