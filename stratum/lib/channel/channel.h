// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_LIB_CHANNEL_CHANNEL_H_
#define STRATUM_LIB_CHANNEL_CHANNEL_H_

#include <deque>
#include <list>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "stratum/lib/channel/channel_internal.h"
#include "stratum/lib/macros.h"

namespace stratum {

// Channels provide intra-process typed message-passing, designed to
// enable thread-safe data transfer within the Stratum switch-stack.
//
// A Channel<T> synchronizes communication between multiple ChannelWriter<T>s
// and ChannelReader<T>s. Messages are handled in FIFO order.
//
// Channel Objects:
//
//   Channel<T>: This is the main Channel object which encapsulates an internal
//   queue and the necessary synchronization primitives. A Channel<T> is created
//   via Channel<T>::Create() which returns a unique pointer to a new Channel<T>
//   object. Communication through a Channel<T> instance is done via
//   ChannelReader<T> and ChannelWriter<T> instances.
//
//   ChannelWriter<T>, ChannelReader<T>: The ChannelWriter and ChannelReader
//   objects are used to access the Write and Read functionalities respectively
//   of a Channel. ChannelWriter and ChannelReader instances share ownership of
//   a Channel instance via shared pointers.
//
//   T: Message type. T must be move-assignable.
//
// Example Setup and Cleanup:
//
//   int max_depth = 128;
//   // Create new Channel with internal buffer size 128 * sizeof(T) bytes.
//   std::shared_ptr<Channel<T>> channel = Channel<T>::Create(max_depth);
//
//   // Create the ChannelReader and ChannelWriter.
//   auto reader = ChannelReader<T>::Create(channel);
//   auto writer = ChannelWriter<T>::Create(channel);
//
//   // Pass the ChannelReader and ChannelWriter to other threads.
//   reader_args_t reader_args(std::move(reader));
//   pthread_create(..., &reader_args);
//   writer_args_t writer_args(std::move(writer));
//   pthread_create(..., &writer_args);
//
//   // Relinquish control of the Channel object once all required
//   // ChannelReaders and ChannelWriters have been created. The object will
//   // only be destroyed once all related ChannelReaders and ChannelWriters
//   // have been destroyed.
//   channel.reset();
//
//   // ALTERNATIVE: There may be conditions where it is known that the Channel
//   // will no longer be required, such as shutdown scenarios. In such cases,
//   // the original reference can be retained, and the following may be done:
//   channel->Close();
//   // This notifies all blocked ChannelReaders or ChannelWriters that the
//   // Channel is closed. Subsequent Read() or Write() calls immediately
//   // return.
//
// Example ChannelReader Function:
//
//   bool exit = false;  // can be set by another thread.
//   void* ThreadFunc(void* args) {
//     auto reader = std::move(reinterpret_cast<reader_args_t*>(args)->reader);
//     absl::Duration timeout = absl::Seconds(5);
//     T buf;
//     ::util::Status retval;
//     // Keep reading as long as the Channel is still open.
//     do {
//       retval = reader.Read(&buf, timeout);
//       if (retval.CanonicalCode() == ::util::error::CANCELLED) break;
//       // At most every 5 seconds, check whether or not to exit.
//       if (exit) break;
//       // If the queue was empty, block on Read again.
//       if (retval.CanonicalCode() == ::util::error::NOT_FOUND) continue;
//       // Operate on data.
//       ...
//     } while (1);
//     // If the ChannelReader holds the last reference to the Channel, the
//     // Channel will also be destroyed.
//     return nullptr;
//   }
//
// Example Select Function:
//   // Read all of the data through a ChannelReader of any message type.
//   template <typename T>
//   void ProcessChannel(ChannelReader<T>* reader) {
//     std::vector<T> Ts;
//     if (T_reader->ReadAll(&Ts).CanonicalCode() == ::util::error::Cancelled) {
//       return;
//     }
//     // Operate on the read data.
//   }
//
//   bool exit = false;  // can be set by another thread.
//   void* ThreadFunc(void* args) {
//     auto T_channel =
//         std::move(reinterpret_cast<select_args_t*>(args)->T_channel);
//     auto T_reader = ChannelReader<T>::Create(T_channel);
//     auto U_channel =
//         std::move(reinterpret_cast<select_args_t*>(args)->U_channel);
//     auto U_reader = ChannelReader<U>::Create(U_channel);
//     std::vector<channel_internal::ChannelBase*> channels =
//         {T_channel.get(), U_channel.get()};
//     absl::Duration timeout = absl::Seconds(5);
//     // Keep reading as long as at least one Channel is still open.
//     do {
//       auto status_or_ready = Select(channels, timeout);
//       // At most every 5 seconds, check whether or not to exit.
//       if (exit) break;
//       if (!status_or_ready.ok()) {
//         auto& status = status_or_ready.status();
//         // If all Channels are closed, exit.
//         if (status.CanonicalCode() == ::util::error::CANCELLED) break;
//         // If both Channels were empty, block on Select again.
//         if (status.CanonicalCode() == ::util::error::NOT_FOUND) continue;
//       }
//       SelectResult ready_flags = std::move(status_or_ready).ValueOrDie();
//       // Process T_channel if ready for read.
//       if (ready_flags(T_channel.get())) { ProcessChannel(T_reader.get()); }
//       // Process U_channel if ready for read.
//       if (ready_flags(U_channel.get())) { ProcessChannel(U_reader.get()); }
//     } while (1);
//     // Both Channels were closed and/or exit was set.
//     return nullptr;
//   }
//
// Notes on Usage:
//
// 1. The Channel remains open so long as Close() has not been called. As long
//    as a valid shared_ptr managing the original Channel instance remains in
//    scope, more ChannelReaders or ChannelWriters may be added to the Channel.
//
// 2. It is recommended to only read from a given Channel from a single thread.
//    Reading necessarily consumes data which will not be available to other
//    threads. Additionally, Reading from multiple threads can easily cause
//    out-of-sender-order processing of messages.

template <typename T>
class Channel;
template <typename T>
class ChannelReader;
template <typename T>
class ChannelWriter;

// "select"s on one or more Channels, each of which can be of any valid message
// type. For each Channel, sets the associated ready flag if there are any
// messages enqueued, unless it is closed. If no Channels are ready, blocks with
// the given timeout until at least one Channel is ready and sets the
// appropriate ready flags. Returns OK if any Channel is marked ready. If all
// given Channels are closed, returns code ERR_CANCELLED. If the timeout is
// reached without any ready flags set, returns code ERR_ENTRY_NOT_FOUND.
//
// NOTE: This function requires that all Channel pointers given remain valid
// throughout the execution of Select() (though the Channel state may change
// during execution).
//
// NOTE: This function name or signature may change in the case that it is found
// useful to enable selecting on Channels for the purpose of writing in addition
// to reading.
class SelectResult {
 public:
  explicit SelectResult(
      std::unique_ptr<std::unordered_map<channel_internal::ChannelBase*, bool>>
          ready_flags)
      : ready_flags_(std::move(ready_flags)) {}
  SelectResult(SelectResult&& other)
      : ready_flags_(std::move(other.ready_flags_)) {}
  SelectResult(const SelectResult&) = delete;

