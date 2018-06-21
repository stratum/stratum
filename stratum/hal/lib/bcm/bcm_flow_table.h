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


#ifndef STRATUM_HAL_LIB_BCM_BCM_FLOW_TABLE_H_
#define STRATUM_HAL_LIB_BCM_BCM_FLOW_TABLE_H_

#include "stratum/glue/status/status_macros.h"
#include "stratum/lib/utils.h"
#include "stratum/public/lib/error.h"
#include "stratum/glue/integral_types.h"
#include "github.com/p4lang/PI/p4/v1/p4runtime.grpc.pb.h"
#include "stratum/glue/gtl/node_hash_set.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"

namespace stratum {
namespace hal {
namespace bcm {

// Custom hash and equal function for ::p4::v1::TableEntry protos. We need a way
// to differeniate flows in the following way: If we have 2 flows f1 and f2
// with f2 being the modified version of f1 as intended by the controller, f1
// = f2. In any other case they should not.
struct TableEntryHash {
  size_t operator()(const ::p4::v1::TableEntry& x) const {
    // Save a copy of the input proto, clear action and controller metadata
    // and hash the rest.
    ::p4::v1::TableEntry a = x;
    a.clear_table_id();
    a.clear_action();
    a.clear_controller_metadata();
    a.clear_meter_config();
    a.clear_counter_data();
    // Hash on match field combination, not on permutation.
    std::sort(a.mutable_match()->begin(), a.mutable_match()->end(),
              [](const ::p4::v1::FieldMatch& l, const ::p4::v1::FieldMatch& r) {
                return ProtoLess(l, r);
              });
    return ProtoHash(a);
  }
};

struct TableEntryEqual {
  bool operator()(const ::p4::v1::TableEntry& x, const ::p4::v1::TableEntry& y) const {
    // Save copies of the input protos, clear actions and controller metadata
    // and compare the rest.
    ::p4::v1::TableEntry a = x, b = y;
    a.clear_table_id();
    a.clear_action();
    a.clear_controller_metadata();
    a.clear_meter_config();
    a.clear_counter_data();
    b.clear_table_id();
    b.clear_action();
    b.clear_controller_metadata();
    b.clear_meter_config();
    b.clear_counter_data();

    // The order of the match fields are not important.
    if (a.match_size() != b.match_size() ||
        !std::is_permutation(
            a.match().begin(), a.match().end(), b.match().begin(),
            [](const ::p4::v1::FieldMatch& l, const ::p4::v1::FieldMatch& r) {
              return ProtoEqual(l, r);
            })) {
      return false;
    }
    a.clear_match();
    b.clear_match();

    return ProtoEqual(a, b);
  }
};

using TableEntrySet =
    gtl::node_hash_set<::p4::v1::TableEntry, TableEntryHash, TableEntryEqual>;

// Class for managing a BCM table.
class BcmFlowTable {
 public:
  // STL-style types that allow table traversal.
  using const_iterator = TableEntrySet::const_iterator;
  using value_type = TableEntrySet::value_type;

  // Constructors.
  explicit BcmFlowTable(uint32 p4_table_id)
      : id_(p4_table_id), name_(), entries_() {}

  BcmFlowTable(uint32 p4_table_id, absl::string_view name)
      : id_(p4_table_id), name_(name), entries_() {}

  explicit BcmFlowTable(const ::p4::config::v1::Table& table)
      : id_(table.preamble().id()),
        name_(table.preamble().name()),
        entries_() {}

  // Copy Constructor.
  BcmFlowTable(const BcmFlowTable& other)
      : id_(other.id_), name_(other.name_), entries_(other.entries_) {}

  // Move Constructor.
  BcmFlowTable(BcmFlowTable&& other)
      : id_(other.id_),
        name_(std::move(other.name_)),
        entries_(std::move(other.entries_)) {}

  // Destructor.
  virtual ~BcmFlowTable() {}

  //***************************************************************************
  //  Accessors
  //***************************************************************************

  // Returns the table's P4 ID.
  virtual uint32 Id() const { return id_; }

  // Returns the table's P4 name.
  virtual string Name() const { return name_; }

