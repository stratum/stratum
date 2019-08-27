// Copyright 2019 Dell EMC
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


#include "stratum/hal/lib/phal/adapter.h"

#include "stratum/glue/status/status.h"

namespace stratum {
namespace hal {
namespace phal {

::util::StatusOr<std::unique_ptr<PhalDB>> Adapter::Get(const Path& path) {

    ASSIGN_OR_RETURN(auto database, phal_interface_->GetPhalDB());
    ASSIGN_OR_RETURN(auto db_query, database->MakeQuery({path}));
    ASSIGN_OR_RETURN(auto phaldb_resp, db_query->Get());

    return std::move(phaldb_resp);
}

::util::Status Adapter::Subscribe(const Path& path,
    std::unique_ptr<ChannelWriter<PhalDB>> writer,
    absl::Duration poll_time) {

    ASSIGN_OR_RETURN(auto database, phal_interface_->GetPhalDB());
    ASSIGN_OR_RETURN(auto db_query, database->MakeQuery({path}));
    return (db_query->Subscribe(std::move(writer), poll_time));
}

::util::Status Adapter::Set(const AttributeValueMap& attrs) {
    ASSIGN_OR_RETURN(auto database, phal_interface_->GetPhalDB());
    return (database->Set(attrs));
}

}  // namespace phal
}  // namespace hal
}  // namespace stratum