  bool operator()(channel_internal::ChannelBase* channel) const {
    return ready_flags_->count(channel) ? ready_flags_->at(channel) : false;
  }

  // FIXME Constructor for mock StatusOr errors
  SelectResult() {}

  SelectResult& operator=(SelectResult&& other) = default;
  SelectResult& operator=(const SelectResult& other) = delete;

 private:
  // Map from Channel "select"ed on to ready flag.
  std::unique_ptr<std::unordered_map<channel_internal::ChannelBase*, bool>>
      ready_flags_;
};

::util::StatusOr<SelectResult> Select(
    const std::vector<channel_internal::ChannelBase*>& channels,
    absl::Duration timeout);

// TODO(unknown): add support for optional en/dequeue timestamping.
template <typename T>
class Channel : public channel_internal::ChannelBase {
  // Check the type requirements documented at the top of this file.
  static_assert(std::is_move_assignable<T>::value,
                "Channel<T> requires T to be MoveAssignable.");

 public:
  ~Channel() override {}

  // Creates shared Channel object with given maximum queue depth.
  static std::unique_ptr<Channel<T>> Create(size_t max_depth) {
    return absl::WrapUnique(new Channel<T>(max_depth));
  }

  // Closes the Channel. Any blocked Read() or Write() operations immediately
  // return ERR_CANCELLED. Returns false if the Channel is already closed.
  virtual bool Close() LOCKS_EXCLUDED(queue_lock_);

