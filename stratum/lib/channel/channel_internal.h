#ifndef PLATFORMS_NETWORKING_HERCULES_LIB_CHANNEL_CHANNEL_INTERNAL_H_
#define PLATFORMS_NETWORKING_HERCULES_LIB_CHANNEL_CHANNEL_INTERNAL_H_

#include "platforms/networking/hercules/lib/macros.h"
#include "absl/synchronization/mutex.h"

namespace google {
namespace hercules {
namespace channel_internal {

// Data used by a Channel to manage an ongoing Select operation.
struct SelectData {
  absl::Mutex lock;
  absl::CondVar cond;
  bool done;
};

// Non-templated base Channel class. This exists to facilitate operations on
// Channels which are agnostic of the message type.
class ChannelBase {
 public:
  virtual ~ChannelBase() {}

  // Registers a Select operation on this Channel. Returns false if the Channel
  // is closed.
  virtual void SelectRegister(const std::shared_ptr<SelectData>& select_data,
                              bool* ready) = 0;

  // Disallow copy and assign.
  ChannelBase(const ChannelBase&) = delete;
  ChannelBase& operator=(const ChannelBase&) = delete;

 protected:
  // Default constructor.
  ChannelBase() {}
};

}  // namespace channel_internal
}  // namespace hercules
}  // namespace google

#endif  // PLATFORMS_NETWORKING_HERCULES_LIB_CHANNEL_CHANNEL_INTERNAL_H_
