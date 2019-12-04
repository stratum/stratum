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

#include "stratum/lib/libcproxy/libcwrapper.h"

#include <cstdarg>

extern "C" {
int close(int fd) { return stratum::LibcWrapper::GetLibcProxy()->close(fd); }

int socket(int domain, int type, int protocol) {
  return stratum::LibcWrapper::GetLibcProxy()->socket(domain, type, protocol);
}

int setsockopt(int sockfd, int level, int optname, const void* optval,
               socklen_t optlen) {
  return stratum::LibcWrapper::GetLibcProxy()->setsockopt(
      sockfd, level, optname, optval, optlen);
}

int ioctl(int fd, uint64_t request, ...) {
  std::va_list args;
  va_start(args, request);
  void* arg = va_arg(args, void*);
  va_end(args);
  return stratum::LibcWrapper::GetLibcProxy()->ioctl(fd, request, arg);
}

int bind(int sockfd, const struct sockaddr* my_addr, socklen_t addrlen) {
  return stratum::LibcWrapper::GetLibcProxy()->bind(sockfd, my_addr, addrlen);
}

ssize_t sendmsg(int sockfd, const struct msghdr* msg, int flags) {
  return stratum::LibcWrapper::GetLibcProxy()->sendmsg(sockfd, msg, flags);
}

ssize_t recvmsg(int sockfd, struct msghdr* msg, int flags) {
  return stratum::LibcWrapper::GetLibcProxy()->recvmsg(sockfd, msg, flags);
}

int epoll_create1(int flags) {
  return stratum::LibcWrapper::GetLibcProxy()->epoll_create1(flags);
}

int epoll_ctl(int efd, int op, int fd, struct epoll_event* event) {
  return stratum::LibcWrapper::GetLibcProxy()->epoll_ctl(efd, op, fd, event);
}

int epoll_wait(int efd, struct epoll_event* events, int maxevents,
               int timeout) {
  return stratum::LibcWrapper::GetLibcProxy()->epoll_wait(efd, events,
                                                          maxevents, timeout);
}
}

namespace stratum {

PassthroughLibcProxy* LibcWrapper::proxy_;

void LibcWrapper::SetLibcProxy(PassthroughLibcProxy* proxy) {
  LibcWrapper::proxy_ = proxy;
}

PassthroughLibcProxy* LibcWrapper::GetLibcProxy() {
  return LibcWrapper::proxy_;
}

}  // namespace stratum
