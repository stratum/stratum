/*
 * Copyright 2018 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


// P4StaticEntryMapper is a P4TableMapper helper class.  Similar to
// P4TableMapper, one instance exists per configured P4 device ID.
// P4StaticEntryMapper manages the static table entries, i.e. those defined
// as "const entries" in the P4 program, for the device it represents.  The
// P4StaticEntryMapper recognizes two types of static entries.  The first
// type consists of entries that get programmed directly into the switch's
// physical tables, such as the cluster MAC entry for L2 lookups.  The second
// type consists of entries in "hidden" tables.  These entries are never
// directly programmed into physical tables.  Instead, they typically get
// combined into actions for other physical tables.  Encap/decap operations
// in Stratum P4 programs are an example of this type.  Both types of static
// entries can be affected by changes to the P4PipelineConfig.
//
// P4StaticEntryMapper is platform independent.  It provides common information
// to help the target switch implementation program static flows, but the
// details of programming these flows remain a target-specific responsibility.

#ifndef STRATUM_HAL_LIB_P4_P4_STATIC_ENTRY_MAPPER_H_
#define STRATUM_HAL_LIB_P4_P4_STATIC_ENTRY_MAPPER_H_

#include "stratum/glue/status/status.h"
#include "stratum/hal/lib/p4/p4_pipeline_config.pb.h"
#include "p4/v1/p4runtime.pb.h"

namespace stratum {
namespace hal {

// Forward reference for closely-coupled class, breaking circular dependency.
// TODO: There are a couple of potential ways to resolve this,
// depending on future P4StaticEntryMapper implementation for tunneling.
// At present, P4StaticEntryMapper only depends on IsTableStageHidden from
// P4TableMapper.  This could be reworked to pass in a set of
// hidden table IDs instead.  Tunneling may add other dependencies, which
// could be addressed by having P4TableMapper inherit from a small
// interface class to inject into P4StaticEntryMapper.
class P4TableMapper;

// When constructed, each instance of P4StaticEntryMapper is injected with
// the P4TableMapper associated with the same device.  Thereafter,
// the P4StaticEntryMapper maintains the state of P4Runtime WriteRequests that
// represent that static table entries in the P4 program.  It gets involved
// in the P4PipelineConfig push process to identify any changes that delete,
// change, or modify static table entries.
class P4StaticEntryMapper {
 public:
  explicit P4StaticEntryMapper(P4TableMapper* p4_table_mapper);
  virtual ~P4StaticEntryMapper() {}

  // P4StaticEntryMapper does not provide a verify method.  It depends on
  // P4ConfigVerifier to do pre-push verification of static entries relative
  // to other parts of the P4PipelineConfig.
  // TODO: It may be beneficial for P4ConfigVerifier to be able to
  // call a Verify method in this class for assistance.

  // These two methods support P4PipelineConfig pushes.  The role of these
  // methods relative to the overall pipeline config push is described by
  // the P4TableMapper interface for static table entries.
  // P4TableMapper's HandlePrePushStaticEntryChanges and
  // HandlePostPushStaticEntryChanges methods are wrappers to these functions.
  //
  // Both HandlePrePushChanges and HandlePostPushChanges modify the
  // internal state of P4StaticEntryMapper with the expectation that the caller
  // will proceed to act on the output.  Thus, two successive calls to
  // HandlePrePushChanges with the same parameter values will yield different
  // output.  The second call yields an empty out_request because the
  // internal state accounts for the deleted entries from the first call.  Both
  // methods return an OK status to indicate success, and the output messages
  // contain the entries for the caller to change, if any.  Either method can
  // return ERR_REBOOT_REQUIRED if they detect some reason a change cannot be
  // accomplished.  Neither method produces any output for entries in hidden,
  // non-physical tables, but a change in the entries for such tables results
  // in an ERR_REBOOT_REQUIRED status.
  virtual ::util::Status HandlePrePushChanges(
      const ::p4::v1::WriteRequest& new_static_config,
      ::p4::v1::WriteRequest* out_request);
  virtual ::util::Status HandlePostPushChanges(
      const ::p4::v1::WriteRequest& new_static_config,
      ::p4::v1::WriteRequest* out_request);

  // P4StaticEntryMapper is neither copyable nor movable.
  P4StaticEntryMapper(const P4StaticEntryMapper&) = delete;
  P4StaticEntryMapper& operator=(const P4StaticEntryMapper&) = delete;

 protected:
  // Default constructor. To be called by the Mock class instance only.
  P4StaticEntryMapper();

 private:
  // Splits the input new_request into two P4 WriteRequests, one with
  // static entries for physical tables and one with static entries for
  // hidden tables.
  ::util::Status SplitRequest(const ::p4::v1::WriteRequest& new_request,
                              ::p4::v1::WriteRequest* physical_request,
                              ::p4::v1::WriteRequest* hidden_request);

  // This member is the injected table mapper for the P4 device.
  P4TableMapper* p4_table_mapper_;

  // This P4 WriteRequest contains the entries that are currently programmed
  // into physical tables in the hardware pipeline.  These entries may differ
  // from the P4PipelineConfig's static_table_entries when entries for
  // "hidden" tables are present or when a P4PipelineConfig push is in
  // progress.
  ::p4::v1::WriteRequest physical_static_entries_;

  // This P4 WriteRequest contains the subset of entries that apply only
  // to hidden non-physical tables.
  // TODO: These probably need to be known by p4_table_mapper_,
  // but this form facilitates easier change detection during P4PipelineConfig
  // pushes.
  ::p4::v1::WriteRequest hidden_static_entries_;
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_P4_P4_STATIC_ENTRY_MAPPER_H_
