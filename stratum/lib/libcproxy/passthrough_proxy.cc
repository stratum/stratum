// Copyright 2019-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/lib/libcproxy/passthrough_proxy.h"

#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

namespace stratum {

int PassthroughLibcProxy::close(int fd) { return ::close(fd); }

int PassthroughLibcProxy::socket(int domain, int type, int protocol) {
  return ::socket(domain, type, protocol);
}

int PassthroughLibcProxy::setsockopt(int sockfd, int level, int optname,
                                     const void* optval, socklen_t optlen) {
  return ::setsockopt(sockfd, level, optname, optval, optlen);
}

int PassthroughLibcProxy::ioctl(int fd, uint64 request, void* arg) {
  return ::ioctl(fd, request, arg);
}

int PassthroughLibcProxy::bind(int sockfd, const struct sockaddr* my_addr,
                               socklen_t addrlen) {
  return ::bind(sockfd, my_addr, addrlen);
}

ssize_t PassthroughLibcProxy::sendmsg(int sockfd, const struct msghdr* msg,
                                      int flags) {
  return ::sendmsg(sockfd, msg, flags);
}

ssize_t PassthroughLibcProxy::recvmsg(int sockfd, struct msghdr* msg,
                                      int flags) {
  return ::recvmsg(sockfd, msg, flags);
}

int PassthroughLibcProxy::epoll_create1(int flags) {
  return ::epoll_create1(flags);
}

int PassthroughLibcProxy::epoll_ctl(int efd, int op, int fd,
                                    struct epoll_event* event) {
  return ::epoll_ctl(efd, op, fd, event);
}

int PassthroughLibcProxy::epoll_wait(int efd, struct epoll_event* events,
                                     int maxevents, int timeout) {
  return ::epoll_wait(efd, events, maxevents, timeout);
}

bool PassthroughLibcProxy::ShouldProxyEpollCreate() { return false; }

}  // namespace stratum