  // Returns true if the Channel has been closed.
  virtual bool IsClosed() LOCKS_EXCLUDED(queue_lock_);

  // Disallow copy and assign.
  Channel(const Channel&) = delete;
  Channel& operator=(const Channel&) = delete;

 protected:
  // Protected constructor which intializes the Channel to the given maximum
  // queue depth.
  explicit Channel(size_t max_depth) : closed_(false), max_depth_(max_depth) {}

  // Writes a copy of t into the Channel. Returns ERR_SUCCESS on successful
  // enqueue. Blocks if the queue is full until the timeout, then returns
  // ERR_NO_RESOURCE. Returns ERR_CANCELLED if the Channel is closed.
  //
  // Note: If timeout = absl::InfiniteDuration(), Write() blocks indefinitely.
  virtual ::util::Status Write(const T& t, absl::Duration timeout)
      LOCKS_EXCLUDED(queue_lock_);
  virtual ::util::Status Write(T&& t, absl::Duration timeout)
      LOCKS_EXCLUDED(queue_lock_);

  // Returns ERR_NO_RESOURCE immediately if the queue is full.
  virtual ::util::Status TryWrite(const T& t) LOCKS_EXCLUDED(queue_lock_);
  virtual ::util::Status TryWrite(T&& t) LOCKS_EXCLUDED(queue_lock_);

  // Reads and pops the first element of the queue into t. Returns ERR_SUCCESS
  // on successful dequeue. Blocks if the queue is empty until the timeout, then
  // returns ERR_ENTRY_NOT_FOUND. Returns ERR_CANCELED if Channel is closed and
  // the queue is empty.
  //
  // Note: If timeout = absl::InfiniteDuration(), Read() blocks indefinitely.
  virtual ::util::Status Read(T* t, absl::Duration timeout)
      LOCKS_EXCLUDED(queue_lock_);

  // Returns ERR_ENTRY_NOT_FOUND immediately if the queue is empty.
  virtual ::util::Status TryRead(T* t) LOCKS_EXCLUDED(queue_lock_);

  // Reads all of the elements of the queue into ts. Returns ERR_CANCELED if the
  // Channel is closed, otherwise ERR_SUCCESS.
  virtual ::util::Status ReadAll(std::vector<T>* t_s)
      LOCKS_EXCLUDED(queue_lock_);

  // Checks whether there are any elements enqueued in the Channel. If true,
  // sets both done and ready to true and returns ERR_SUCCESS. If the Channel
  // is closed, returns ERR_CANCELLED.
  //
  // If the queue is empty, adds the select_data object as well as the ready
  // flag to an internal list. Once a new message is enqueued, all existing list
  // items are notified and removed from the list.
  void SelectRegister(
      const std::shared_ptr<channel_internal::SelectData>& select_data,
      bool* ready) LOCKS_EXCLUDED(queue_lock_) override;

 private:
  // Helper function used by both variants of Write(). Checks if Channel state
  // is closed and blocks if the internal queue is full. Returns OK or the error
  // statuses described above.
  ::util::Status CheckWriteStateAndBlock(absl::Duration timeout)
      EXCLUSIVE_LOCKS_REQUIRED(queue_lock_);

  // Helper function used by both variants of TryWrite(). Checks Channel state
  // for closure and queue occupancy. Returns OK or the error statuses described
  // above.
  ::util::Status CheckWriteState() EXCLUSIVE_LOCKS_REQUIRED(queue_lock_);

  // Helper function used by the Write()s and Close() on successful operation.
  // Pops each element on the select list, setting the corresponding done and
  // ready flags to the given value and signaling their condition variables.
  void ClearSelectList(bool ready) EXCLUSIVE_LOCKS_REQUIRED(queue_lock_);

