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

#include "stratum/glue/status/status_test_util.h"
#include "gtest/gtest.h"
#include "absl/synchronization/mutex.h"

namespace stratum {

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
    for (int i = 0; i < msgs.size(); ++i) {
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

ABSL_CONST_INIT absl::Mutex arr_dst_lock/*absl::kConstInit*/;
absl::CondVar arr_dst_done/*base::LINKER_INITIALIZED*/;
constexpr size_t kArrTestSize = 5;
int test_arr_src[kArrTestSize];
int read_cnt = 0;
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
  for (int i = 0; i < kArrTestSize; ++i) test_arr_src[i] = i + 1;
  // Create ChannelReader/ChannelWriter threads.
  for (int i = 0; i < kArrTestSize; ++i) {
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
  for (int i = 0; i < kArrTestSize; ++i) {
    EXPECT_EQ(test_arr_src[i], test_arr_dst[i]);
  }
  // Join ChannelReader/ChannelWriter threads.
  for (int i = 0; i < kArrTestSize; ++i) {
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
TEST(ChannelTest, StressTest) {
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
    writers[i].writer = ChannelWriter<int>::Create(channel);
    writers[i].src = &src;
    writers[i].src_lock = &src_lock;
    pthread_create(&writer_tids[i], nullptr, StressTestChannelWriterFunc,
                   &writers[i]);
  }
  // Create ChannelReader threads.
  for (size_t i = 0; i < kChannelReaderCnt; i++) {
    readers[i].reader = ChannelReader<int>::Create(channel);
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

}  // namespace stratum
