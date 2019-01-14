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

// This is a .p4 file that SimpleHitInspectorTest uses to verify various
// invalid uses of temporary hit variables.

#include <core.p4>
#include <v1model.p4>

#include "control_stubs.p4"
#include "simple_headers.p4"


// This control is intended for testing at the individual statement level.
control complex_hits(inout headers hdr, inout test_metadata_t meta,
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

    // Statement index 0: Complex dual-miss condition.
    if (hdr.ethernet.isValid()) {
      if (!table1.apply().hit && !table2.apply().hit) {
        meta.smaller_metadata = 1;
      }
    }

    // Statement index 1: Complex dual-hit condition.
    if (hdr.ethernet.isValid()) {
      if (table1.apply().hit && table2.apply().hit) {
        meta.smaller_metadata = 1;
      }
    }

    // Statement index 2: For this statement, the IR ultimately looks like this:
    //  if (isValid)
    //    tmp_14 = table1.apply().hit;
    //    if (!tmp_14)
    //      tmp_15 = false;
    //    else
    //      tmp_15 = (meta.smaller_metadata == 0);
    //    if (tmp_15)
    //      meta.smaller_metadata = 1;
    // TODO(teverman): SimpleHitInspector can't currently detect that this is
    // invalid.  It will need to be rejected by the parent TableHitInspector.
    if (hdr.ethernet.isValid()) {
      if (!table1.apply().hit && meta.smaller_metadata == 0) {
        meta.smaller_metadata = 1;
      }
    }

    // Statement index 3: Dual hit condition assigned to temporary.
    if (hdr.ethernet.isValid()) {
      bool tmp_multi_hit = table1.apply().hit && table3.apply().hit;
      if (tmp_multi_hit)
        meta.smaller_metadata = 1;
    }

    // Statement index 4: Dual miss condition assigned to temporary.
    if (hdr.ethernet.isValid()) {
      bool tmp_multi_miss = !table1.apply().hit && !table3.apply().hit;
      if (tmp_multi_miss)
        meta.smaller_metadata = 1;
    }

    // Statement index 5: Multiple hit variables in one condition.  (The IR
    // renames the temps but leaves the logic as is.)
    if (hdr.ethernet.isValid()) {
      bool tmp_hit1 = table1.apply().hit;
      bool tmp_hit2 = table2.apply().hit;
      if (tmp_hit1 && tmp_hit2)
        meta.smaller_metadata = 1;
    }

    // Statement index 6: Two hit variables combined.  (The IR actually
    // optimizes out tmp_hit_both.)
    if (hdr.ethernet.isValid()) {
      bool tmp_hit1 = table1.apply().hit;
      bool tmp_hit2 = table2.apply().hit;
      bool tmp_hit_both = tmp_hit1 && tmp_hit2;
      if (tmp_hit_both)
        meta.smaller_metadata = 1;
    }

    // Statement index 7: Temporary variable reassignment.  This one turns
    // out to be valid because the IR optimizes out tmp_hit2.
    if (hdr.ethernet.isValid()) {
      bool tmp_hit1 = table1.apply().hit;
      bool tmp_hit2 = tmp_hit1;
      if (tmp_hit2)
        meta.smaller_metadata = 1;
    }

    // Statement index 8: Multiple hit variables combined with different
    // operators.
    if (hdr.ethernet.isValid()) {
      bool tmp_hit1 = table1.apply().hit;
      bool tmp_hit2 = table2.apply().hit;
      bool tmp_hit3 = table3.apply().hit;
      bool tmp_hit_all = tmp_hit1 && tmp_hit2 || tmp_hit3;
      if (tmp_hit_all)
        meta.smaller_metadata = 1;
    }
  }
}

V1Switch(parser_stub(), verify_checksum_stub(), complex_hits(),
         egress_stub(), compute_checksum_stub(), deparser_stub()) main;
