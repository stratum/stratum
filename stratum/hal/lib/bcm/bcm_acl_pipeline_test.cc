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


#include "stratum/hal/lib/bcm/bcm_acl_pipeline.h"

#include "stratum/glue/status/status_test_util.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/p4/p4_control.pb.h"
#include "stratum/lib/utils.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"

namespace stratum {
namespace hal {
namespace bcm {
namespace {

// The default P4ControlBlock implements the following program:
// apply {
//   table1_hit = table1.apply().hit;
//   if (!table2.apply().hit) {
//     table3.apply();
//   }
//   if (table4.apply().hit) {
//   } else {
//     if (!table5.apply().hit) {
//       table6.apply();
//     }
//   }
//   table7.apply();
//   if (!table1_hit) {
//     table8.apply();
//   }
// }
const char kDefaultP4ControlBlock[] = R"PROTO(
    statements {
      apply { table_name: "table1" table_id: 1 }
    }
    statements {
      apply { table_name: "table2" table_id: 2 }
    }
    statements {
      branch {
        condition {
          not_operator: true
          hit { table_name: "table2" table_id: 2 }
        }
        true_block {
          statements {
            apply { table_name: "table3" table_id: 3 }
          }
        }
      }
    }
    statements {
      apply { table_name: "table4" table_id: 4 }
    }
    statements {
      branch {
        condition {
          hit { table_name: "table4" table_id: 4 }
        }
        false_block {
          statements {
            apply { table_name: "table5" table_id: 5 }
          }
          statements {
            branch {
              condition {
                not_operator: true
                hit { table_name: "table5" table_id: 5 }
              }
              true_block {
                statements {
                  apply { table_name: "table6" table_id: 6 }
                }
              }
            }
          }
        }
      }
    }
    statements {
      apply { table_name: "table7" table_id: 7 }
    }
    statements {
      branch {
        condition {
          not_operator: true
          hit { table_name: "table1" table_id: 1 }
        }
        true_block {
          statements {
            apply { table_name: "table8" table_id: 8 }
          }
        }
      }
    })PROTO";

const char kDefaultP4ControlBlockString[] =
    R"(
table1 --> table2 --> table4 --> table7
(   2)     (   4)     (   7)     (   8)
    |          |          |
table8     table3     table5
(   1)     (   3)     (   6)
                          |
                      table6
                      (   5))";

// Verify the that BcmAclPipeline creates the default control block from
// the provided P4ControlBlock.
void VerifyDefaultPipeline(const P4ControlBlock& control_block) {
  auto pipeline_result = BcmAclPipeline::CreateBcmAclPipeline(control_block);
  ASSERT_OK(pipeline_result.status()) << pipeline_result.status();
  std::unique_ptr<BcmAclPipeline> pipeline =
      pipeline_result.ConsumeValueOrDie();
  ASSERT_NE(nullptr, pipeline);
  string pipeline_string = pipeline->LogicalPipelineAsString();
  EXPECT_EQ(kDefaultP4ControlBlockString, "\n" + pipeline_string);
}

// Tests the logical pipeline factory function can process a valid pipeline.
TEST(BcmAclPipelineTest, ConstructValidPipeline) {
  P4ControlBlock control_block;
  ASSERT_OK(ParseProtoFromString(kDefaultP4ControlBlock, &control_block));
  VerifyDefaultPipeline(control_block);
}

// The logical pipeline generation should fail to process an is_valid branch.
TEST(BcmAclPipelineTest, IsValidBranchFailure) {
  P4ControlBlock control_block;
  const char kControlBlock[] = R"PROTO(
    statements {
      apply {table_name: "a" table_id: 1 }
    }
    statements {
      branch {
        condition {
          is_valid {}
        }
      }
    })PROTO";
  ASSERT_OK(ParseProtoFromString(kControlBlock, &control_block));
  EXPECT_FALSE(BcmAclPipeline::CreateBcmAclPipeline(control_block).ok());
}

// The logical pipeline generation should fail to process a hit branch.
TEST(BcmAclPipelineTest, HitTrueBranchFailure) {
  P4ControlBlock control_block;
  const char kControlBlock[] = R"PROTO(
    statements {
      apply {table_name: "a" table_id: 1 }
    }
    statements {
      branch {
        condition {
          hit { table_name: "a" table_id: 1 }
        }
        true_block {
          statements {
            apply { table_name: "b" table_id: 2 }
          }
        }
      }
    })PROTO";
  ASSERT_OK(ParseProtoFromString(kControlBlock, &control_block));
  EXPECT_FALSE(BcmAclPipeline::CreateBcmAclPipeline(control_block).ok());
}

// The logical pipeline generation should fail to process a hit branch.
TEST(BcmAclPipelineTest, MissFalseBranchFailure) {
  P4ControlBlock control_block;
  const char kControlBlock[] = R"PROTO(
    statements {
      apply {table_name: "a" table_id: 1 }
    }
    statements {
      branch {
        condition {
          not_operator: true
          hit {table_name: "a" table_id: 1 }
        }
        false_block {
          statements {
            apply { table_name: "b" table_id: 2 }
          }
        }
      }
    })PROTO";
  ASSERT_OK(ParseProtoFromString(kControlBlock, &control_block));
  EXPECT_FALSE(BcmAclPipeline::CreateBcmAclPipeline(control_block).ok());
}

