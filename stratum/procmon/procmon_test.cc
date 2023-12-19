// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

#include "stratum/procmon/procmon.h"

#include <map>
#include <queue>

#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/lib/test_utils/matchers.h"
#include "stratum/lib/utils.h"
#include "stratum/procmon/procmon.pb.h"

namespace stratum {

namespace procmon {

const char kShortProcessConfig[] = R"(
client_processes {
  label: "SPEEDY_PROC"
  executable: "sleep"
  argv: "1"
  directory: "/bin"
  on_death: LOG
}
)";

const char kBrokenProcessConfig[] = R"(
client_processes {
  label: "BAD_PROC"
  executable: "sloop"
  argv: "111111"
  directory: "/some-missing-dir"
  on_death: LOG
}
)";

const char kTwoProcessConfig[] = R"(
client_processes {
  label: "SLOW_PROC"
  executable: "sleep"
  argv: "123456"
  directory: "/bin"
  on_kill: TERMINATE
  on_death: LOG
}
client_processes {
  label: "SPEEDY_PROC"
  executable: "sleep"
  argv: "1"
  directory: "/bin"
  on_kill: ABORT
  on_death: LOG
}
)";

const char kKillAllProcessesConfig[] = R"(
client_processes {
  label: "ASSASSIN_PROC"
  executable: "sleep"
  argv: "1"
  directory: "/bin"
  on_death: KILL_ALL
}
client_processes {
  label: "LONG_RUNNING_PROC"
  executable: "sleep"
  argv: "100"
  directory: "/bin"
  on_death: LOG
}
)";

const char kIgnoreKillAllProcessesConfig[] = R"(
client_processes {
  label: "FAILED_ASSASSIN_PROC"
  executable: "sleep"
  argv: "1"
  directory: "/bin"
  on_death: KILL_ALL
}
client_processes {
  label: "IMMORTAL_PROC"
  executable: "sleep"
  argv: "100000000"
  directory: "/bin"
  on_kill: CONTINUE
  on_death: LOG
}
)";

// We fake out the behavior of these calls rather than mocking so that we can
// handle regular requests from the procmon monitor thread.
class FakeProcessHandler : public ProcessHandler {
 public:
  // ProcessHandler functions:
  pid_t Fork() override {
    absl::MutexLock lock(&proc_lock_);
    EXPECT_GE(to_fork_.size(), 1);
    if (to_fork_.empty()) return -1;
    pid_t pid = to_fork_.front();
    to_fork_.pop();
    procs_running_[pid] = true;
    return pid;
  }
  pid_t Waitpid(pid_t pid, int* status, int options) override {
    absl::MutexLock lock(&proc_lock_);
    EXPECT_GT(pid, 0);
    if (pid <= 0) return -1;
    auto found = procs_running_.find(pid);
    EXPECT_TRUE(found != procs_running_.end());
    if (found == procs_running_.end()) return -1;
    bool running = found->second;
    // We should never see a blocking waitpid on a live process.
    if (!(options & WNOHANG)) {
      EXPECT_FALSE(running);
    }
    if (running) {
      return 0;
    } else {
      procs_running_.erase(found);
      if (status != nullptr) *status = 0;
      return pid;
    }
  }
  int Kill(pid_t pid, int sig) override {
    absl::MutexLock lock(&proc_lock_);
    EXPECT_GT(pid, 0);
    auto found = procs_running_.find(pid);
    EXPECT_TRUE(found != procs_running_.end());
    if (found == procs_running_.end()) return -1;
    found->second = false;
    return 0;
  }

  // Fake functions:
  void WillFork(pid_t new_pid) {
    absl::MutexLock lock(&proc_lock_);
    to_fork_.push(new_pid);
  }
  void VerifyCleanState() {
    absl::MutexLock lock(&proc_lock_);
    EXPECT_TRUE(to_fork_.empty());        // All procs started.
    EXPECT_TRUE(procs_running_.empty());  // No zombie procs.
  }
  void VerifyRunningProcsRemain() {
    absl::MutexLock lock(&proc_lock_);
    EXPECT_TRUE(to_fork_.empty());         // All procs started.
    EXPECT_FALSE(procs_running_.empty());  // Procs running.
  }

 private:
  absl::Mutex proc_lock_;
  std::queue<pid_t> to_fork_ GUARDED_BY(proc_lock_);
  std::map<pid_t, bool> procs_running_ GUARDED_BY(proc_lock_);
};

// We use ProcmonTest to manually check Procmon state and trigger
// important events that would normally be handled by the Run loop.
// This lets us test Procmon's behavior without dealing with much
// multi-thread and multi-process complexity.
class ProcmonTest : public ::testing::Test {
 protected:
  ProcmonConfig MakeConfig(const char text[]) {
    ProcmonConfig config;
    ParseProtoFromString(text, &config).IgnoreError();
    return config;
  }