  // Mutex to protect internal queue of the Channel and state.
  mutable absl::Mutex queue_lock_;
  std::deque<T> queue_ GUARDED_BY(queue_lock_);
  bool closed_ GUARDED_BY(queue_lock_);
  std::list<std::pair<std::shared_ptr<channel_internal::SelectData>, bool>>
      select_list_ GUARDED_BY(queue_lock_);

  // Maximum queue depth.
  const size_t max_depth_;

  // Condition variable for ChannelReaders waiting on empty queue_.
  mutable absl::CondVar cond_not_empty_;

  // Condition variable for ChannelWriters waiting on full queue_.
  mutable absl::CondVar cond_not_full_;

  friend class ChannelReader<T>;
  friend class ChannelWriter<T>;
};

template <typename T>
class ChannelReader {
 public:
  virtual ~ChannelReader() {}

  // Creates and returns ChannelReader for the Channel. Returns nullptr if
  // Channel is nullptr or closed.
  static std::unique_ptr<ChannelReader<T>> Create(
      std::shared_ptr<Channel<T>> channel) {
    if (!channel || channel->IsClosed()) return nullptr;
    return absl::WrapUnique(new ChannelReader<T>(std::move(channel)));
  }

  // The following functions are wrappers around the corresponding Channel
  // functionality.
  virtual ::util::Status Read(T* t, absl::Duration timeout) {
    return channel_->Read(t, timeout);
  }
  virtual ::util::Status TryRead(T* t) { return channel_->TryRead(t); }
  virtual ::util::Status ReadAll(std::vector<T>* t_s) {
    return channel_->ReadAll(t_s);
  }
  virtual bool IsClosed() { return channel_->IsClosed(); }

  // Disallow copy and assign.
  ChannelReader(const ChannelReader&) = delete;
  ChannelReader& operator=(const ChannelReader&) = delete;

 protected:
  // Constructor for mock ChannelReaders.
  ChannelReader() {}

 private:
  // Private constructor which initializes a ChannelReader from the given
  // Channel.
  explicit ChannelReader(std::shared_ptr<Channel<T>> channel)
      : channel_(ABSL_DIE_IF_NULL(std::move(channel))) {}

  std::shared_ptr<Channel<T>> channel_;
};

template <typename T>
class ChannelWriter {
 public:
  virtual ~ChannelWriter() {}

  // Creates and returns ChannelWriter for the Channel. Returns nullptr if
  // Channel is nullptr or closed.
  static std::unique_ptr<ChannelWriter<T>> Create(
      std::shared_ptr<Channel<T>> channel) {
    if (!channel || channel->IsClosed()) return nullptr;
    return absl::WrapUnique(new ChannelWriter<T>(std::move(channel)));
  }

  // The following functions are wrappers around the corresponding Channel
  // functionality.
  virtual ::util::Status Write(const T& t, absl::Duration timeout) {
    return channel_->Write(t, timeout);
  }
  virtual ::util::Status Write(T&& t, absl::Duration timeout) {
    return channel_->Write(std::move(t), timeout);
  }
  virtual ::util::Status TryWrite(const T& t) { return channel_->TryWrite(t); }
  virtual ::util::Status TryWrite(T&& t) {
    return channel_->TryWrite(std::move(t));
  }
  virtual bool IsClosed() { return channel_->IsClosed(); }

  // Disallow copy and assign.
  ChannelWriter(const ChannelWriter&) = delete;
  ChannelWriter& operator=(const ChannelWriter&) = delete;

 protected:
  // Constructor for mock ChannelWriters.
  ChannelWriter() {}

 private:
  // Private constructor which initializes a ChannelWriter to the given Channel.
  explicit ChannelWriter(std::shared_ptr<Channel<T>> channel)
      : channel_(ABSL_DIE_IF_NULL(std::move(channel))) {}

