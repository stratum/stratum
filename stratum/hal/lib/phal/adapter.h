// Copyright 2019 Open Networking Foundation
// Copyright 2019 Dell EMC
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_PHAL_ADAPTER_H_
#define STRATUM_HAL_LIB_PHAL_ADAPTER_H_

#include <memory>
#include <vector>

#include "stratum/glue/status/status.h"
#include "stratum/hal/lib/phal/attribute_database_interface.h"
#include "stratum/hal/lib/phal/db.pb.h"
#include "stratum/lib/channel/channel.h"

namespace stratum {
namespace hal {
namespace phal {

// This is the basic interface for attribute database adapters.
// TODO(max): extend if necessary.
class Adapter {
 public:
  // Construct a new Adapter to the database given.
  explicit Adapter(AttributeDatabaseInterface* attribute_db_interface)
      : database_(attribute_db_interface) {}

  virtual ~Adapter() = default;

  // Convenience function to Get values from the database.
  ::util::StatusOr<std::unique_ptr<PhalDB>> Get(const std::vector<Path>& paths);

  // Convenience function to Subscribe to the database.
  ::util::StatusOr<std::unique_ptr<Query>> Subscribe(
      const std::vector<Path>& paths,
      std::unique_ptr<ChannelWriter<PhalDB>> writer, absl::Duration poll_time);

  // Convenience function to Set values in the database.
  ::util::Status Set(const AttributeValueMap& values);

 private:
  // Handle to the database. Not owned by this class.
  AttributeDatabaseInterface* database_;
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_ADAPTER_H_