  // Initialize and HandleEvent are normally called by Run(config).
  // We can control Procmon's execution better by calling them manually.
  ::util::Status Initialize(std::shared_ptr<ProcessHandler> interface,
                            const ProcmonConfig& config) {
    procmon_ = absl::make_unique<Procmon>(std::move(interface));
    return procmon_->Initialize(config);
  }
  ::util::Status HandleEvent() { return procmon_->HandleEvent(); }

  // These functions let us access procmon's internal state.
  std::queue<ProcmonEvent> GetEventQueue() {
    absl::MutexLock lock(&procmon_->event_queue_lock_);
    return procmon_->event_queue_;
  }
  std::map<pid_t, ProcessInfo> GetProcesses() {
    absl::MutexLock lock(&procmon_->monitored_process_lock_);
    return procmon_->processes_;
  }

  void WaitForEvent() {
    // Wait up to 10 seconds (200 * 50000 us). We wait in small increments to
    // avoid blocking tests for longer than
    // necessary.
    for (int i = 0; i < 200; i++) {
      usleep(50000);
      auto event_queue = GetEventQueue();
      if (event_queue.size() == 1) return;
    }
    FAIL();  // Timeout!
  }

  std::unique_ptr<Procmon> procmon_;
};

TEST_F(ProcmonTest, CanConfigureProcmon) {
  auto process_interface = std::make_shared<FakeProcessHandler>();
  EXPECT_OK(Initialize(process_interface, MakeConfig(kShortProcessConfig)));
  process_interface->VerifyCleanState();
}

TEST_F(ProcmonTest, CantConfigureNoProcesses) {
  EXPECT_FALSE(
      Initialize(std::make_shared<ProcessHandler>(), ProcmonConfig()).ok());
}

TEST_F(ProcmonTest, NewProcmonSchedulesFirstProcess) {
  auto process_interface = std::make_shared<FakeProcessHandler>();
  ASSERT_OK(Initialize(process_interface, MakeConfig(kShortProcessConfig)));
  auto event_queue = GetEventQueue();
  ASSERT_EQ(event_queue.size(), 1);
  EXPECT_EQ(event_queue.front().event_type, START_PROCESS);
  EXPECT_EQ(event_queue.front().affected_startup_sequence, 0);
  process_interface->VerifyCleanState();  // Process not started!
}

TEST_F(ProcmonTest, NewProcmonStartsFirstProcess) {
  auto process_interface = std::make_shared<FakeProcessHandler>();
  process_interface->WillFork(1234);
  ASSERT_OK(Initialize(process_interface, MakeConfig(kShortProcessConfig)));
  EXPECT_EQ(GetProcesses().size(), 0);
  EXPECT_OK(HandleEvent());  // Handle a START_PROCESS event.
  EXPECT_EQ(GetProcesses().size(), 1);
  process_interface->VerifyRunningProcsRemain();
}

TEST_F(ProcmonTest, ProcmonStopsProcessOnExit) {
  auto process_interface = std::make_shared<FakeProcessHandler>();
  process_interface->WillFork(1234);
  ASSERT_OK(Initialize(process_interface, MakeConfig(kShortProcessConfig)));
  EXPECT_OK(HandleEvent());  // Handle a START_PROCESS event.
  EXPECT_EQ(GetProcesses().size(), 1);
  process_interface->VerifyRunningProcsRemain();
  procmon_ = nullptr;
  process_interface->VerifyCleanState();  // Make sure our process was killed.
}

TEST_F(ProcmonTest, ProcmonDeletesStoppedProcess) {
  auto process_interface = std::make_shared<FakeProcessHandler>();
  process_interface->WillFork(1234);
  ASSERT_OK(Initialize(process_interface, MakeConfig(kShortProcessConfig)));
  EXPECT_OK(HandleEvent());             // Handle a START_PROCESS event.
  EXPECT_EQ(GetProcesses().size(), 1);  // Now there's one process.
  process_interface->Kill(1234, SIGTERM);
  WaitForEvent();
  auto event_queue = GetEventQueue();
  EXPECT_EQ(event_queue.front().event_type, PROCESS_EXIT_OK);
  EXPECT_EQ(event_queue.front().affected_pid, 1234);
  EXPECT_OK(HandleEvent());             // Handle a PROCESS_EXIT_OK event.
  EXPECT_EQ(GetProcesses().size(), 0);  // Now there are no processes.
  process_interface->VerifyCleanState();
}

