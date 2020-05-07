// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_COMMON_CLIENT_SYNC_READER_WRITER_H_
#define STRATUM_HAL_LIB_COMMON_CLIENT_SYNC_READER_WRITER_H_

#include <memory>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "grpcpp/grpcpp.h"
#include "stratum/glue/logging.h"

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
      : stream_(std::move(ABSL_DIE_IF_NULL(stream))) {}

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
