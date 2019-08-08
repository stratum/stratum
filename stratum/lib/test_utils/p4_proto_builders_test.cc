// Copyright 2018 Google LLC
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

#include "stratum/lib/test_utils/p4_proto_builders.h"

#include "stratum/lib/test_utils/matchers.h"
#include "stratum/lib/utils.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/substitute.h"

namespace stratum {
namespace test_utils {
namespace p4_proto_builders {
namespace {

using hal::P4ControlTableRef;
using hal::P4ControlBlock;
using hal::P4ControlStatement;

TEST(P4ProtoBuildersTest, Table) {
  P4ControlTableRef expected;
  CHECK_OK(
      ParseProtoFromString(
          R"PROTO(table_id: 1234
                  table_name: "table_1234"
                  pipeline_stage: EGRESS_ACL)PROTO", &expected));
  EXPECT_THAT(Table(1234, P4Annotation::EGRESS_ACL), EqualsProto(expected));
}

TEST(P4ProtoBuildersTest, ApplyTable_FromId) {
  P4ControlStatement expected;
  CHECK_OK(ParseProtoFromString(
      R"PROTO(
        apply {
          table_id: 1234
          table_name: "table_1234"
          pipeline_stage: EGRESS_ACL
        })PROTO", &expected));
  EXPECT_THAT(ApplyTable(1234, P4Annotation::EGRESS_ACL),
              EqualsProto(expected));
}

TEST(P4ProtoBuildersTest, ApplyTable_FromTable) {
  ::p4::config::v1::Table table;
  CHECK_OK(ParseProtoFromString(
      R"PROTO(preamble { id: 1234 name: "HelloWorld" })PROTO", &table));
  P4ControlStatement expected;
  CHECK_OK(ParseProtoFromString(
      R"PROTO(
        apply {
          table_id: 1234
          table_name: "HelloWorld"
          pipeline_stage: EGRESS_ACL
        })PROTO", &expected));
  EXPECT_THAT(ApplyTable(table, P4Annotation::EGRESS_ACL),
              EqualsProto(expected));
}

TEST(P4ProtoBuildersTest, ApplyTable_FromPreamble) {
  ::p4::config::v1::Preamble preamble;
  CHECK_OK(ParseProtoFromString(R"PROTO(id: 1234 name: "HelloWorld")PROTO",
                                &preamble));
  P4ControlStatement expected;
  CHECK_OK(ParseProtoFromString(
      R"PROTO(
        apply {
          table_id: 1234
          table_name: "HelloWorld"
          pipeline_stage: EGRESS_ACL
        })PROTO", &expected));
  EXPECT_THAT(ApplyTable(preamble, P4Annotation::EGRESS_ACL),
              EqualsProto(expected));
}

TEST(P4ProtoBuildersTest, ApplyNested_FromRefs) {
  std::vector<std::string> table_strings = {
    R"PROTO(table_id: 1 table_name: "t1" pipeline_stage: VLAN_ACL)PROTO",
    R"PROTO(table_id: 2 table_name: "t2" pipeline_stage: VLAN_ACL)PROTO",
    R"PROTO(table_id: 3 table_name: "t3" pipeline_stage: VLAN_ACL)PROTO",
  };
  std::vector<hal::P4ControlTableRef> tables;
  for (const std::string& table_string : table_strings) {
    hal::P4ControlTableRef table;
    CHECK_OK(ParseProtoFromString(table_string, &table));
    tables.push_back(table);
  }

  P4ControlBlock expected;
  CHECK_OK(ParseProtoFromString(
      absl::Substitute(
          R"PROTO(
            statements { apply { $0 } }
            statements {
              branch {
                condition { hit { $0 } }
                false_block {
                  statements { apply { $1 } }
                  statements {
                    branch {
                      condition { hit { $1 } }
                      false_block { statements { apply { $2 } } }
                    }
                  }
                }
              }
            }
          )PROTO", table_strings[0], table_strings[1], table_strings[2]),
      &expected));

  EXPECT_THAT(ApplyNested(tables), EqualsProto(expected));
}

TEST(P4ProtoBuildersTest, ApplyNested_FromTables) {
  std::vector<std::string> table_strings = {
      R"PROTO(preamble { id: 1 name: "t1" })PROTO",
      R"PROTO(preamble { id: 2 name: "t2" })PROTO",
      R"PROTO(preamble { id: 3 name: "t3" })PROTO",
  };
  std::vector<::p4::config::v1::Table> tables;
  for (const std::string& table_string : table_strings) {
    ::p4::config::v1::Table table;
    CHECK_OK(ParseProtoFromString(table_string, &table));
    tables.push_back(table);
  }

  std::vector<std::string> ref_strings = {
    R"PROTO(table_id: 1 table_name: "t1" pipeline_stage: VLAN_ACL)PROTO",
    R"PROTO(table_id: 2 table_name: "t2" pipeline_stage: VLAN_ACL)PROTO",
    R"PROTO(table_id: 3 table_name: "t3" pipeline_stage: VLAN_ACL)PROTO",
  };

  P4ControlBlock expected;
  CHECK_OK(ParseProtoFromString(
      absl::Substitute(
          R"PROTO(
            statements { apply { $0 } }
            statements {
              branch {
                condition { hit { $0 } }
                false_block {
                  statements { apply { $1 } }
                  statements {
                    branch {
                      condition { hit { $1 } }
                      false_block { statements { apply { $2 } } }
                    }
                  }
                }
              }
            }
          )PROTO", ref_strings[0], ref_strings[1], ref_strings[2]),
      &expected));

  EXPECT_THAT(ApplyNested(tables, P4Annotation::VLAN_ACL),
              EqualsProto(expected));
}

TEST(P4ProtoBuildersTest, ApplyNested_Empty) {
  P4ControlBlock expected;
  EXPECT_THAT(ApplyNested({}), EqualsProto(expected));
}

TEST(P4ProtoBuildersTest, ApplyNested_SingleTable) {
  std::string table_string =
      R"PROTO(table_id: 1 table_name: "t1" pipeline_stage: VLAN_ACL)PROTO";
  hal::P4ControlTableRef table;
  CHECK_OK(ParseProtoFromString(table_string, &table));

  P4ControlBlock expected;
  CHECK_OK(ParseProtoFromString(absl::Substitute(
                                    R"PROTO(
                                      statements { apply { $0 } }
                                    )PROTO", table_string),
                                &expected));

  EXPECT_THAT(ApplyNested({table}), EqualsProto(expected));
}

// P4ControlTableRefBuilder tests.
TEST(P4ProtoBuildersTest, P4ControlTableRefBuilderEmpty) {
  P4ControlTableRef expected;

  EXPECT_THAT(P4ControlTableRefBuilder().Build(), EqualsProto(expected));
}

TEST(P4ProtoBuildersTest, P4ControlTableRefBuilderId) {
  P4ControlTableRef expected;
  expected.set_table_id(1234);

  EXPECT_THAT(P4ControlTableRefBuilder().Id(1234).Build(),
              EqualsProto(expected));
}

TEST(P4ProtoBuildersTest, P4ControlTableRefBuilderName) {
  P4ControlTableRef expected;
  expected.set_table_name("1234");

  EXPECT_THAT(P4ControlTableRefBuilder().Name("1234").Build(),
              EqualsProto(expected));
}

TEST(P4ProtoBuildersTest, P4ControlTableRefBuilderStage) {
  P4ControlTableRef expected;
  expected.set_pipeline_stage(P4Annotation::VLAN_ACL);

  EXPECT_THAT(P4ControlTableRefBuilder().Stage(P4Annotation::VLAN_ACL).Build(),
              EqualsProto(expected));
}

TEST(P4ProtoBuildersTest, P4ControlTableRefBuilderFromPreamble) {
  P4ControlTableRef expected;
  CHECK_OK(ParseProtoFromString(
      R"PROTO(table_id: 1234 table_name: "table")PROTO", &expected));

  ::p4::config::v1::Preamble preamble;
  CHECK_OK(
      ParseProtoFromString(R"PROTO(id: 1234 name: "table")PROTO", &preamble));
  EXPECT_THAT(P4ControlTableRefBuilder(preamble).Build(),
              EqualsProto(expected));
}

TEST(P4ProtoBuildersTest, P4ControlTableRefBuilderFromPreambleAndStage) {
  P4ControlTableRef expected;
  CHECK_OK(ParseProtoFromString(R"PROTO(
    table_id: 1234
    table_name: "table"
    pipeline_stage: VLAN_ACL
  )PROTO", &expected));

  ::p4::config::v1::Preamble preamble;
  CHECK_OK(
      ParseProtoFromString(R"PROTO(id: 1234 name: "table")PROTO", &preamble));
  EXPECT_THAT(
      P4ControlTableRefBuilder(preamble, P4Annotation::VLAN_ACL).Build(),
      EqualsProto(expected));
}

TEST(P4ProtoBuildersTest, P4ControlTableRefBuilderFromTable) {
  P4ControlTableRef expected;
  CHECK_OK(ParseProtoFromString(
      R"PROTO(table_id: 1234 table_name: "table")PROTO", &expected));

  ::p4::config::v1::Table table;
  CHECK_OK(ParseProtoFromString(
      R"PROTO(preamble { id: 1234 name: "table" })PROTO", &table));
  EXPECT_THAT(P4ControlTableRefBuilder(table).Build(), EqualsProto(expected));
}

TEST(P4ProtoBuildersTest, P4ControlTableRefBuilderFromTableAndStage) {
  P4ControlTableRef expected;
  CHECK_OK(ParseProtoFromString(R"PROTO(
    table_id: 1234
    table_name: "table"
    pipeline_stage: VLAN_ACL
  )PROTO", &expected));

  ::p4::config::v1::Table table;
  CHECK_OK(ParseProtoFromString(
      R"PROTO(preamble { id: 1234 name: "table" })PROTO", &table));
  EXPECT_THAT(P4ControlTableRefBuilder(table, P4Annotation::VLAN_ACL).Build(),
              EqualsProto(expected));
}

TEST(P4ProtoBuildersTest, P4ControlTableRefBuilderMixed) {
  P4ControlTableRef expected;
  CHECK_OK(ParseProtoFromString(R"PROTO(
    table_id: 1234
    table_name: "table"
    pipeline_stage: VLAN_ACL)PROTO", &expected));

  EXPECT_THAT(P4ControlTableRefBuilder()
                  .Id(1234)
                  .Name("table")
                  .Stage(P4Annotation::VLAN_ACL)
                  .Build(),
              EqualsProto(expected));
}

// HitBuilder tests.
TEST(P4ProtoBuildersTest, HitBuilderEmpty) {
  P4ControlStatement expected;

  EXPECT_THAT(HitBuilder().Build(), EqualsProto(expected));
}

TEST(P4ProtoBuildersTest, HitBuilderOnHit) {
  P4ControlStatement expected;
  CHECK_OK(ParseProtoFromString(
      absl::Substitute(
          R"PROTO(
            branch { condition { hit { $0 } } true_block { statements { $1 } } }
          )PROTO", Table(1).ShortDebugString().c_str(),
          ApplyTable(2).ShortDebugString().c_str()),
      &expected));

  EXPECT_THAT(HitBuilder().OnHit(Table(1)).Do(ApplyTable(2)).Build(),
              EqualsProto(expected));
}

TEST(P4ProtoBuildersTest, HitBuilderOnHitUseFalse) {
  P4ControlStatement expected;
  CHECK_OK(ParseProtoFromString(
      absl::Substitute(R"PROTO(
        branch {
          condition { not_operator: true hit { $0 } }
          false_block { statements { $1 } }
        }
      )PROTO", Table(1).ShortDebugString().c_str(),
                       ApplyTable(2).ShortDebugString().c_str()),
      &expected));

  EXPECT_THAT(HitBuilder().UseFalse().OnHit(Table(1)).Do(ApplyTable(2)).Build(),
              EqualsProto(expected));
}

TEST(P4ProtoBuildersTest, HitBuilderOnMiss) {
  P4ControlStatement expected;
  CHECK_OK(ParseProtoFromString(
      absl::Substitute(R"PROTO(
        branch {
          condition { not_operator: true hit { $0 } }
          true_block { statements { $1 } }
        }
      )PROTO", Table(1).ShortDebugString().c_str(),
                       ApplyTable(2).ShortDebugString().c_str()),
      &expected));

  EXPECT_THAT(HitBuilder().OnMiss(Table(1)).Do(ApplyTable(2)).Build(),
              EqualsProto(expected));
}

TEST(P4ProtoBuildersTest, HitBuilderOnMissUseFalse) {
  P4ControlStatement expected;
  CHECK_OK(ParseProtoFromString(
      absl::Substitute(R"PROTO(
        branch {
          condition { not_operator: false hit { $0 } }
          false_block { statements { $1 } }
        }
      )PROTO", Table(1).ShortDebugString().c_str(),
                       ApplyTable(2).ShortDebugString().c_str()),
      &expected));

  EXPECT_THAT(
      HitBuilder().OnMiss(Table(1)).Do(ApplyTable(2)).UseFalse().Build(),
      EqualsProto(expected));
}

TEST(P4ProtoBuildersTest, HitBuilderOnMissOverwritesOnHit) {
  P4ControlStatement expected;
  CHECK_OK(ParseProtoFromString(
      absl::Substitute(R"PROTO(
        branch {
          condition { not_operator: false hit { $0 } }
          false_block { statements { $1 } }
        }
      )PROTO", Table(1).ShortDebugString().c_str(),
                       ApplyTable(2).ShortDebugString().c_str()),
      &expected));

  EXPECT_THAT(HitBuilder()
                  .OnHit(Table(1))
                  .OnMiss(Table(1))
                  .Do(ApplyTable(2))
                  .UseFalse()
                  .Build(),
              EqualsProto(expected));
}

TEST(P4ProtoBuildersTest, HitBuilderOnHitOverwritesOnMiss) {
  P4ControlStatement expected;
  CHECK_OK(ParseProtoFromString(
      absl::Substitute(
          R"PROTO(
            branch { condition { hit { $0 } } true_block { statements { $1 } } }
          )PROTO", Table(1).ShortDebugString().c_str(),
          ApplyTable(2).ShortDebugString().c_str()),
      &expected));

  EXPECT_THAT(
      HitBuilder().OnMiss(Table(1)).Do(ApplyTable(2)).OnHit(Table(1)).Build(),
      EqualsProto(expected));
}

TEST(P4ProtoBuildersTest, HitBuilderControlBlock) {
  P4ControlStatement expected;
  CHECK_OK(ParseProtoFromString(
      absl::Substitute(
          R"PROTO(
            branch { condition { hit { $0 } } true_block { statements { $1 } } }
          )PROTO", Table(1).ShortDebugString().c_str(),
          ApplyTable(2).ShortDebugString().c_str()),
      &expected));

  hal::P4ControlBlock block;
  *block.add_statements() = ApplyTable(2);
  EXPECT_THAT(HitBuilder().OnHit(Table(1)).ControlBlock(block).Build(),
              EqualsProto(expected));
}

TEST(P4ProtoBuildersTest, HitBuilderMultipleActions) {
  P4ControlStatement expected;
  CHECK_OK(ParseProtoFromString(
      absl::Substitute(R"PROTO(
        branch {
          condition { not_operator: true hit { $0 } }
          true_block { statements { $1 } statements { $2 } }
        }
      )PROTO", Table(1).ShortDebugString().c_str(),
                       ApplyTable(2).ShortDebugString().c_str(),
                       ApplyTable(3).ShortDebugString().c_str()),
      &expected));

