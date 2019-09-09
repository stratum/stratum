// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#include "stratum/glue/net_util/ports.h"

#include <errno.h>
#include <netinet/ip.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "stratum/glue/logging.h"

namespace stratum {

int PickUnusedIpv4PortOrDie() {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  CHECK_NE(-1, sock);
  struct sockaddr_in sa;
  bzero(&sa, sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_port = 0;
  sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  CHECK_NE(-1, bind(sock, reinterpret_cast<struct sockaddr*>(&sa), sizeof(sa)))
      << "bind() failed: " << strerror(errno);
  bzero(&sa, sizeof(sa));
  sa.sin_family = AF_INET;
  socklen_t sa_len = sizeof(sa);
  CHECK_NE(-1,
           getsockname(sock, reinterpret_cast<struct sockaddr*>(&sa), &sa_len))
      << "getsockaddr() failed " << strerror(errno);
  close(sock);
  return ntohs(sa.sin_port);
}

}  // namespace stratum
