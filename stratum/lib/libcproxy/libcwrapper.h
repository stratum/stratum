/*
 * Copyright 2019-present Open Networking Foundation
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
