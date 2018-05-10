/*
 * Copyright 2018 Google LLC
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


#ifndef STRATUM_HAL_LIB_COMMON_CLIENT_SYNC_READER_WRITER_H_
#define STRATUM_HAL_LIB_COMMON_CLIENT_SYNC_READER_WRITER_H_

#include <grpcpp/grpcpp.h>
#include <memory>

#include "stratum/glue/logging.h"
#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"

namespace stratum {
namespace hal {

// A partial wrapper class over ::grpc::ClientReaderWriter that provides
// synchronization among concurrent writes or reads. A call to WritesDone()
// is also synchronized with other Write() calls. The gRPC framework only
// guarantees concurrency between a read and a write.
// NOTE: Ensures concurrency among reads and writes separately.
//       Takes ownership of the gRPC stream reader-writer.
//
// W: Type of stream message from client-to-server
// R: Type of stream message from server-to-client
template <typename W, typename R>
class ClientSyncReaderWriter {
 public:
  explicit ClientSyncReaderWriter(
      std::unique_ptr<::grpc::ClientReaderWriter<W, R>> stream)
      : stream_(std::move(CHECK_NOTNULL(stream))) {}

  // ClientSyncReaderWriter is not copyable or assignable
  ClientSyncReaderWriter(const ClientSyncReaderWriter&) = delete;
  ClientSyncReaderWriter& operator=(const ClientSyncReaderWriter&) = delete;

  // Returns a reference to the stream reader-writer object.
  ::grpc::ClientReaderWriter<W, R>* get() const { return stream_.get(); }

  bool Read(R* msg) LOCKS_EXCLUDED(read_lock_) {
    absl::MutexLock l(&read_lock_);
    return stream_->Read(msg);
  }

  bool Write(const W& msg) LOCKS_EXCLUDED(write_lock_) {
    absl::MutexLock l(&write_lock_);
    return stream_->Write(msg);
  }

  bool WritesDone() LOCKS_EXCLUDED(write_lock_) {
    absl::MutexLock l(&write_lock_);
    return stream_->WritesDone();
  }

 private:
  // Mutex lock for concurrent writes.
  absl::Mutex write_lock_;

  // Mutex lock for concurrent reads.
  absl::Mutex read_lock_;

  // gRPC stream reader-writer object.
  std::unique_ptr<::grpc::ClientReaderWriter<W, R>> stream_;
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_COMMON_CLIENT_SYNC_READER_WRITER_H_
