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


#include "stratum/hal/lib/phal/attribute_database.h"

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "stratum/glue/status/status_macros.h"
#include "stratum/hal/lib/phal/dummy_threadpool.h"
//#include "stratum/hal/lib/phal/google_switch_configurator.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"
#include "absl/memory/memory.h"

namespace stratum {
namespace hal {
namespace phal {
namespace {
// DatabaseQuery is a wrapper for AttributeGroupQuery that transforms query
// responses from google::protobuf::Message into PhalDB.
// TODO: The subscription polling thread should probably be
// implemented at this level rather than on a per-attribute group basis.
class DatabaseQuery : public Query {
 public:
  DatabaseQuery(AttributeGroup* root_group, ThreadpoolInterface* threadpool)
      : query_(root_group, threadpool) {}

  ::util::StatusOr<std::unique_ptr<PhalDB>> Get() override {
    auto query_result = absl::make_unique<PhalDB>();
    RETURN_IF_ERROR(query_.Get(query_result.get()));
    return std::move(query_result);
  }

  ::util::Status Subscribe(std::unique_ptr<ChannelWriter<PhalDB>> subscriber,
                           absl::Duration polling_interval) override {
    return query_.Subscribe(std::move(subscriber), polling_interval);
  }

  AttributeGroupQuery* InternalQuery() { return &query_; }

 private:
  AttributeGroupQuery query_;
};
}  // namespace

::util::StatusOr<std::unique_ptr<AttributeDatabase>> AttributeDatabase::Make(
    std::unique_ptr<AttributeGroup> root,
    std::unique_ptr<ThreadpoolInterface> threadpool) {
  CHECK_RETURN_IF_FALSE(root->AcquireReadable()->GetDescriptor() ==
                        PhalDB::descriptor())
      << "The root group of a AttributeDatabase must use "
      << "PhalDB as its schema.";
  return absl::WrapUnique(
      new AttributeDatabase(std::move(root), std::move(threadpool)));
}

::util::StatusOr<std::unique_ptr<AttributeDatabase>>
AttributeDatabase::MakeGoogle(const std::string& legacy_phal_config_path,
                              const SystemInterface* system_interface) {
  LegacyPhalInitConfig config;
  RETURN_IF_ERROR(ReadProtoFromTextFile(legacy_phal_config_path, &config));
  std::unique_ptr<AttributeGroup> root_group =
      AttributeGroup::From(PhalDB::descriptor());
  ASSIGN_OR_RETURN(std::unique_ptr<UdevEventHandler> udev,
                   UdevEventHandler::MakeUdevEventHandler(system_interface));
  //auto configurator =
  //    absl::make_unique<GoogleSwitchConfigurator>(system_interface, udev.get());
  //RETURN_IF_ERROR(configurator->ConfigureSwitch(config, root_group.get()));
  ASSIGN_OR_RETURN(
      std::unique_ptr<AttributeDatabase> database,
      Make(std::move(root_group), absl::make_unique<DummyThreadpool>()));
  database->udev_ = std::move(udev);
  //database->google_switch_configurator_ = std::move(configurator);
  return std::move(database);
}

::util::StatusOr<std::unique_ptr<AttributeDatabase>>
AttributeDatabase::MakeOnlp() {
  RETURN_ERROR() << "Not yet implemented.";
}

::util::Status AttributeDatabase::Set(
    const std::vector<std::tuple<Path, Attribute>>& values) {
  // TODO: Implement.
  RETURN_ERROR() << "AttributeDatabase::Set is not yet implemented.";
}

::util::StatusOr<std::unique_ptr<Query>> AttributeDatabase::MakeQuery(
    const std::vector<Path>& query_paths) {
  auto query = absl::make_unique<DatabaseQuery>(root_.get(), threadpool_.get());
  RETURN_IF_ERROR(root_->AcquireReadable()->RegisterQuery(
      query->InternalQuery(), query_paths));
  // An implicit cast doesn't work here.
  return absl::WrapUnique<Query>(query.release());
}

}  // namespace phal
}  // namespace hal
}  // namespace stratum
