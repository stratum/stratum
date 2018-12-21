// This main program runs a p4c binary for testing.  The test p4c binary
// currently runs without any backend extensions.

#include <memory>
#include <vector>

#include "base/init_google.h"
#include "platforms/networking/hercules/p4c_backend/common/backend_extension_interface.h"
#include "platforms/networking/hercules/p4c_backend/common/backend_pass_manager.h"
#include "platforms/networking/hercules/p4c_backend/common/p4c_front_mid_real.h"

using google::hercules::p4c_backend::BackendExtensionInterface;
using google::hercules::p4c_backend::BackendPassManager;
using google::hercules::p4c_backend::P4cFrontMidReal;

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);
  std::unique_ptr<P4cFrontMidReal> p4c_real_fe_me(new P4cFrontMidReal);
  std::vector<BackendExtensionInterface*> no_extensions = {};
  std::unique_ptr<BackendPassManager> backend(
      new BackendPassManager(p4c_real_fe_me.get(), no_extensions));
  backend->Compile();
}
