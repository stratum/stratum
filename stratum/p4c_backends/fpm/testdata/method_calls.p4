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

// This is a .p4 file that MethodCallDecoderTest uses to verify various
// forms of method call statements.

#include <core.p4>
#include <v1model.p4>

#include "control_stubs.p4"
#include "simple_headers.p4"

// This extern tests method calls to an unsupported P4::ExternFunction.
extern void unsupported_function();

// Every action in the ingress control contains a single method call
// statement for test input.
control ingress(inout headers hdr, inout test_metadata_t meta,
                inout standard_metadata_t standard_metadata) {
  direct_counter(CounterType.packets_and_bytes) test_direct_counter;
  direct_meter<meter_color_t>(MeterType.bytes) test_direct_meter;
  counter(32w1, CounterType.packets_and_bytes) test_counter;
  meter(32w1, MeterType.packets) test_meter;

  action drop_statement() {
    mark_to_drop();
  }
  action clone3_statement_const_port() {
    clone3(CloneType.I2E, 1024, {});
  }
  action clone_statement_const_port() {
    clone(CloneType.I2E, 1024);
  }
  action set_header_invalid() {
    hdr.ethernet.setInvalid();
  }
  action set_header_valid() {
    hdr.ethernet.setValid();
  }
  action call_unsupported() {
    unsupported_function();
  }
  action call_ignored_builtin() {
    hdr.ethernet_stack.push_front(1);
  }
  action count_direct_counter() {
    test_direct_counter.count();
  }
  action read_direct_meter() {
    test_direct_meter.read(meta.enum_color);
  }
  action count_counter() {
    test_counter.count(0);
  }
  action execute_meter_meter() {
    test_meter.execute_meter(0, meta.color);
  }
  @switchstack("pipeline_stage: VLAN_ACL")
  table dummy_table {
    actions = {
      drop_statement;
      clone3_statement_const_port;
      clone_statement_const_port;
      set_header_invalid;
      set_header_valid;
      call_unsupported;
      call_ignored_builtin;
      count_direct_counter;
      read_direct_meter;
      count_counter;
      execute_meter_meter;
    }
    key = {
      hdr.ethernet.etherType: exact;
    }
  }
  apply {
    dummy_table.apply();
  }
}

V1Switch(parser_stub(), verify_checksum_stub(), ingress(), egress_stub(),
         compute_checksum_stub(), deparser_stub()) main;
