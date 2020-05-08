// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

// The P4WriteRequestDiffer class compares a pair of P4 WriteRequest
// messages. Its typical use case is to find any differences between
// P4 WriteRequests that represent static table entries from different
// versions of the Stratum P4 program.

#ifndef STRATUM_HAL_LIB_P4_P4_WRITE_REQUEST_DIFFER_H_
#define STRATUM_HAL_LIB_P4_P4_WRITE_REQUEST_DIFFER_H_

#include <set>
#include <vector>

#include "google/protobuf/message.h"
#include "google/protobuf/util/message_differencer.h"
#include "p4/v1/p4runtime.pb.h"
#include "stratum/glue/status/status.h"

namespace stratum {
namespace hal {

// The P4WriteRequestDiffer constructor accepts two P4 WriteRequest messages
// for comparison.  In typical usage, the old_request contains the static
// table entries from the P4PipelineConfig for the running version of the P4
// program, and new_request contains potential new static entries from the
// latest P4PipelineConfig push.  The GenerateAddAndDeleteRequests method
// compares the injected WriteRequests and outputs WriteRequests that contain
// only the differences.
class P4WriteRequestDiffer {
 public:
  // The constructor takes the pair of P4 runtime WriteRequests to compare.
  // The caller needs to keep these requests in scope throughout the lifetime
  // of the P4WriteRequestDiffer instance.
  P4WriteRequestDiffer(const ::p4::v1::WriteRequest& old_request,
                       const ::p4::v1::WriteRequest& new_request);
  virtual ~P4WriteRequestDiffer() {}

  // This method compares the P4 WriteRequest pair, as injected to the
  // constructor, and generates output messages with the changes, if any.  It
  // returns an OK status after successfully comparing the two messages,
  // regardless of whether any differences occurred.  If the two injected
  // requests are identical, the delete_request, modify_request, and add_request
  // outputs will all be empty upon return, and unchanged_request will contain
  // a copy of the original old_request.  Otherwise, the outputs are as follows:
  //  delete_request - contains any entries in the injected old_request that
  //      do not appear in the injected new_request.  Updates in this output
  //      have type DELETE.
  //  add_request - contains any entries in new_request that were not part of
  //      old_request.  Updates in this output have type INSERT.
  //  modify_request - contains any entries that appear in both old_request
  //      and new_request, but have different field values.  To evaluate
  //      whether updates in old_request and new_request refer to the same
  //      static entry, P4WriteRequestDiffer forms a key from the entry's
  //      table_id plus the set of all the entry's match fields.  Updates in
  //      this output have type MODIFY.
  //  unchanged_request - contains static entries that do not vary between
  //      old_request and new_request.  This output includes all updates
  //      that are in a different order in the old and new requests, but have
  //      no other field changes.Updates in this output have the same type
  //      as the input request.
  // The caller can selectively choose to disable any output by passing nullptr.
  ::util::Status Compare(::p4::v1::WriteRequest* delete_request,
                         ::p4::v1::WriteRequest* add_request,
                         ::p4::v1::WriteRequest* modify_request,
                         ::p4::v1::WriteRequest* unchanged_request);

  // P4WriteRequestDiffer is neither copyable nor movable.
  P4WriteRequestDiffer(const P4WriteRequestDiffer&) = delete;
  P4WriteRequestDiffer& operator=(const P4WriteRequestDiffer&) = delete;

 private:
  // DoCompare is a private helper function for Compare.
  ::util::Status DoCompare(::p4::v1::WriteRequest* delete_request,
                           ::p4::v1::WriteRequest* add_request,
                           ::p4::v1::WriteRequest* modify_request,
                           ::p4::v1::WriteRequest* unchanged_request);

  // Populates output_request with data accumulated by a P4WriteRequestReporter
  // during comparison of the old_request_/new_request_ pair.
  void FillOutputFromReporterIndexes(
      const ::p4::v1::WriteRequest& source_request,
      const std::vector<int>& reporter_indexes, ::p4::v1::Update::Type type,
      ::p4::v1::WriteRequest* output_request);

