#ifndef PLATFORMS_NETWORKING_HERCULES_HAL_LIB_BCM_BCM_GLOBAL_VARS_H_
#define PLATFORMS_NETWORKING_HERCULES_HAL_LIB_BCM_BCM_GLOBAL_VARS_H_

#include "absl/synchronization/mutex.h"

namespace google {
namespace hercules {
namespace hal {
namespace bcm {

// Lock which governs chassis state (ports, etc.) across the entire switch.
extern absl::Mutex chassis_lock;

// Flag indicating if the switch has been shut down. Initialized to false.
extern bool shutdown;

}  // namespace bcm
}  // namespace hal
}  // namespace hercules
}  // namespace google

#endif  // PLATFORMS_NETWORKING_HERCULES_HAL_LIB_BCM_BCM_GLOBAL_VARS_H_