  EXPECT_THAT(
      HitBuilder().OnMiss(Table(1)).Do(ApplyTable(2)).Do(ApplyTable(3)).Build(),
      EqualsProto(expected));
}

// IsValidBuilder tests.
TEST(P4ProtoBuildersTest, IsValidBuilderEmpty) {
  P4ControlStatement expected;
  CHECK_OK(ParseProtoFromString(R"PROTO(
    branch {
      condition {
        is_valid {
          header_name: "P4_HEADER_UNKNOWN"
          header_type: P4_HEADER_UNKNOWN
        }
      }
    }
  )PROTO", &expected));

  EXPECT_THAT(IsValidBuilder().Build(), EqualsProto(expected));
}

// IsValidBuilder tests.
TEST(P4ProtoBuildersTest, IsValidBuilderValidControlBlock) {
  P4ControlStatement expected;
  CHECK_OK(ParseProtoFromString(
      absl::Substitute(R"PROTO(
        branch {
          condition {
            is_valid {
              header_name: "P4_HEADER_UNKNOWN"
              header_type: P4_HEADER_UNKNOWN
            }
          }
          true_block { statements { $0 } }
        }
      )PROTO", ApplyTable(1).ShortDebugString().c_str()), &expected));
  hal::P4ControlBlock control_block;
  *control_block.add_statements() = ApplyTable(1);
  EXPECT_THAT(IsValidBuilder().ValidControlBlock(control_block).Build(),
              EqualsProto(expected));
}

TEST(P4ProtoBuildersTest, IsValidBuilderInvalidControlBlock) {
  P4ControlStatement expected;
  CHECK_OK(ParseProtoFromString(
      absl::Substitute(R"PROTO(
        branch {
          condition {
            is_valid {
              header_name: "P4_HEADER_UNKNOWN"
              header_type: P4_HEADER_UNKNOWN
            }
          }
          false_block { statements { $0 } }
        }
      )PROTO", ApplyTable(1).ShortDebugString().c_str()), &expected));
  hal::P4ControlBlock control_block;
  *control_block.add_statements() = ApplyTable(1);
  EXPECT_THAT(IsValidBuilder().InvalidControlBlock(control_block).Build(),
              EqualsProto(expected));
}

TEST(P4ProtoBuildersTest, IsValidBuilderDoIfValid_Statement) {
  P4ControlStatement expected;
  CHECK_OK(ParseProtoFromString(
      absl::Substitute(R"PROTO(
        branch {
          condition {
            is_valid {
              header_name: "P4_HEADER_UNKNOWN"
              header_type: P4_HEADER_UNKNOWN
            }
          }
          true_block { statements { $0 } }
        }
      )PROTO", ApplyTable(1).ShortDebugString().c_str()), &expected));
  EXPECT_THAT(IsValidBuilder().DoIfValid(ApplyTable(1)).Build(),
              EqualsProto(expected));
}

TEST(P4ProtoBuildersTest, IsValidBuilderDoIfValid_Block) {
  P4ControlBlock block;
  *block.add_statements() = ApplyTable(1);
  *block.add_statements() = ApplyTable(2);
  P4ControlStatement expected;
  CHECK_OK(ParseProtoFromString(
      absl::Substitute(
          R"PROTO(
            branch {
              condition {
                is_valid {
                  header_name: "P4_HEADER_UNKNOWN"
                  header_type: P4_HEADER_UNKNOWN
                }
              }
              true_block { statements { $0 } statements { $1 } }
            }
          )PROTO", ApplyTable(1).ShortDebugString().c_str(),
          ApplyTable(2).ShortDebugString().c_str()),
      &expected));
  EXPECT_THAT(
      IsValidBuilder().DoIfValid(block).Build(),
      EqualsProto(expected));
}

TEST(P4ProtoBuildersTest, IsValidBuilderDoIfInvalid_Statement) {
  P4ControlStatement expected;
  CHECK_OK(ParseProtoFromString(
      absl::Substitute(R"PROTO(
        branch {
          condition {
            is_valid {
              header_name: "P4_HEADER_UNKNOWN"
              header_type: P4_HEADER_UNKNOWN
            }
          }
          false_block { statements { $0 } }
        }
      )PROTO", ApplyTable(1).ShortDebugString().c_str()), &expected));
  EXPECT_THAT(IsValidBuilder().DoIfInvalid(ApplyTable(1)).Build(),
              EqualsProto(expected));
}

TEST(P4ProtoBuildersTest, IsValidBuilderDoIfInvalid_Block) {
  P4ControlBlock block;
  *block.add_statements() = ApplyTable(1);
  *block.add_statements() = ApplyTable(2);
  P4ControlStatement expected;
  CHECK_OK(ParseProtoFromString(
      absl::Substitute(
          R"PROTO(
            branch {
              condition {
                is_valid {
                  header_name: "P4_HEADER_UNKNOWN"
                  header_type: P4_HEADER_UNKNOWN
                }
              }
              false_block { statements { $0 } statements { $1 } }
            }
          )PROTO", ApplyTable(1).ShortDebugString().c_str(),
          ApplyTable(2).ShortDebugString().c_str()),
      &expected));
  EXPECT_THAT(
      IsValidBuilder().DoIfInvalid(block).Build(),
      EqualsProto(expected));
}

TEST(P4ProtoBuildersTest, IsValidBuilderDoIfValidMultipleActions) {
  P4ControlStatement expected;
  CHECK_OK(ParseProtoFromString(
      absl::Substitute(R"PROTO(
        branch {
          condition {
            is_valid {
              header_name: "P4_HEADER_UNKNOWN"
              header_type: P4_HEADER_UNKNOWN
            }
          }
          true_block { statements { $0 } statements { $1 } }
        }
      )PROTO", ApplyTable(1).ShortDebugString().c_str(),
                       ApplyTable(2).ShortDebugString().c_str()),
      &expected));
  EXPECT_THAT(IsValidBuilder()
                  .DoIfValid(ApplyTable(1))
                  .DoIfValid(ApplyTable(2))
                  .Build(),
              EqualsProto(expected));
}

TEST(P4ProtoBuildersTest, IsValidBuilderDoIfInvalidMultipleActions) {
  P4ControlStatement expected;
  CHECK_OK(ParseProtoFromString(
      absl::Substitute(R"PROTO(
        branch {
          condition {
            is_valid {
              header_name: "P4_HEADER_UNKNOWN"
              header_type: P4_HEADER_UNKNOWN
            }
          }
          false_block { statements { $0 } statements { $1 } }
        }
      )PROTO", ApplyTable(1).ShortDebugString().c_str(),
                       ApplyTable(2).ShortDebugString().c_str()),
      &expected));
  EXPECT_THAT(IsValidBuilder()
                  .DoIfInvalid(ApplyTable(1))
                  .DoIfInvalid(ApplyTable(2))
                  .Build(),
              EqualsProto(expected));
}

TEST(P4ProtoBuildersTest, IsValidBuilderDoIfValidUseNot) {
  P4ControlStatement expected;
  CHECK_OK(ParseProtoFromString(
      absl::Substitute(R"PROTO(
        branch {
          condition {
            not_operator: true
            is_valid {
              header_name: "P4_HEADER_UNKNOWN"
              header_type: P4_HEADER_UNKNOWN
            }
          }
          false_block { statements { $0 } }
        }
      )PROTO", ApplyTable(1).ShortDebugString().c_str()), &expected));
  EXPECT_THAT(IsValidBuilder().UseNot().DoIfValid(ApplyTable(1)).Build(),
              EqualsProto(expected));
}

TEST(P4ProtoBuildersTest, IsValidBuilderDoIfInvalidUseNot) {
  P4ControlStatement expected;
  CHECK_OK(ParseProtoFromString(
      absl::Substitute(R"PROTO(
        branch {
          condition {
            not_operator: true
            is_valid {
              header_name: "P4_HEADER_UNKNOWN"
              header_type: P4_HEADER_UNKNOWN
            }
          }
          true_block { statements { $0 } }
        }
      )PROTO", ApplyTable(1).ShortDebugString().c_str()), &expected));
  EXPECT_THAT(IsValidBuilder().DoIfInvalid(ApplyTable(1)).UseNot().Build(),
              EqualsProto(expected));
}

TEST(P4ProtoBuildersTest, IsValidBuilderHeader) {
  P4ControlStatement expected;
  CHECK_OK(ParseProtoFromString(
      R"PROTO(
        branch {
          condition {
            is_valid {
              header_name: "P4_HEADER_IPV4"
              header_type: P4_HEADER_IPV4
            }
          }
        }
      )PROTO", &expected));
  EXPECT_THAT(IsValidBuilder().Header(P4_HEADER_IPV4).Build(),
              EqualsProto(expected));
}

TEST(P4ProtoBuildersTest, IsValidBuilderComplexBlock) {
  P4ControlStatement expected;
  CHECK_OK(ParseProtoFromString(
      absl::Substitute(R"PROTO(
        branch {
          condition {
            not_operator: true
            is_valid {
              header_name: "P4_HEADER_IPV4"
              header_type: P4_HEADER_IPV4
            }
          }
          true_block { statements { $0 } statements { $1 } }
          false_block { statements { $2 } statements { $3 } }
        }
      )PROTO", ApplyTable(1).ShortDebugString().c_str(),
                       ApplyTable(2).ShortDebugString().c_str(),
                       ApplyTable(3).ShortDebugString().c_str(),
                       ApplyTable(4).ShortDebugString().c_str()),
      &expected));
  IsValidBuilder is_valid_builder;
  EXPECT_THAT(IsValidBuilder()
                  .Header(P4_HEADER_IPV4)
                  .UseNot()
                  .DoIfInvalid(ApplyTable(1))
                  .DoIfInvalid(ApplyTable(2))
                  .DoIfValid(ApplyTable(3))
                  .DoIfValid(ApplyTable(4))
                  .Build(),
              EqualsProto(expected));
}

}  // namespace
}  // namespace p4_proto_builders
}  // namespace test_utils
}  // namespace stratum
