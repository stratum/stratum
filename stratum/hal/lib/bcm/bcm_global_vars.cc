#include "stratum/hal/lib/bcm/bcm_global_vars.h"
#include "absl/synchronization/mutex.h"

namespace stratum {

namespace hal {
namespace bcm {

ABSL_CONST_INIT absl::Mutex chassis_lock(absl::kConstInit);
bool shutdown = false;

}  // namespace bcm
}  // namespace hal

}  // namespace stratum