  // Returns true if this table already has this entry.
  virtual bool HasEntry(const ::p4::v1::TableEntry& entry) const {
    return entries_.count(entry) > 0;
  }

  // Returns the number of entries in this table.
  virtual int EntryCount() const { return entries_.size(); }

  // Returns true if this table has no entries.
  virtual bool Empty() const { return entries_.empty(); }

  // Returns the ::p4::v1::TableEntry that matches a given entry key.
  // Returns ERR_ENTRY_NOT_FOUND if a matching entry is not found.
  virtual util::StatusOr<p4::v1::TableEntry> Lookup(
      const ::p4::v1::TableEntry& key) const {
    auto lookup = entries_.find(key);
    if (lookup == entries_.end()) {
      return MAKE_ERROR(ERR_ENTRY_NOT_FOUND)
             << TableStr()
             << " does not contain TableEntry: " << key.ShortDebugString();
    }
    return *lookup;
  }

  TableEntrySet::const_iterator begin() const { return entries_.begin(); }
  TableEntrySet::const_iterator end() const { return entries_.end(); }

  //***************************************************************************
  //  Table Entry Management
  //***************************************************************************

  // Attempts to add the entry to this table.
  // Returns ERR_ENTRY_EXISTS if a matching entry already exists.
  // Returns an error if the entry cannot otherwise be added.
  //
  // An entry may match an existing entry if all of the following values match:
  // 1) TableEntry.match (all matches)
  // 2) TableEntry.priority
  // 3) is_default_action
  //
  // See TableEntryEqual below.
  virtual util::Status InsertEntry(const ::p4::v1::TableEntry& entry) {
    auto result = entries_.insert(entry);
    if (!result.second) {
      return MAKE_ERROR(ERR_ENTRY_EXISTS)
             << TableStr() << " contains duplicate of TableEntry: "
             << entry.ShortDebugString()
             << ". Matching TableEntry: " << result.first->ShortDebugString()
             << ".";
    }
    return util::OkStatus();
  }

  // Performs a dry-run of InsertEntry. Returns errors if the entry cannot be
  // inserted. If the entry can be inserted, returns util::OkStatus().
  virtual util::Status DryRunInsertEntry(const ::p4::v1::TableEntry& entry) const {
    const auto result = entries_.find(entry);
    if (result != entries_.end()) {
      return MAKE_ERROR(ERR_ENTRY_EXISTS)
             << TableStr() << " contains duplicate of TableEntry: "
             << entry.ShortDebugString()
             << ". Matching TableEntry: " << result->ShortDebugString() << ".";
    }
    return util::OkStatus();
  }

  // Attempts to modify an existing entry in this table. Returns the original
  // entry on success.
  // Returns ERR_ENTRY_NOT_FOUND if a matching entry does not already exist.
  // Returns an error if the entry cannot be added.
  virtual util::StatusOr<p4::v1::TableEntry> ModifyEntry(
      const ::p4::v1::TableEntry& entry) {
    ASSIGN_OR_RETURN(p4::v1::TableEntry old_entry, DeleteEntry(entry));
    entries_.insert(entry);
    return old_entry;
  }

  // Attempts to delete an existing entry in this table. Returns the deleted
  // entry on success.
  // Returns ERR_ENTRY_NOT_FOUND if a matching entry does not already exist.
  virtual util::StatusOr<p4::v1::TableEntry> DeleteEntry(
      const ::p4::v1::TableEntry& key) {
    const auto lookup = entries_.find(key);
    if (lookup == entries_.end()) {
      return MAKE_ERROR(ERR_ENTRY_NOT_FOUND)
             << TableStr()
             << " does not contain TableEntry: " << key.ShortDebugString()
             << ".";
    }
    ::p4::v1::TableEntry entry = *lookup;
    entries_.erase(lookup);
    return entry;
  }

 protected:
  // Returns the standard Table ID string.
  string TableStr() const {
    return absl::StrCat("Table <", Id(), "> (", Name(), ")");
  }

  // ***************************************************************************
  // Parameters
  // ***************************************************************************
  uint32 id_;
  string name_;
  // Keeps track of all entries currently in the table.
  TableEntrySet entries_;
};

}  // namespace bcm
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_BCM_FLOW_TABLE_H_
