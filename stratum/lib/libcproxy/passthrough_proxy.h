// Copyright 2019-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_LIB_LIBCPROXY_PASSTHROUGH_PROXY_H_
#define STRATUM_LIB_LIBCPROXY_PASSTHROUGH_PROXY_H_

#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "stratum/glue/integral_types.h"
#include "stratum/glue/logging.h"

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
