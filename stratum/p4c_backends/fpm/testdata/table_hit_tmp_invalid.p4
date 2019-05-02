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

// This is a .p4 file that TableHitInspectorTest uses to verify various
// invalid uses of temporary hit variables.

#include <core.p4>
#include <v1model.p4>

#include "control_stubs.p4"
#include "simple_headers.p4"

// This control is intended for testing at the individual statement level.
control hit_var_invalid(inout headers hdr, inout test_metadata_t meta,
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
  @switchstack("pipeline_stage: INGRESS_ACL")
  table table2 {
    actions = {
      dummy_action;
    }
    key = {
      hdr.ethernet.etherType: exact;
    }
  }
  @switchstack("pipeline_stage: INGRESS_ACL")
  table table3 {
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

    // Statement index 0: Table apply can't be conditional on a prior hit.
    if (hdr.ethernet.isValid()) {
      if (table1.apply().hit) {
        table2.apply();
      }
    }

    // Statement index 1: Table apply+miss can't be conditional on a prior hit.
    if (hdr.ethernet.isValid()) {
      if (table1.apply().hit) {
        if (!table2.apply().hit) {
          meta.other_metadata = 1;
        }
      }
    }

    // Statement index 2: Table hit can't be conditional on a prior hit.
    if (hdr.ethernet.isValid()) {
      if (table1.apply().hit) {
        meta.other_metadata = 1;
        table2.apply();
        if (table3.apply().hit) {
          meta.smaller_metadata = 2;
        }
      }
    }

    // Statement index 3: Meter action can't be conditional on a table miss.
    if (hdr.ethernet.isValid()) {
      if (!table1.apply().hit) {
        if (meta.enum_color == meter_color_t.COLOR_RED) {
          mark_to_drop();
        }
      }
    }

    // Statement index 4: Complex conditional expression involving table hit
    // and meter.
    if (hdr.ethernet.isValid()) {
      if (table1.apply().hit && meta.enum_color == meter_color_t.COLOR_RED) {
        mark_to_drop();
      }
    }

    // Statement index 5: Meter action can't be conditional on a table miss.
    if (hdr.ethernet.isValid()) {
      if (!table1.apply().hit) {
        if (meta.enum_color == meter_color_t.COLOR_RED) {
          mark_to_drop();
        }
      }
    }

    // Statement index 6: Meter action can't be in the else clause after
    // a table hit.
    if (hdr.ethernet.isValid()) {
      if (table1.apply().hit) {
        meta.other_metadata = 1;
      } else if (meta.enum_color == meter_color_t.COLOR_RED) {
        mark_to_drop();
      }
    }

    // Statement index 7: Statement 6 written a different way.
    if (hdr.ethernet.isValid()) {
      if (table1.apply().hit) {
        meta.other_metadata = 1;
      } else {
        if (meta.enum_color == meter_color_t.COLOR_RED) {
          mark_to_drop();
        }
      }
    }

    // Statement index 8: Meter action can't be conditional on anything
    // but a table hit.
    if (hdr.ethernet.isValid()) {
      if (table1.apply().hit) {
        if (meta.other_metadata == 5) {
          if (meta.enum_color == meter_color_t.COLOR_RED) {
            mark_to_drop();
          }
        }
      }
    }

    // Statement index 9: Switch apply can't be conditional on a table hit.
    if (hdr.ethernet.isValid()) {
      if (table1.apply().hit) {
        switch (table2.apply().action_run) {
          dummy_action: {
            if (meta.enum_color == meter_color_t.COLOR_YELLOW)
              mark_to_drop();
          }
        }
      }
    }

    // Statement index 10: Hit variable scope abuse.
    if (hdr.ethernet.isValid()) {
      bool hit1 = table1.apply().hit;
      bool hit2 = table2.apply().hit;
      if (!hit1) {
        meta.smaller_metadata = 1;
      }
      if (!hit2) {
        meta.smaller_metadata = 2;
      }
    }

    // Statement index 11: Standalone meter color evaluation with no table hit.
    if (hdr.ethernet.isValid()) {
      if (meta.enum_color == meter_color_t.COLOR_YELLOW)
        mark_to_drop();
    }

    // Statement index 12: Hit variable can't be used after another apply.
    if (hdr.ethernet.isValid()) {
      bool hit1 = table1.apply().hit;
      table2.apply();
      if (!hit1) {
        meta.smaller_metadata = 1;
      }
    }
  }
}

V1Switch(parser_stub(), verify_checksum_stub(), hit_var_invalid(),
         egress_stub(), compute_checksum_stub(), deparser_stub()) main;
