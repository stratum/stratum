// Copyright 2019 Dell EMC
// Copyright 2019-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/phal/adapter.h"

#include <utility>
#include <vector>

namespace stratum {
namespace hal {
namespace phal {

::util::StatusOr<std::unique_ptr<PhalDB>> Adapter::Get(
    const std::vector<Path>& paths) {
  ASSIGN_OR_RETURN(auto db_query, database_->MakeQuery(paths));
  ASSIGN_OR_RETURN(auto phaldb_resp, db_query->Get());
  return std::move(phaldb_resp);
}

::util::StatusOr<std::unique_ptr<Query>> Adapter::Subscribe(
    const std::vector<Path>& paths,
    std::unique_ptr<ChannelWriter<PhalDB>> writer, absl::Duration poll_time) {
  ASSIGN_OR_RETURN(auto db_query, database_->MakeQuery(paths));
  RETURN_IF_ERROR(db_query->Subscribe(std::move(writer), poll_time));
  return db_query;
}

::util::Status Adapter::Set(const AttributeValueMap& attrs) {
  return database_->Set(attrs);
}

}  // namespace phal
}  // namespace hal
}  // namespace stratum
