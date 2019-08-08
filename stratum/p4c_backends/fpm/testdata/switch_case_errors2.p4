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

// This is a .p4 file that SwitchCaseDecoderTest uses to verify various
// errors in IR::SwitchStatements.

#include <core.p4>
#include <v1model.p4>

#include "control_stubs.p4"
#include "simple_headers.p4"

// TODO(unknown): Given recent instability in the P4 checksum API and
// likely additional turbulence through PSA, consider moving these tests
// to some other control.
// The checksum verify control is overloaded to test an unsupported comparison
// of a color field that isn't IR::Type_Enum.
control color_not_enum(inout headers hdr, inout test_metadata_t meta) {
  action case1_color_not_enum() {
  }
  @switchstack("pipeline_stage: VLAN_ACL")
  table foo_table {
    actions = {
      case1_color_not_enum;
    }
    key = {
      hdr.ethernet.etherType: exact;
    }
  }
  apply {
    switch (foo_table.apply().action_run) {
      case1_color_not_enum: {
        // meta.color is a bit field, not an enum.
        if (meta.color != 1)
          mark_to_drop();
      }
    }
  }
}

// The checksum compute control is overloaded to test an unsupported
// extern function call within a switch statement.
extern void unsupported_function();

control unsupported_function_test(
    inout headers hdr, inout test_metadata_t meta) {
  action case1_call_unsupported() {
    unsupported_function();
  }
  @switchstack("pipeline_stage: VLAN_ACL")
  table foo_table {
    actions = {
      case1_call_unsupported;
    }
    key = {
      hdr.ethernet.etherType: exact;
    }
  }
  apply {
    switch (foo_table.apply().action_run) {
      case1_call_unsupported: {
        if (meta.enum_color == meter_color_t.COLOR_YELLOW)
          unsupported_function();
      }
    }
  }
}

V1Switch(parser_stub(), color_not_enum(), ingress_stub(),
         egress_stub(), unsupported_function_test(), deparser_stub()) main;
