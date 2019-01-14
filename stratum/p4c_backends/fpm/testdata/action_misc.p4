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

// This is a .p4 file that ActionDecoderTest uses to verify miscellaneous
// types of action statements.

#include <core.p4>
#include <v1model.p4>

#include "control_stubs.p4"
#include "simple_headers.p4"

control ingress(inout headers hdr, inout test_metadata_t meta,
                inout standard_metadata_t standard_metadata) {
  action empty_statement() {
  }
  action exit_statement() {
    exit;
  }
  action return_statement() {
    return;
  }
  action set_valid_statement() {
    hdr.ethernet2.setValid();
  }
  action set_invalid_statement() {
    hdr.ethernet2.setInvalid();
  }
  action push_statement() {
    hdr.ethernet_stack.push_front(1);
  }

  // The next two actions need @id annotations to make sure the p4c frontend
  // preserves them in nested blocks within "two_nested_blocks" below.
  @id(0xa1)
  action nested_block_1() {
    meta.smaller_metadata = 1;
  }
  @id(0xa2)
  action nested_block_2() {
    meta.other_metadata = 2;
  }
  action two_nested_blocks() {
    nested_block_1();
    nested_block_2();
  }

  @switchstack("pipeline_stage: VLAN_ACL")
  table acl_table {
    actions = {
      empty_statement;
      exit_statement;
      return_statement;
      set_valid_statement;
      set_invalid_statement;
      push_statement;
      two_nested_blocks();
    }
    key = {
      hdr.ethernet.etherType: exact;
    }
  }
  apply {
    acl_table.apply();
  }
}

V1Switch(parser_stub(), verify_checksum_stub(), ingress(), egress_stub(),
         compute_checksum_stub(), deparser_stub()) main;
