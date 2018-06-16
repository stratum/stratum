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


#ifndef STRATUM_HAL_LIB_PHAL_ATTRIBUTE_DATABASE_H_
#define STRATUM_HAL_LIB_PHAL_ATTRIBUTE_DATABASE_H_

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/phal/attribute_database_interface.h"
#include "stratum/hal/lib/phal/attribute_group.h"
#include "stratum/hal/lib/phal/db.pb.h"
//#include "stratum/hal/lib/phal/google_switch_configurator.h"
#include "stratum/hal/lib/phal/phal.pb.h"
#include "stratum/hal/lib/phal/system_interface.h"
#include "stratum/hal/lib/phal/threadpool_interface.h"
#include "stratum/hal/lib/phal/udev_event_handler.h"
#include "stratum/lib/channel/channel.h"
#include "absl/synchronization/mutex.h"

namespace stratum {
namespace hal {
namespace phal {

// The internal implementation of AttributeDatabaseInterface.
//
// This interface will change as AttributeDatabaseInterface changes.
class AttributeDatabase : public AttributeDatabaseInterface {
 public:
  // Creates a new attribute database that uses the given group as its root node
  // and executes queries on the given threadpool. MakeGoogle or MakeONLP should
  // typically be called rather than this function.
  static ::util::StatusOr<std::unique_ptr<AttributeDatabase>> Make(
      std::unique_ptr<AttributeGroup> root,
      std::unique_ptr<ThreadpoolInterface> threadpool);

  // Creates a new attribute database that runs on a google-developed switch
  // platform.
  static ::util::StatusOr<std::unique_ptr<AttributeDatabase>> MakeGoogle(
      const std::string& legacy_phal_config_path,
      const SystemInterface* system_interface);

  // Creates a new attribute database that runs on an Open Network Linux
  // Platform (ONLP) switch. Currently unimplemented.
  static ::util::StatusOr<std::unique_ptr<AttributeDatabase>>
  MakeOnlp();

  ::util::Status Set(
      const std::vector<std::tuple<Path, Attribute>>& values) override;
  ::util::StatusOr<std::unique_ptr<Query>> MakeQuery(
      const std::vector<Path>& query_paths) override;

 private:
  AttributeDatabase(std::unique_ptr<AttributeGroup> root,
                    std::unique_ptr<ThreadpoolInterface> threadpool)
      : root_(std::move(root)), threadpool_(std::move(threadpool)) {}
  // The root node of the attribute tree maintained by this database.
  std::unique_ptr<AttributeGroup> root_;
  // The threadpool used to parallelize database queries.
  std::unique_ptr<ThreadpoolInterface> threadpool_;
  // The udev handler for detecting hardware state changes that affect the
  // database structure.
  std::unique_ptr<UdevEventHandler> udev_;
  // The configurator used for google switches.
  // TODO: Figure out if we really need this.
  //std::unique_ptr<GoogleSwitchConfigurator> google_switch_configurator_;
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_ATTRIBUTE_DATABASE_H_
