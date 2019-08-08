// Copyright 2018 Google LLC
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


#include "stratum/lib/channel/channel.h"

#include <pthread.h>
#include <unistd.h>

#include <set>
#include <thread>  // NOLINT
#include <string>

#include "stratum/glue/status/status_test_util.h"
#include "stratum/lib/test_utils/matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/synchronization/mutex.h"

namespace stratum {

using channel_internal::ChannelBase;

// Test Channel creation, IsOpen() check, Close(), and destruction.
TEST(ChannelTest, TestCreateChannelClose) {
  auto channel = Channel<int>::Create(0);
  EXPECT_FALSE(channel->IsClosed());
  EXPECT_TRUE(channel->Close());
  EXPECT_TRUE(channel->IsClosed());
  // Should not be able to Close twice.
  EXPECT_FALSE(channel->Close());
}

// Test ChannelReader and ChannelWriter creation and Channel reference count.
TEST(ChannelTest, TestCreateChannelReaderChannelWriter) {
  std::shared_ptr<Channel<int>> channel = Channel<int>::Create(1);
  EXPECT_EQ(1, channel.use_count());
  auto reader = ChannelReader<int>::Create(channel);
  EXPECT_EQ(2, channel.use_count());
  auto writer = ChannelWriter<int>::Create(channel);
  EXPECT_EQ(3, channel.use_count());
  reader.reset();
  EXPECT_EQ(2, channel.use_count());
  writer.reset();
  EXPECT_EQ(1, channel.use_count());
}

// Test basic ChannelReader/ChannelWriter interaction with Channel.
TEST(ChannelTest, TestReadWriteClose) {
  std::shared_ptr<Channel<int>> channel = Channel<int>::Create(2);
  auto reader = ChannelReader<int>::Create(channel);
  auto writer = ChannelWriter<int>::Create(channel);
  absl::Duration timeout = absl::InfiniteDuration();

  // Write elements into Channel.
  EXPECT_FALSE(writer->IsClosed());
  EXPECT_OK(writer->TryWrite(1));
  EXPECT_OK(writer->Write(2, timeout));  // Should not block.
  // No space available in Channel.
  EXPECT_EQ(ERR_NO_RESOURCE, writer->TryWrite(3).error_code());

  // Read elements from Channel.
  EXPECT_FALSE(reader->IsClosed());
  int msg;
  EXPECT_OK(reader->TryRead(&msg));
  EXPECT_EQ(1, msg);
  EXPECT_OK(reader->Read(&msg, timeout));  // Should not block.
  EXPECT_EQ(2, msg);
  // No messages left in Channel.
  EXPECT_EQ(ERR_ENTRY_NOT_FOUND, reader->TryRead(&msg).error_code());

  // Test ReadAll().
  EXPECT_OK(writer->TryWrite(3));
  EXPECT_OK(writer->TryWrite(4));
  std::vector<int> msgs;
  EXPECT_OK(reader->ReadAll(&msgs));
  EXPECT_EQ(2, msgs.size());
  if (msgs.size() == 2) {
    std::vector<int> expected = {3, 4};
    for (size_t i = 0; i < msgs.size(); ++i) {
      EXPECT_EQ(msgs[i], expected[i]);
    }
  }
  // ReadAll() should always succeed if Channel open.
  EXPECT_OK(reader->ReadAll(&msgs));
  EXPECT_EQ(0, msgs.size());

  // Test Close() prevents any access to the Channel.
  EXPECT_OK(writer->TryWrite(1));
  EXPECT_TRUE(channel->Close());
  EXPECT_TRUE(writer->IsClosed());
  EXPECT_TRUE(reader->IsClosed());
  // Blocking and non-blocking calls should fail with ERR_CANCELLED.
  EXPECT_EQ(ERR_CANCELLED, writer->TryWrite(2).error_code());
  EXPECT_EQ(ERR_CANCELLED, writer->Write(3, timeout).error_code());
  EXPECT_EQ(ERR_CANCELLED, reader->TryRead(&msg).error_code());
  EXPECT_EQ(ERR_CANCELLED, reader->ReadAll(&msgs).error_code());
  EXPECT_EQ(ERR_CANCELLED, reader->Read(&msg, timeout).error_code());
}

namespace {

void* TestCloseReadFunc(void* arg) {
  const std::unique_ptr<ChannelReader<int>>& reader =
      *reinterpret_cast<std::unique_ptr<ChannelReader<int>>*>(arg);
  int buf;
  EXPECT_EQ(ERR_CANCELLED,
            reader->Read(&buf, absl::InfiniteDuration()).error_code());
  return nullptr;
}

void* TestCloseWriteFunc(void* arg) {
  const std::unique_ptr<ChannelWriter<int>>& writer =
      *reinterpret_cast<std::unique_ptr<ChannelWriter<int>>*>(arg);
  EXPECT_EQ(ERR_CANCELLED,
            writer->Write(0, absl::InfiniteDuration()).error_code());
  return nullptr;
}

}  // namespace

// Test Close() broadcast to blocked ChannelReaders or ChannelWriters on
// separate threads.
TEST(ChannelTest, TestCloseBroadcast) {
  // Channel size 0 will cause both readers and writers to block.
  std::shared_ptr<Channel<int>> channel = Channel<int>::Create(0);
  auto reader = ChannelReader<int>::Create(channel);
  auto writer = ChannelWriter<int>::Create(channel);

  // Create reader/writer threads.
  pthread_t r_tid, w_tid;
  pthread_create(&r_tid, nullptr, TestCloseReadFunc, &reader);
  pthread_create(&w_tid, nullptr, TestCloseWriteFunc, &writer);

  // Sleep for a while to allow other threads to be scheduled.
  usleep(10000);

  EXPECT_TRUE(channel->Close());
  // No matter which threads ran first, both Read() and Write() should return
  // indicating closed Channel and the threads should join.
  pthread_join(r_tid, nullptr);
  pthread_join(w_tid, nullptr);
}

namespace {

void* TestReadWaitFunc(void* arg) {
  const std::unique_ptr<ChannelReader<int>>& reader =
      *reinterpret_cast<std::unique_ptr<ChannelReader<int>>*>(arg);
  int buf;
  // Read will block indefinitely.
  EXPECT_OK(reader->Read(&buf, absl::InfiniteDuration()));
  return nullptr;
}

}  // namespace

// Test blocking Read operation using multiple threads.
TEST(ChannelTest, TestBlockingRead) {
  std::shared_ptr<Channel<int>> channel = Channel<int>::Create(1);
  auto reader = ChannelReader<int>::Create(channel);
  auto writer = ChannelWriter<int>::Create(channel);

  // Read with zero timeout will fail, as the queue is empty.
  int buf = 0;
  EXPECT_EQ(ERR_ENTRY_NOT_FOUND,
            reader->Read(&buf, absl::ZeroDuration()).error_code());

  // Create reader thread.
  pthread_t r_tid;
  pthread_create(&r_tid, nullptr, TestReadWaitFunc, &reader);

  // Wait for a while.
  usleep(10000);

  // Blocking write should return immediately.
  EXPECT_OK(writer->Write(0, absl::InfiniteDuration()));

  pthread_join(r_tid, nullptr);
}

namespace {

void* TestWriteWaitFunc(void* arg) {
  const std::unique_ptr<ChannelWriter<int>>& writer =
      *reinterpret_cast<std::unique_ptr<ChannelWriter<int>>*>(arg);
  // Write will block indefinitely.
  EXPECT_OK(writer->Write(0, absl::InfiniteDuration()));
  return nullptr;
}

}  // namespace

// Test blocking Write operation using multiple threads.
TEST(ChannelTest, TestBlockingWrite) {
  std::shared_ptr<Channel<int>> channel = Channel<int>::Create(1);
  auto reader = ChannelReader<int>::Create(channel);
  auto writer = ChannelWriter<int>::Create(channel);

  // Add a message to fill the queue.
  EXPECT_OK(writer->TryWrite(0));

  // Write with zero timeout will fail, as the queue is full.
  EXPECT_EQ(ERR_NO_RESOURCE,
            writer->Write(0, absl::ZeroDuration()).error_code());

  // Create writer thread.
  pthread_t w_tid;
  pthread_create(&w_tid, nullptr, TestWriteWaitFunc, &writer);

  // Wait for a while.
  usleep(10000);

  // Blocking read should return immediately.
  int buf;
  EXPECT_OK(reader->Read(&buf, absl::InfiniteDuration()));

  pthread_join(w_tid, nullptr);
}

namespace {

ABSL_CONST_INIT absl::Mutex arr_dst_lock(absl::kConstInit);  // NOLINTNEXTLINE
 // Opensource version of absl::CondVar has takes no arguments: https://github.com/abseil/abseil-cpp/blob/master/absl/synchronization/mutex.h#L777
absl::CondVar arr_dst_done /*(base::LINKER_INITIALIZED)*/;
constexpr size_t kArrTestSize = 5;
int test_arr_src[kArrTestSize];
size_t read_cnt = 0;
int test_arr_dst[kArrTestSize];
struct TestStruct {
  size_t idx;
  int val;
};
struct ChannelWriterArgs {
  std::unique_ptr<ChannelWriter<TestStruct>> writer;
  size_t idx;
};

void* TestArrChannelWriter(void* arg) {
  auto* args = reinterpret_cast<ChannelWriterArgs*>(arg);
  EXPECT_OK(args->writer->Write({args->idx, test_arr_src[args->idx]},
                                absl::InfiniteDuration()));
  return nullptr;
}

void* TestArrChannelReader(void* arg) {
  auto* reader =
      reinterpret_cast<std::unique_ptr<ChannelReader<TestStruct>>*>(arg);
  TestStruct buf;
  EXPECT_OK((*reader)->Read(&buf, absl::InfiniteDuration()));
  {
    absl::MutexLock l(&arr_dst_lock);
    test_arr_dst[buf.idx] = buf.val;
    if (++read_cnt == kArrTestSize) arr_dst_done.Signal();
  }
  return nullptr;
}

}  // namespace

// Test multiple blocking Reads and Writes on the same Channel from independent
// threads. The test copies the values from one array to another. ChannelWriters
// read from the source array and send a message with index and value.
// ChannelReaders write the received values into the associated indices of the
// destination array. At the end, the destination array should be a copy of the
// source array. The test should complete without a Close() call as there are an
// equal number of blocking Read()/Write() operations on the Channel.
TEST(ChannelTest, TestMultipleBlockingReadWrite) {
  pthread_t reader_tids[kArrTestSize];
  pthread_t writer_tids[kArrTestSize];
  std::unique_ptr<ChannelReader<TestStruct>> readers[kArrTestSize];
  ChannelWriterArgs writers[kArrTestSize];
  std::shared_ptr<Channel<TestStruct>> channel =
      Channel<TestStruct>::Create(kArrTestSize);
  // Initialize test array.
  for (size_t i = 0; i < kArrTestSize; ++i) test_arr_src[i] = i + 1;
  // Create ChannelReader/ChannelWriter threads.
  for (size_t i = 0; i < kArrTestSize; ++i) {
    readers[i] = ChannelReader<TestStruct>::Create(channel);
    pthread_create(&reader_tids[i], nullptr, TestArrChannelReader, &readers[i]);
    writers[i].writer = ChannelWriter<TestStruct>::Create(channel);
    writers[i].idx = i;
    pthread_create(&writer_tids[i], nullptr, TestArrChannelWriter, &writers[i]);
  }
  // Wait for all ChannelReaders to write to the destination array.
  {
    absl::MutexLock l(&arr_dst_lock);
    while (read_cnt != kArrTestSize) {
      arr_dst_done.Wait(&arr_dst_lock);
    }
    read_cnt = 0;
  }
  // Check final array.
  for (size_t i = 0; i < kArrTestSize; ++i) {
    EXPECT_EQ(test_arr_src[i], test_arr_dst[i]);
  }
  // Join ChannelReader/ChannelWriter threads.
  for (size_t i = 0; i < kArrTestSize; ++i) {
    pthread_join(reader_tids[i], nullptr);
    pthread_join(writer_tids[i], nullptr);
  }
}

// Thread functions for stress test.
namespace {

constexpr size_t kSetSize = 1000;
constexpr size_t kChannelWriterCnt = 10;
constexpr size_t kChannelReaderCnt = 15;
constexpr size_t kMaxDepth = 5;

struct StressTestChannelWriterArgs {
  std::set<int>* src;
  absl::Mutex* src_lock;
  std::unique_ptr<ChannelWriter<int>> writer;
};
struct StressTestChannelReaderArgs {
  std::set<int>* dst;
  absl::Mutex* dst_lock;
  absl::CondVar* dst_cond;
  std::unique_ptr<ChannelReader<int>> reader;
};

void* StressTestChannelWriterFunc(void* arg) {
  auto* args = reinterpret_cast<StressTestChannelWriterArgs*>(arg);
  bool block = (static_cast<size_t>(pthread_self()) & 1) != 0U;
  while (true) {
    int data = 0;
    // Get element from source set.
    {
      absl::MutexLock l(args->src_lock);
      if (args->src->empty()) break;
      data = *args->src->begin();
      args->src->erase(args->src->begin());
    }
    // Either Write() or loop on TryWrite() to send element through Channel.
    if (block) {
      if (args->writer->Write(data, absl::InfiniteDuration()).error_code() ==
          ERR_CANCELLED)
        break;
    } else {
      ::util::Status status;
      do {
        status = args->writer->TryWrite(data);
        // Prevent starvation of reader threads.
        // FIXME: this should already be guaranteed by absl::Mutex
        if (status.error_code() == ERR_NO_RESOURCE)
          std::this_thread::yield();
      } while (status.error_code() == ERR_NO_RESOURCE);
      if (status.error_code() == ERR_CANCELLED) break;
    }
  }
  return nullptr;
}

void* StressTestChannelReaderFunc(void* arg) {
  auto* args = reinterpret_cast<StressTestChannelReaderArgs*>(arg);
  bool block = (static_cast<size_t>(pthread_self()) & 1) != 0U;
  while (true) {
    int data = 0;
    // Either Read() or loop on TryRead() to receive element from Channel.
    if (block) {
      if (args->reader->Read(&data, absl::InfiniteDuration()).error_code() ==
          ERR_CANCELLED)
        break;
    } else {
      ::util::Status status;
      do {
        status = args->reader->TryRead(&data);
        std::this_thread::yield();
      } while (status.error_code() == ERR_ENTRY_NOT_FOUND);
      if (status.error_code() == ERR_CANCELLED) break;
    }
    // Write element into destination set and increment read count.
    {
      absl::MutexLock l(args->dst_lock);
      read_cnt++;
      // Signal main thread if all reads done.
      if (read_cnt == kSetSize) args->dst_cond->Signal();
      EXPECT_TRUE(args->dst->insert(data).second);
      if (args->dst->size() == kSetSize) break;
    }
  }
  return nullptr;
}

}  // namespace

// Similarly to the previous test, this test involves copying data from one set
// to another. However, the size of the set greatly exceeds the maximum queue
// depth, and there are more ChannelReaders and more ChannelWriters than that
// depth. Additionally, ChannelReaders and ChannelWriters may utilize the
// non-blocking calls.
TEST(ChannelTest, ReadWriteStressTest) {
  pthread_t reader_tids[kChannelReaderCnt];
  pthread_t writer_tids[kChannelWriterCnt];
  std::shared_ptr<Channel<int>> channel = Channel<int>::Create(kMaxDepth);
  StressTestChannelReaderArgs readers[kChannelReaderCnt];
  StressTestChannelWriterArgs writers[kChannelWriterCnt];
  std::set<int> src, src_copy, dst;
  absl::Mutex src_lock, dst_lock;
  absl::CondVar dst_cond;
  // Initialize source set and copy to compare against at end.
  for (size_t i = 0; i < kSetSize; i++) {
    src.insert(i);
    src_copy.insert(i);
  }
  // Create ChannelWriter threads.
  for (size_t i = 0; i < kChannelWriterCnt; i++) {
    ASSERT_NE(nullptr, writers[i].writer = ChannelWriter<int>::Create(channel));
    writers[i].src = &src;
    writers[i].src_lock = &src_lock;
    pthread_create(&writer_tids[i], nullptr, StressTestChannelWriterFunc,
                   &writers[i]);
  }
  // Create ChannelReader threads.
  for (size_t i = 0; i < kChannelReaderCnt; i++) {
    ASSERT_NE(nullptr, readers[i].reader = ChannelReader<int>::Create(channel));
    readers[i].dst = &dst;
    readers[i].dst_lock = &dst_lock;
    readers[i].dst_cond = &dst_cond;
    pthread_create(&reader_tids[i], nullptr, StressTestChannelReaderFunc,
                   &readers[i]);
  }
  // Block on completion case (as many completed Read()s as set size).
  {
    absl::MutexLock l(&dst_lock);
    while (read_cnt < kSetSize) dst_cond.Wait(&dst_lock);
    channel->Close();
    read_cnt = 0;
  }
  // Join ChannelWriter threads.
  for (auto writer_tid : writer_tids) {
    pthread_join(writer_tid, nullptr);
  }
  // Join ChannelReader threads.
  for (auto reader_tid : reader_tids) {
    pthread_join(reader_tid, nullptr);
  }
  // Check success.
  EXPECT_EQ(src_copy, dst);
}

TEST(ChannelTest, BasicSelectTest) {
  std::shared_ptr<Channel<int>> channel = Channel<int>::Create(2);
  auto writer = ChannelWriter<int>::Create(channel);
  auto reader = ChannelReader<int>::Create(channel);

  // When Channel is empty, Select() should fail.
  auto status_or_ready = Select({channel.get()}, absl::Milliseconds(100));
  EXPECT_EQ(ERR_ENTRY_NOT_FOUND, status_or_ready.status().error_code());

  EXPECT_OK(writer->TryWrite(1));
  // When Channel has data, Select() should succeed.
  status_or_ready = Select({channel.get()}, absl::InfiniteDuration());
  EXPECT_TRUE(status_or_ready.ok());
  if (status_or_ready.ok()) {
    EXPECT_TRUE(status_or_ready.ValueOrDie()(channel.get()));
  }

  // Select should not modify the Channel queue.
  status_or_ready = Select({channel.get()}, absl::InfiniteDuration());
  EXPECT_TRUE(status_or_ready.ok());
  EXPECT_OK(Select({channel.get()}, absl::InfiniteDuration()));
  if (status_or_ready.ok()) {
    EXPECT_TRUE(status_or_ready.ValueOrDie()(channel.get()));
  }
  int dummy = 0;
  EXPECT_OK(reader->TryRead(&dummy));
  EXPECT_EQ(1, dummy);

  EXPECT_TRUE(channel->Close());
  // When Channel is closed, Select() should fail.
  status_or_ready = Select({channel.get()}, absl::InfiniteDuration());
  EXPECT_EQ(ERR_CANCELLED, status_or_ready.status().error_code());
}

TEST(ChannelTest, BasicSelectTestMultiChannel) {
  std::shared_ptr<Channel<int>> int_channel = Channel<int>::Create(2);
  auto int_writer = ChannelWriter<int>::Create(int_channel);
  auto int_reader = ChannelReader<int>::Create(int_channel);
  std::shared_ptr<Channel<std::string>> str_channel =
      Channel<std::string>::Create(2);
  auto str_writer = ChannelWriter<std::string>::Create(str_channel);
  auto str_reader = ChannelReader<std::string>::Create(str_channel);
  std::vector<ChannelBase*> channels = {int_channel.get(), str_channel.get()};

  // When Channels are empty, Select() should fail.
  auto status_or_ready = Select(channels, absl::Milliseconds(100));
  EXPECT_EQ(ERR_ENTRY_NOT_FOUND, status_or_ready.status().error_code());

  // When one Channel is ready_flags, regardless of order, Select() should set
  // its flag.
  EXPECT_OK(int_writer->TryWrite(1));
  status_or_ready = Select(channels, absl::InfiniteDuration());
  EXPECT_TRUE(status_or_ready.ok());
  if (status_or_ready.ok()) {
    // Only int_channel should be ready.
    EXPECT_TRUE(status_or_ready.ValueOrDie()(int_channel.get()));
    EXPECT_FALSE(status_or_ready.ValueOrDie()(str_channel.get()));
  }
  EXPECT_OK(str_writer->TryWrite("1"));
  status_or_ready = Select(channels, absl::InfiniteDuration());
  EXPECT_TRUE(status_or_ready.ok());
  if (status_or_ready.ok()) {
    // Both int_channel and str_channel should be ready.
    EXPECT_TRUE(status_or_ready.ValueOrDie()(int_channel.get()));
    EXPECT_TRUE(status_or_ready.ValueOrDie()(str_channel.get()));
  }
  int dummy = 0;
  EXPECT_OK(int_reader->TryRead(&dummy));
  status_or_ready = Select(channels, absl::InfiniteDuration());
  EXPECT_TRUE(status_or_ready.ok());
  if (status_or_ready.ok()) {
    // Only str_channel should be ready.
    EXPECT_FALSE(status_or_ready.ValueOrDie()(int_channel.get()));
    EXPECT_TRUE(status_or_ready.ValueOrDie()(str_channel.get()));
  }

  // Select() should ignore a closed Channel if there are any open ones.
  EXPECT_TRUE(int_channel->Close());
  status_or_ready = Select(channels, absl::InfiniteDuration());
  EXPECT_TRUE(status_or_ready.ok());
  if (status_or_ready.ok()) {
    // Only str_channel should be ready.
    EXPECT_FALSE(status_or_ready.ValueOrDie()(int_channel.get()));
    EXPECT_TRUE(status_or_ready.ValueOrDie()(str_channel.get()));
  }

  // Select() should fail with ERR_CANCELLED if all Channels are closed.
  EXPECT_TRUE(str_channel->Close());
  status_or_ready = Select(channels, absl::InfiniteDuration());
  EXPECT_EQ(ERR_CANCELLED, status_or_ready.status().error_code());
}

namespace {

void SelectStressTestProcessChannel(ChannelReader<int>* reader,
                                    std::set<int>* set, bool* done) {
  std::vector<int> data;
  ASSERT_OK(reader->ReadAll(&data));
  for (auto datum : data) {
    EXPECT_TRUE(set->insert(datum).second);
  }
  read_cnt += data.size();
  if (read_cnt >= kSetSize) {
    *done = true;
    EXPECT_EQ(kSetSize, read_cnt);
    read_cnt = 0;
  }
}

}  // namespace

// Similar to ReadWriteStressTest but creates a separate Channel for each writer
// to use. The main thread Selects on and processes all of the messages.
TEST(ChannelTest, SelectStressTest) {
  const int kChannelCnt = 10;
  pthread_t writer_tids[kChannelCnt];
  std::shared_ptr<Channel<int>> channels[kChannelCnt];
  std::unique_ptr<ChannelReader<int>> readers[kChannelCnt];
  StressTestChannelWriterArgs writers[kChannelCnt];
  std::vector<ChannelBase*> channel_ptrs;
  std::set<int> src, src_copy, dst;
  absl::Mutex src_lock, dst_lock;
  bool dst_cond = false;
  // Initialize source set and copy to compare against at end.
  for (size_t i = 0; i < kSetSize; ++i) {
    src.insert(i);
    src_copy.insert(i);
  }
  // Create ChannelWriter threads.
  for (size_t i = 0; i < kChannelCnt; ++i) {
    channels[i] = Channel<int>::Create(kMaxDepth);
    ASSERT_NE(nullptr,
              writers[i].writer = ChannelWriter<int>::Create(channels[i]));
    writers[i].src = &src;
    writers[i].src_lock = &src_lock;
    pthread_create(&writer_tids[i], nullptr, StressTestChannelWriterFunc,
                   &writers[i]);
    ASSERT_NE(nullptr, readers[i] = ChannelReader<int>::Create(channels[i]));
    channel_ptrs.push_back(channels[i].get());
  }
  // Keep reading until the array has been copied.
  while (!dst_cond) {
    auto status_or_ready = Select(channel_ptrs, absl::InfiniteDuration());
    // The Channels should not be closed and Select() should not return without
    // at least one Channel being ready.
    ASSERT_TRUE(status_or_ready.ok());
    const auto& ready = status_or_ready.ValueOrDie();
    // Check if each Channel has data to be read.
    for (int i = 0; i < kChannelCnt; ++i) {
      if (ready(channel_ptrs[i])) {
        SelectStressTestProcessChannel(readers[i].get(), &dst, &dst_cond);
      }
    }
  }
  // Join ChannelWriter threads.
  for (int i = 0; i < kChannelCnt; ++i) {
    EXPECT_TRUE(channels[i]->Close());
    pthread_join(writer_tids[i], nullptr);
  }
  // Check success.
  EXPECT_EQ(src_copy, dst);
}

}  // namespace stratum
