// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

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
