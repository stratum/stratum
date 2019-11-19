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

//

#ifndef STRATUM_LIB_LIBCPROXY_PASSTHROUGH_PROXY_H_
#define STRATUM_LIB_LIBCPROXY_PASSTHROUGH_PROXY_H_

#include "stratum/glue/integral_types.h"
#include "stratum/glue/logging.h"

#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

namespace stratum {

// PassthroughLibcProxy defines both the LibcProxy interface and provides a
// pass-through implementation of it. It is not intended to be used directly,
// but rather through LibcWrapper.
class PassthroughLibcProxy {
 public:
  PassthroughLibcProxy() {}

  virtual ~PassthroughLibcProxy() {}

  virtual int close(int fd);

  virtual int socket(int domain, int type, int protocol);

  virtual int setsockopt(int sockfd, int level, int optname, const void* optval,
                         socklen_t optlen);

  virtual int ioctl(int fd, uint64 request, void* arg);

  virtual int bind(int sockfd, const struct sockaddr* my_addr,
                   socklen_t addrlen);

  virtual ssize_t sendmsg(int sockfd, const struct msghdr* msg, int flags);

  virtual ssize_t recvmsg(int sockfd, struct msghdr* msg, int flags);

  virtual int epoll_create1(int flags);

  virtual int epoll_ctl(int efd, int op, int fd, struct epoll_event* event);

  virtual int epoll_wait(int efd, struct epoll_event* events, int maxevents,
                         int timeout);

  virtual bool ShouldProxyEpollCreate();
};

}  // namespace stratum

#endif  // STRATUM_LIB_LIBCPROXY_PASSTHROUGH_PROXY_H_
