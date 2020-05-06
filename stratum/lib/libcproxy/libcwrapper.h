// Copyright 2019-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_LIB_LIBCPROXY_LIBCWRAPPER_H_
#define STRATUM_LIB_LIBCPROXY_LIBCWRAPPER_H_

#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "stratum/glue/integral_types.h"
#include "stratum/glue/logging.h"
#include "stratum/lib/libcproxy/passthrough_proxy.h"

namespace stratum {

// LibcWrapper intercepts certain libc functions and passes them to a proxy.
// This version works by providing these functions as strong symbols in the
// text section of the binary. These (should) take precedence over the
// undefined versions, which would normal be resolved at runtime by loading
// the libc shared libray.
class LibcWrapper {
 public:
  static void SetLibcProxy(PassthroughLibcProxy* proxy);

  static PassthroughLibcProxy* GetLibcProxy();

 private:
  static PassthroughLibcProxy* proxy_;
};

}  // namespace stratum

#endif  // STRATUM_LIB_LIBCPROXY_LIBCWRAPPER_H_
