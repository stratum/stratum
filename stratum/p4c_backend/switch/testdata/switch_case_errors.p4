// This is a .p4 file that SwitchCaseDecoderTest uses to verify various
// errors in IR::SwitchStatements.

#include <core.p4>
#include <v1model.p4>

#include "control_stubs.p4"
#include "simple_headers.p4"

control ingress_nested_if(inout headers hdr, inout test_metadata_t meta,
                          inout standard_metadata_t standard_metadata) {
  action case1_nested_if() {
  }
  action case2_action() {
  }
  @switchstack("pipeline_stage: INGRESS_ACL")
  table action_switch_table {
    actions = {
      case1_nested_if;
      case2_action;
    }
    key = {
      hdr.ethernet.etherType: exact;
    }
  }
  apply {
    switch (action_switch_table.apply().action_run) {
      case1_nested_if: {
        // The backend does not allow nested if statements inside cases.
        if (meta.enum_color != meter_color_t.COLOR_GREEN) {
          if (meta.enum_color != meter_color_t.COLOR_YELLOW) {
            mark_to_drop();
          }
        }
      }
      case2_action: {
      }
    }
  }
}

control egress_assign(inout headers hdr, inout test_metadata_t meta,
                      inout standard_metadata_t standard_metadata) {
  action case1_assign() {
  }
  @switchstack("pipeline_stage: EGRESS_ACL")
  table egress_switch_table {
    actions = {
      case1_assign;
    }
    key = {
      hdr.ethernet.etherType: exact;
    }
  }
  apply {
    switch (egress_switch_table.apply().action_run) {
      case1_assign: {
        // The backend does not allow assignments inside cases.
        meta.enum_color = meter_color_t.COLOR_RED;
      }
    }
  }
}

// TODO(teverman): Given recent instability in the P4 checksum API and
// likely additional turbulence through PSA, consider moving these tests
// to some other control.
// The checksum verify control is overloaded to test an unsupported condition
// in an if statement.
control bad_condition(inout headers hdr, inout test_metadata_t meta) {
  action case1_condition_or() {
  }
  @switchstack("pipeline_stage: VLAN_ACL")
  table foo_table {
    actions = {
      case1_condition_or;
    }
    key = {
      hdr.ethernet.etherType: exact;
    }
  }
  apply {
    switch (foo_table.apply().action_run) {
      case1_condition_or: {
        // The logical || is too complex for the backend.
        if (meta.enum_color == meter_color_t.COLOR_RED ||
            meta.enum_color == meter_color_t.COLOR_YELLOW)
          mark_to_drop();
      }
    }
  }
}

// TODO(teverman): Given recent instability in the P4 checksum API and
// likely additional turbulence through PSA, consider moving these tests
// to some other control.
// The compute checksum control is overloaded to test case labels that
// fall through to the next label.
control fall_through_case(inout headers hdr, inout test_metadata_t meta) {
  action case1_fallthru() {
  }
  action case2_drop_red() {
  }
  @switchstack("pipeline_stage: VLAN_ACL")
  table foo_table {
    actions = {
      case1_fallthru;
      case2_drop_red;
    }
    key = {
      hdr.ethernet.etherType: exact;
    }
  }
  apply {
    switch (foo_table.apply().action_run) {
      case1_fallthru:  // Fallthru intended.
      case2_drop_red: {
        if (meta.enum_color == meter_color_t.COLOR_RED)
          mark_to_drop();
      }
    }
  }
}

V1Switch(parser_stub(), bad_condition(), ingress_nested_if(),
         egress_assign(), fall_through_case(), deparser_stub()) main;