  // These members refer to the two WriteRequests for comparison.
  const ::p4::v1::WriteRequest& old_request_;
  const ::p4::v1::WriteRequest& new_request_;
};

// P4WriteRequestDiffer attaches this Reporter subclass to a MessageDifferencer
// in order to identify P4 WriteRequest entries that have changed.  The
// P4WriteRequestReporter expects its enclosing MessageDifferencer to be
// using the "AS_SET" mode of repeated field comparison.
class P4WriteRequestReporter
    : public ::google::protobuf::util::MessageDifferencer::Reporter {
 public:
  // This type simplifies the declaration of the member overrides below.
  typedef std::vector<
      ::google::protobuf::util::MessageDifferencer::SpecificField>
      FieldVector;

  P4WriteRequestReporter();
  ~P4WriteRequestReporter() override {}

  // Reports that a field has been added into message2.
  void ReportAdded(const ::google::protobuf::Message& message1,
                   const ::google::protobuf::Message& message2,
                   const FieldVector& field_path) override;

  // Reports that a field has been deleted from message1.
  void ReportDeleted(const ::google::protobuf::Message& message1,
                     const ::google::protobuf::Message& message2,
                     const FieldVector& field_path) override;

  // Reports that a field in message1 has been modified by message2.
  void ReportModified(const ::google::protobuf::Message& message1,
                      const ::google::protobuf::Message& message2,
                      const FieldVector& field_path) override;

  // Reports that a field in message1 has been moved in message2, but no
  // other changes exist.
  void ReportMoved(const ::google::protobuf::Message& message1,
                   const ::google::protobuf::Message& message2,
                   const FieldVector& field_path) override;

  // Reports that message1 and message2 match.
  void ReportMatched(const ::google::protobuf::Message& message1,
                     const ::google::protobuf::Message& message2,
                     const FieldVector& field_path) override;

  // Accessors to containers with indices of added/deleted/modified/unchanged
  // fields.
  const std::vector<int>& deleted_indexes() const { return deleted_indexes_; }
  const std::vector<int>& added_indexes() const { return added_indexes_; }
  const std::vector<int>& modified_indexes() const { return modified_indexes_; }
  const std::set<int>& unchanged_indexes() const { return unchanged_indexes_; }

  // Accesses the status outcome following all reported additions, deletions,
  // and modifications in the message comparison.
  const ::util::Status& status() { return status_; }

  // P4WriteRequestReporter is neither copyable nor movable.
  P4WriteRequestReporter(const P4WriteRequestReporter&) = delete;
  P4WriteRequestReporter& operator=(const P4WriteRequestReporter&) = delete;

 private:
  // Maintains overall P4WriteRequestReporter status as this instance processes
  // reports of additions, deletions, and modifications.
  ::util::Status status_;

  // As the field comparison reports differences, P4WriteRequestReporter
  // accumulates field changes, or lack thereof, in these containers.  An index
  // in deleted_indexes_ is relative to message1 in ReportDeleted, an index
  // in added_indexes_ is relative to message2 in ReportAdded, and an index
  // in modified__indexes_ is relative to message2 in ReportModified.
  std::vector<int> deleted_indexes_;
  std::vector<int> added_indexes_;
  std::vector<int> modified_indexes_;
  std::set<int> unchanged_indexes_;
};

// P4WriteRequestDiffer attaches this MapKeyComparator subclass to a
// MessageDifferencer to compare two updates within a P4 WriteRequest.  The
// IsMatch override treats the update table_id and match fields as the
// comparison key, i.e. a P4 Update message pair with the same table_id and
// match field set is considered to be a match.  If a P4 Update message pair
// has the same key value according to IsMatch, but the message pair differs
// in other fields, then P4WriteRequestDiffer considers the change to be a
// modification of the same update.
class P4WriteRequestComparator
    : public ::google::protobuf::util::MessageDifferencer::MapKeyComparator {
 public:
  P4WriteRequestComparator() {}
  ~P4WriteRequestComparator() override{};

  bool IsMatch(const ::google::protobuf::Message& message1,
               const ::google::protobuf::Message& message2,
               const std::vector<
                   ::google::protobuf::util::MessageDifferencer::SpecificField>&
                   parent_fields) const override;

  // P4WriteRequestComparator is neither copyable nor movable.
  P4WriteRequestComparator(const P4WriteRequestComparator&) = delete;
  P4WriteRequestComparator& operator=(const P4WriteRequestComparator&) = delete;
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_P4_P4_WRITE_REQUEST_DIFFER_H_