TEST_F(ProcmonTest, ProcmonCanStartRealProcess) {
  ASSERT_OK(Initialize(std::make_shared<ProcessHandler>(),
                       MakeConfig(kShortProcessConfig)));
  EXPECT_EQ(GetProcesses().size(), 0);
  EXPECT_OK(HandleEvent());  // Handle a START_PROCESS event.
  EXPECT_EQ(GetProcesses().size(), 1);
}

TEST_F(ProcmonTest, ProcmonDeletesRealFailedProcess) {
  ASSERT_OK(Initialize(std::make_shared<ProcessHandler>(),
                       MakeConfig(kBrokenProcessConfig)));
  EXPECT_OK(HandleEvent());             // Handle a START_PROCESS event.
  EXPECT_EQ(GetProcesses().size(), 1);  // Now there's one process.
  WaitForEvent();
  auto event_queue = GetEventQueue();
  EXPECT_EQ(event_queue.front().event_type, PROCESS_EXIT_ERR);
  EXPECT_OK(HandleEvent());             // Handle a PROCESS_EXIT_ERR event.
  EXPECT_EQ(GetProcesses().size(), 0);  // Now there are no processes.
}

TEST_F(ProcmonTest, ProcmonHandlesSimultaneousProcesses) {
  auto process_interface = std::make_shared<FakeProcessHandler>();
  ASSERT_OK(Initialize(process_interface, MakeConfig(kTwoProcessConfig)));
  EXPECT_EQ(GetProcesses().size(), 0);
  process_interface->WillFork(1111);
  EXPECT_OK(HandleEvent());  // Handle a START_PROCESS event.
  EXPECT_EQ(GetProcesses().size(), 1);
  process_interface->WillFork(2222);
  EXPECT_OK(HandleEvent());  // Handle a START_PROCESS event.
  EXPECT_EQ(GetProcesses().size(), 2);
  process_interface->Kill(2222, SIGTERM);
  WaitForEvent();
  {
    auto event_queue = GetEventQueue();
    EXPECT_EQ(event_queue.front().event_type, PROCESS_EXIT_OK);
  }
  EXPECT_OK(HandleEvent());  // Handle a PROCESS_EXIT_OK event.
  EXPECT_EQ(GetProcesses().size(), 1);
  // Make sure that the right process was killed.
  EXPECT_EQ(GetProcesses().begin()->second.configuration.label(), "SLOW_PROC");
  process_interface->VerifyRunningProcsRemain();
  procmon_ = nullptr;
  process_interface->VerifyCleanState();  // Make sure our process was killed.
}

TEST_F(ProcmonTest, ProcessCanKillAll) {
  auto process_interface = std::make_shared<FakeProcessHandler>();
  ASSERT_OK(Initialize(process_interface, MakeConfig(kKillAllProcessesConfig)));
  process_interface->WillFork(1111);
  EXPECT_OK(HandleEvent());  // Handle a START_PROCESS event.
  process_interface->WillFork(2222);
  EXPECT_OK(HandleEvent());                // Handle a START_PROCESS event.
  EXPECT_EQ(GetProcesses().size(), 2);     // Now there are TWO processes.
  process_interface->Kill(1111, SIGTERM);  // Short process stops. KILL_ALL!
  WaitForEvent();
  auto event_queue = GetEventQueue();
  EXPECT_EQ(event_queue.front().event_type, PROCESS_EXIT_OK);
  EXPECT_OK(HandleEvent());  // Handle a PROCESS_EXIT_OK event.
  // This event should kill the longer running process!
  EXPECT_EQ(GetProcesses().size(), 0);  // Now there are no processes.
  process_interface->VerifyCleanState();
}

TEST_F(ProcmonTest, ProcessCanIgnoreKillAll) {
  auto process_interface = std::make_shared<FakeProcessHandler>();
  ASSERT_OK(
      Initialize(process_interface, MakeConfig(kIgnoreKillAllProcessesConfig)));
  process_interface->WillFork(1111);
  EXPECT_OK(HandleEvent());  // Handle a START_PROCESS event.
  process_interface->WillFork(2222);
  EXPECT_OK(HandleEvent());                // Handle a START_PROCESS event.
  EXPECT_EQ(GetProcesses().size(), 2);     // Now there are TWO processes.
  process_interface->Kill(1111, SIGTERM);  // Short process stops. KILL_ALL!
  WaitForEvent();
  auto event_queue = GetEventQueue();
  EXPECT_EQ(event_queue.front().event_type, PROCESS_EXIT_OK);
  EXPECT_OK(HandleEvent());             // Handle a PROCESS_EXIT_OK event.
  EXPECT_EQ(GetProcesses().size(), 1);  // Our other process isn't killed.
  process_interface->VerifyRunningProcsRemain();
}

}  // namespace procmon

}  // namespace stratum
