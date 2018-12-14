// The MidEndInterface defines a common set of methods to access a p4c midend.
// It allows custom midends and open source midends to be used interchangeably
// as long as a wrapper implementation exists.

#ifndef PLATFORMS_NETWORKING_HERCULES_P4C_BACKEND_COMMON_MIDEND_INTERFACE_H_
#define PLATFORMS_NETWORKING_HERCULES_P4C_BACKEND_COMMON_MIDEND_INTERFACE_H_

#include "p4lang_p4c/frontends/common/resolveReferences/referenceMap.h"
#include "p4lang_p4c/frontends/p4/typeMap.h"
#include "p4lang_p4c/ir/ir.h"

namespace google {
namespace hercules {
namespace p4c_backend {

// The MidEndInterface conforms to the Interface class requirements.
class MidEndInterface {
 public:
  virtual ~MidEndInterface() {}

  // Executes the midend pass on the input P4Program.  Midends typically
  // run once per instance to process a single program.  A nullptr return
  // indicates failure.
  virtual IR::ToplevelBlock* RunMidEndPass(const IR::P4Program& program) = 0;

  // Accessors to common midend objects - not valid until after RunMidEndPass.
  virtual IR::ToplevelBlock* top_level() = 0;
  virtual P4::ReferenceMap* reference_map() = 0;
  virtual P4::TypeMap* type_map() = 0;
};

}  // namespace p4c_backend
}  // namespace hercules
}  // namespace google

#endif  // PLATFORMS_NETWORKING_HERCULES_P4C_BACKEND_COMMON_MIDEND_INTERFACE_H_