  std::shared_ptr<Channel<T>> channel_;
};

template <typename T>
bool Channel<T>::Close() {
  absl::MutexLock l(&queue_lock_);
  if (closed_) return false;
  closed_ = true;
  // Signal all blocked ChannelWriters.
  cond_not_full_.SignalAll();
  // Signal all blocked ChannelReaders.
  cond_not_empty_.SignalAll();
  // Signal any Select()-ing threads..
  ClearSelectList(false);
  return true;
}

template <typename T>
bool Channel<T>::IsClosed() {
  absl::MutexLock l(&queue_lock_);
  return closed_;
}

template <typename T>
::util::Status Channel<T>::Write(const T& t, absl::Duration timeout) {
  absl::MutexLock l(&queue_lock_);
  // Check internal state, blocking with timeout if queue is full.
  RETURN_IF_ERROR(CheckWriteStateAndBlock(timeout));
  // Enqueue message.
  queue_.push_back(t);
  // Signal next blocked ChannelReader.
  cond_not_empty_.Signal();
  // Signal any Select()-ing threads..
  ClearSelectList(true);
  return ::util::OkStatus();
}

template <typename T>
::util::Status Channel<T>::Write(T&& t, absl::Duration timeout) {
  absl::MutexLock l(&queue_lock_);
  // Check internal state, blocking with timeout if queue is full.
  RETURN_IF_ERROR(CheckWriteStateAndBlock(timeout));
  // Enqueue message.
  queue_.push_back(std::move(t));
  // Signal next blocked ChannelReader.
  cond_not_empty_.Signal();
  // Signal any Select()-ing threads..
  ClearSelectList(true);
  return ::util::OkStatus();
}

template <typename T>
::util::Status Channel<T>::CheckWriteStateAndBlock(absl::Duration timeout) {
  // Check Channel closure. If closed, there will be no signal.
  if (closed_) return MAKE_ERROR(ERR_CANCELLED) << "Channel is closed.";
  // Wait with timeout for non-full internal buffer. While is required as
  // signals may be delivered without an actual call to Signal() or
  // SignallAll().
  absl::Time deadline = absl::Now() + timeout;
  while (queue_.size() == max_depth_) {
    bool expired = cond_not_full_.WaitWithDeadline(&queue_lock_, deadline);
    // Could have been signalled because Channel is now closed.
    if (closed_) return MAKE_ERROR(ERR_CANCELLED) << "Channel is closed.";
    // Could have been signalled even if timeout has expired.
    if (expired && (queue_.size() == max_depth_)) {
      return MAKE_ERROR(ERR_NO_RESOURCE)
             << "Write did not succeed within timeout due to full Channel.";
    }
  }
  // Queue size should never exceed maximum queue depth.
  if (queue_.size() > max_depth_) {
    // TODO(unknown): Change to CRITICAL error once that is an option.
    return MAKE_ERROR(ERR_INTERNAL)
           << "Channel load " << queue_.size() << " exceeds max queue depth "
           << max_depth_ << ".";
  }
  return ::util::OkStatus();
}

template <typename T>
::util::Status Channel<T>::TryWrite(const T& t) {
  absl::MutexLock l(&queue_lock_);
  // Check internal state.
  RETURN_IF_ERROR(CheckWriteState());
  // Enqueue message.
  queue_.push_back(t);
  // Signal next blocked ChannelReader.
  cond_not_empty_.Signal();
  // Signal any Select()-ing threads..
  ClearSelectList(true);
  return ::util::OkStatus();
}

template <typename T>
::util::Status Channel<T>::TryWrite(T&& t) {
  absl::MutexLock l(&queue_lock_);
  // Check internal state.
  RETURN_IF_ERROR(CheckWriteState());
  // Enqueue message.
  queue_.push_back(std::move(t));
  // Signal next blocked ChannelReader.
  cond_not_empty_.Signal();
  // Signal any Select()-ing threads..
  ClearSelectList(true);
  return ::util::OkStatus();
}

template <typename T>
::util::Status Channel<T>::CheckWriteState() {
  // Check for Channel closure.
  if (closed_) return MAKE_ERROR(ERR_CANCELLED) << "Channel is closed.";
  // Check for full internal buffer.
  if (queue_.size() == max_depth_) {
    return MAKE_ERROR(ERR_NO_RESOURCE) << "Channel is full.";
  }
  // Queue size should never exceed maximum queue depth.
  if (queue_.size() > max_depth_) {
    // TODO(unknown): Change to CRITICAL error once that is an option.
    return MAKE_ERROR(ERR_INTERNAL)
           << "Channel load " << queue_.size() << " exceeds max queue depth "
           << max_depth_ << ".";
  }
  return ::util::OkStatus();
}

template <typename T>
void Channel<T>::ClearSelectList(bool ready) {
  while (!select_list_.empty()) {
    auto& pair = select_list_.front();
    {
      // Set select done flag and Channel ready flag and signal Select()-ing
      // thread.
      pair.second = ready;
      absl::MutexLock sel_lock(&pair.first->lock);
      pair.first->done = ready;
      pair.first->cond.Signal();
    }
    select_list_.pop_front();
  }
}

template <typename T>
::util::Status Channel<T>::Read(T* t, absl::Duration timeout) {
  absl::MutexLock l(&queue_lock_);
  // Check Channel closure. If closed, will not be signaled during wait.
  if (closed_)
    return MAKE_ERROR(ERR_CANCELLED).without_logging() << "Channel is closed.";
  // Wait with timeout for non-empty internal buffer.
  absl::Time deadline = absl::Now() + timeout;
  while (queue_.empty()) {
    bool expired = cond_not_empty_.WaitWithDeadline(&queue_lock_, deadline);
    // Could have been signalled because Channel is now closed.
    if (closed_)
      return MAKE_ERROR(ERR_CANCELLED).without_logging()
             << "Channel is closed.";
    // Could have been signalled even if timeout has expired.
    if (expired && queue_.empty()) {
      return MAKE_ERROR(ERR_ENTRY_NOT_FOUND)
             << "Read did not succeed within timeout due to empty Channel.";
    }
  }
  // Dequeue message.
  *t = std::move(queue_.front());
  queue_.pop_front();
  // Signal next blocked ChannelWriter.
  cond_not_full_.Signal();
  return ::util::OkStatus();
}

template <typename T>
::util::Status Channel<T>::TryRead(T* t) {
  absl::MutexLock l(&queue_lock_);
  // Check for Channel closure.
  if (closed_) return MAKE_ERROR(ERR_CANCELLED) << "Channel is closed.";
  // Check for empty internal buffer.
  if (queue_.empty()) {
    return MAKE_ERROR(ERR_ENTRY_NOT_FOUND) << "Channel is empty.";
  }
  // Dequeue message.
  *t = std::move(queue_.front());
  queue_.pop_front();
  // Signal next blocked ChannelWriter.
  cond_not_full_.Signal();
  return ::util::OkStatus();
}

template <typename T>
::util::Status Channel<T>::ReadAll(std::vector<T>* t_s) {
  absl::MutexLock l(&queue_lock_);
  // Check for Channel closure.
  if (closed_) return MAKE_ERROR(ERR_CANCELLED) << "Channel is closed.";
  // Resize vector for element_s to be moved.
  t_s->resize(queue_.size());
  std::move(queue_.begin(), queue_.end(), t_s->begin());
  // Clear internal buffer.
  queue_.erase(queue_.begin(), queue_.end());
  // Signal all blocked ChannelWriters.
  cond_not_full_.SignalAll();
  return ::util::OkStatus();
}

template <typename T>
void Channel<T>::SelectRegister(
    const std::shared_ptr<channel_internal::SelectData>& select_data,
    bool* ready) {
  absl::MutexLock l(&queue_lock_);
  // Check for Channel closure.
  if (closed_) return;
  // Check for empty internal buffer.
  absl::MutexLock sel_lock(&select_data->lock);
  if (queue_.empty()) {
    // Only enqueue a copy of select_data if the operation is not done.
    if (!select_data->done) {
      select_list_.push_back(std::make_pair(select_data, ready));
    }
  } else {
    *ready = true;
    select_data->done = true;
  }
}

}  // namespace stratum

#endif  // STRATUM_LIB_CHANNEL_CHANNEL_H_
