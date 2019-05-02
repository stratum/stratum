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

// This is a .p4 file that p4c backend classes can use to test how they handle
// P4 return statements.  Due to the way the p4c frontend optimizes, return
// statements are no longer part of the program when the backend sees the IR.
// The frontend replaces returns with temporary booleans and if-statements.

#include <core.p4>
#include <v1model.p4>

#include "control_stubs.p4"
#include "simple_headers.p4"

// This control is intended for testing at the control body level.  It is
// difficult to test returns at the individual statement level because of
// the extra temporary variable assignments and conditions that the frontend
// inserts.
control early_return(inout headers hdr, inout test_metadata_t meta,
                     inout standard_metadata_t standard_metadata) {
  action dummy_action() {
  }
  @switchstack("pipeline_stage: INGRESS_ACL")
  table test_table {
    actions = {
      dummy_action;
    }
    key = {
      hdr.ethernet.etherType: exact;
    }
  }
  apply {
    // When the backend sees the IR, frontend transforms have made the logic
    // below equivalent to this statement series:
    //  hasReturned_0 = false;
    //  if (meta.smaller_metadata == 0) {
    //    hasReturned_0 = true;
    //  }
    //  if (!hasReturned_0) {
    //    test_table.apply();
    //  }
    if (meta.smaller_metadata == 0) {
      return;
    }
    test_table.apply();
  }
}

// This control is also intended for testing at the control body level.  It has
// a more complex return structure.
control control_nested_return(inout headers hdr, inout test_metadata_t meta,
                              inout standard_metadata_t standard_metadata) {
  action dummy_action() {
  }
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
    // The frontend transforms these statements into this form:
    //  hasReturned_2 = false;
    //  if (hdr.ethernet.isValid()) {
    //    tmp_2 = table1.apply().hit;
    //    if (!tmp_2) {
    //      if (meta.smaller_metadata == 0) {
    //        hasReturned_2 = true;
    //      }
    //      if (!hasReturned_2) {
    //        tmp_1 = table2.apply().hit;
    //        if (!tmp_1) {
    //          table3.apply();
    //        }
    //      }
    //    }
    //  }
    if (hdr.ethernet.isValid()) {
      if (!table1.apply().hit) {
        if (meta.smaller_metadata == 0) {
          return;
        }
        if (!table2.apply().hit) {
          table3.apply();
        }
      }
    }
  }
}

V1Switch(parser_stub(), verify_checksum_stub(), control_nested_return(),
         early_return(), compute_checksum_stub(), deparser_stub()) main;