// The logical pipeline generation should fail if a miss condition references an
// unknown table.
TEST(BcmAclPipelineTest, HitUnknownTableFailure) {
  P4ControlBlock control_block;
  const char kControlBlock[] = R"PROTO(
    statements {
      branch {
        condition {
          hit { table_name: "a" table_id: 1 }
        }
        false_block {
          statements {
            apply { table_name: "b" table_id: 2 }
          }
        }
      }
    })PROTO";
  ASSERT_OK(ParseProtoFromString(kControlBlock, &control_block));
  EXPECT_FALSE(BcmAclPipeline::CreateBcmAclPipeline(control_block).ok());
}

// The logical pipeline generation should fail if there are multiple miss
// dependencies on the same table from separate branch statements.
TEST(BcmAclPipelineTest, SeparateHitMultipleDependencyFailure) {
  P4ControlBlock control_block;
  const char kControlBlock[] = R"PROTO(
    statements {
      apply {table_name: "a" table_id: 1 }
    }
    statements {
      branch {
        condition {
          hit { table_name: "a" table_id: 1 }
        }
        false_block {
          statements {
            apply { table_name: "b" table_id: 2 }
          }
        }
      }
    }
    statements {
      branch {
        condition {
          hit { table_name: "a" table_id: 1 }
        }
        false_block {
          statements {
            apply { table_name: "c" table_id: 3 }
          }
        }
      }
    })PROTO";
  ASSERT_OK(ParseProtoFromString(kControlBlock, &control_block));
  EXPECT_FALSE(BcmAclPipeline::CreateBcmAclPipeline(control_block).ok());
}

// The logical pipeline generation should fail if there are multiple miss
// dependencies on the same table from the same branch statement.
TEST(BcmAclPipelineTest, CombinedHitMultipleDependencyFailure) {
  P4ControlBlock control_block;
  const char kControlBlock[] = R"PROTO(
    statements {
      apply {table_name: "a" table_id: 1 }
    }
    statements {
      branch {
        condition {
          hit { table_name: "a" table_id: 1 }
        }
        false_block {
          statements {
            apply { table_name: "b" table_id: 2 }
          }
          statements {
            apply { table_name: "c" table_id: 3 }
          }
        }
      }
    })PROTO";
  ASSERT_OK(ParseProtoFromString(kControlBlock, &control_block));
  EXPECT_FALSE(BcmAclPipeline::CreateBcmAclPipeline(control_block).ok());
}

// The logical pipeline generation should fail if there are multiple apply
// statements for the same table.
TEST(BcmAclPipelineTest, MultipleTableApplyFailure) {
  P4ControlBlock control_block;
  const char kControlBlock[] = R"PROTO(
    statements {
      apply {table_name: "a" table_id: 1 }
    }
    statements {
      apply {table_name: "a" table_id: 1 }
    })PROTO";
  ASSERT_OK(ParseProtoFromString(kControlBlock, &control_block));
  EXPECT_FALSE(BcmAclPipeline::CreateBcmAclPipeline(control_block).ok());
}

// The logical pipeline generation should ignore unknown condition.
TEST(BcmAclPipelineTest, IgnoreUnknownCondition) {
  P4ControlBlock control_block;
  ASSERT_OK(ParseProtoFromString(kDefaultP4ControlBlock, &control_block));
  // Add an unknown branch as the second-to-last statement in the control block
  // so a meaningful statement follows.
  control_block.add_statements()
      ->mutable_branch()
      ->mutable_condition()
      ->set_unknown("mid_unknown");
  control_block.mutable_statements()->SwapElements(
      control_block.statements_size() - 1, control_block.statements_size() - 2);
  // Add another unknown branch to the end of the control block.
  control_block.add_statements()
      ->mutable_branch()
      ->mutable_condition()
      ->set_unknown("end_unknown");

  VerifyDefaultPipeline(control_block);
}

// The logical pipeline generation should ignore "other" statements.
TEST(BcmAclPipelineTest, IgnoreOtherStatements) {
  P4ControlBlock control_block;
  ASSERT_OK(ParseProtoFromString(kDefaultP4ControlBlock, &control_block));
  // Add an "other" statement as the second-to-last statement in the control
  // block so a meaningful statement follows.
  control_block.add_statements()->set_other("mid_other");
  control_block.mutable_statements()->SwapElements(
      control_block.statements_size() - 1, control_block.statements_size() - 2);
  // Add another "other" statement to the end of the control block.
  control_block.add_statements()->set_other("end_other");

  VerifyDefaultPipeline(control_block);
}

}  // namespace
}  // namespace bcm
}  // namespace hal
}  // namespace stratum
