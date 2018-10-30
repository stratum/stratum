#include "stratum/procmon/procmon.h"

#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include <memory>
#include <set>
#include <vector>

#include "gflags/gflags.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"
#include "stratum/glue/integral_types.h"
#include "absl/synchronization/mutex.h"

namespace google {
namespace hercules {
namespace procmon {

static const int32 kManagedProcessPollingIntervalMs = 100;

pid_t ProcessHandler::Fork() {
  return fork();
}

pid_t ProcessHandler::Waitpid(pid_t pid, int* status, int options) {
  return waitpid(pid, status, options);
}

int ProcessHandler::Kill(pid_t pid, int sig) {
  return kill(pid, sig);
}

Procmon::Procmon(std::shared_ptr<ProcessHandler> process_interface)
    : process_interface_(std::move(process_interface)) {}

Procmon::~Procmon() {
  // First stop the monitor thread.
  bool join_monitor_thread;
  {
    absl::MutexLock lock(&monitor_thread_lock_);
    join_monitor_thread = monitor_thread_running_;
    monitor_thread_running_ = false;
  }
  if (join_monitor_thread)
    pthread_join(monitor_thread_id_, nullptr);
  // Now terminate all monitored processes that are still running
  LOG(INFO) << "Stopping all remaining processes before deleting Procmon.";
  KillAll(true).IgnoreError();
}

::util::Status Procmon::Run(const ProcmonConfig& config) {
  RETURN_IF_ERROR(Initialize(config));
  while (true) {
    RETURN_IF_ERROR(HandleEvent());
  }
}

::util::Status Procmon::Initialize(const ProcmonConfig& config) {
  CHECK_RETURN_IF_FALSE(config.client_processes_size() > 0)
      << "Cannot start procmon with no managed processes.";
  config_ = config;
  // Start the monitor thread.
  absl::MutexLock lock(&monitor_thread_lock_);
  CHECK_RETURN_IF_FALSE(!pthread_create(
      &monitor_thread_id_, nullptr, &Procmon::StartMonitorThreadFunc, this));
  monitor_thread_running_ = true;
  // Schedule starting the first process.
  AddEvent(ProcmonEvent(START_PROCESS, -1, 0));
  return ::util::OkStatus();
}

::util::Status Procmon::HandleEvent() {
  ProcmonEvent event = GetEvent();
  ProcmonEventType type = event.event_type;
  pid_t pid = event.affected_pid;
  int startup_sequence = event.affected_startup_sequence;
  switch (type) {
    case START_PROCESS:
      CHECK_RETURN_IF_FALSE(
          startup_sequence < config_.client_processes_size() &&
          startup_sequence >= 0)
          << "Received START_PROCESS for invalid process #"
          << startup_sequence << ".";
      RETURN_IF_ERROR(StartProcess(config_.client_processes(startup_sequence)));
      // If available, schedule the next process to start.
      if (startup_sequence < config_.client_processes_size() - 1)
        AddEvent(ProcmonEvent(START_PROCESS, -1, startup_sequence + 1));
      break;
    case PROCESS_EXIT_OK:
    case PROCESS_EXIT_ERR:
      RETURN_IF_ERROR(HandleStoppedProcess(pid));
      break;
  }
  return ::util::OkStatus();
}

::util::Status Procmon::StartProcess(const ClientProcess& process) {
  pid_t new_pid = process_interface_->Fork();
  if (new_pid == 0) {
    SetupForkedProcess(process).IgnoreError();
    // Kill our forked process if setup failed.
    exit(1);
  } else {
    CHECK_RETURN_IF_FALSE(new_pid != -1)
        << "Failed to fork child process " << process.label() << ".";
    // We are the parent. Mark this process for monitoring.
    LOG(INFO) << "Starting process " << process.label()
              << " (pid " << new_pid << ").";
    ProcessInfo process_info;
    process_info.configuration = process;
    process_info.running = true;
    AddMonitoredPid(new_pid, process_info);
  }
  return ::util::OkStatus();
}

::util::Status Procmon::SetupForkedProcess(const ClientProcess& process) {
  // This function is loosely based off of helpers in
  // depot/depot3/platforms/networking/sandcastle/stack/procmon/monitored_process_helpers.cc
  // If we're running this code, we are in a forked child process. We need to
  // setup our environment and execute a new process as specified in the passed
  // ClientProcess.
  if (!process.directory().empty()) {
    RETURN_IF_ERROR(RecursivelyCreateDir(process.directory()));
    CHECK_RETURN_IF_FALSE(chdir(process.directory().c_str()) == 0)
        << "Failed to change to working directory " << process.directory()
        << ". Error code " << errno << ".";
  }
  // Close/redirect file descriptors.
  close(STDIN_FILENO);
  CHECK_RETURN_IF_FALSE(
      dup2(open("/dev/null", O_WRONLY), STDOUT_FILENO) == STDOUT_FILENO)
      << "Failed to redirect stdout to /dev/null.";
  CHECK_RETURN_IF_FALSE(
      dup2(open("/dev/null", O_WRONLY), STDERR_FILENO) == STDERR_FILENO)
      << "Failed to redirect stderr to /dev/null.";
  // Set the process priority.
  CHECK_RETURN_IF_FALSE(setpriority(PRIO_PROCESS, 0, process.priority()) == 0)
      << "Failed to set new process priority to " << process.priority();
  // Check that the executable file exists.
  CHECK_RETURN_IF_FALSE(PathExists(process.executable()))
      << "Cannot locate executable file " << process.executable() << " in "
      << "working directory " << process.directory() << ". Cannot run.";
  // Construct the argument list.
  std::vector<char*> argv, envp;
  char* exe_cstr = strdup(process.executable().c_str());
  argv.push_back(exe_cstr);
  for (int i = 0; i < process.argv_size(); i++) {
    char* arg_cstr = strdup(process.argv(i).c_str());
    argv.push_back(arg_cstr);
  }
  argv.push_back(nullptr);
  envp.push_back(nullptr);
  // And finally execute the process!
  execve(process.executable().c_str(), &argv[0], &envp[0]);
  return MAKE_ERROR() << "Oh no! Execve has returned! errno=" << errno << ".";
}

::util::Status Procmon::KillProcess(pid_t pid, bool force_kill) {
  ProcessInfo process_info;
  if (!RemoveMonitoredPid(pid, &process_info)) return ::util::OkStatus();
  // If this process has already exited, we are only cleaning up.
  if (!process_info.running) {
    LOG(INFO) << "Cleaning up finished process "
              << process_info.configuration.label() << " (pid " << pid << ").";
    return ::util::OkStatus();
  }
  // The process is still active.
  ClientProcess::OnKillBehavior action = process_info.configuration.on_kill();
  if (action == ClientProcess::CONTINUE && !force_kill) {
    LOG(INFO) << "Process " << process_info.configuration.label()
              << " (pid " << pid << ") ignores KILL_ALL.";
    // This process continues to run, and we continue to monitor it.
    AddMonitoredPid(pid, process_info);
    return ::util::OkStatus();
  }
  // Signal the process and wait for it to stop.
  int kill_ret;
  if (action == ClientProcess::ABORT) {
    LOG(INFO) << "Sending SIGABRT to process "
              << process_info.configuration.label() << " (pid " << pid << ").";
    kill_ret = process_interface_->Kill(pid, SIGABRT);
  } else {  // Everything defaults to a normal SIGTERM.
    LOG(INFO) << "Sending SIGTERM to process "
              << process_info.configuration.label() << " (pid " << pid << ").";
    kill_ret = process_interface_->Kill(pid, SIGTERM);
  }
  CHECK_RETURN_IF_FALSE(kill_ret == 0 || errno == ESRCH)
      // The process is still running, but we didn't send a signal.
      << "Failed to send a signal to pid " << pid
      << ". Unable to kill.";
  pid_t waitpid_ret = process_interface_->Waitpid(pid, nullptr, 0);
  CHECK_RETURN_IF_FALSE(waitpid_ret != -1)
      << "Error in waitpid for process " << process_info.configuration.label()
      << " with pid " << pid << ".";
  return ::util::OkStatus();
}

::util::Status Procmon::KillAll(bool force_kill_all_processes) {
  std::set<pid_t> pids;
  {
    absl::MutexLock lock(&monitored_process_lock_);
    for (const auto& pid_and_info : processes_)
      pids.insert(pid_and_info.first);
  }
  LOG(INFO) << "Attempting to kill " << pids.size() << " processes.";
  for (auto pid : pids)
    RETURN_IF_ERROR(KillProcess(pid, force_kill_all_processes));
  return ::util::OkStatus();
}

::util::Status Procmon::HandleStoppedProcess(pid_t pid) {
  ProcessInfo process_info;
  if (!RemoveMonitoredPid(pid, &process_info)) return ::util::OkStatus();
  switch (process_info.configuration.on_death()) {
    case ClientProcess::KILL_ALL:
      LOG(ERROR) << "Process " << process_info.configuration.label() << " (pid "
             << pid << ") has stopped with status " << process_info.exit_status
                 << ". Killing all processes.";
      RETURN_IF_ERROR(KillAll(false));
      break;
    case ClientProcess::LOG:
      LOG(ERROR) << "Process " << process_info.configuration.label() << " (pid "
             << pid << ") has stopped with status "
             << process_info.exit_status << ".";
      break;
    case ClientProcess::IGNORE:
      LOG(INFO) << "Process " << process_info.configuration.label() << " (pid "
             << pid << ") has stopped.";
      break;
    default:
      return MAKE_ERROR() << "Encountered invalid on_death behavior.";
  }
  return ::util::OkStatus();
}

void Procmon::AddEvent(const ProcmonEvent& event) {
  absl::MutexLock lock(&event_queue_lock_);
  event_queue_.push(event);
  event_queue_cond_var_.Signal();
}

ProcmonEvent Procmon::GetEvent() {
  absl::MutexLock lock(&event_queue_lock_);
  while (event_queue_.empty()) {
    event_queue_cond_var_.Wait(&event_queue_lock_);
  }
  ProcmonEvent event = event_queue_.front();
  event_queue_.pop();
  return event;
}

void Procmon::AddMonitoredPid(pid_t pid, const ProcessInfo& process_info) {
  absl::MutexLock lock(&monitored_process_lock_);
  processes_[pid] = process_info;
}

bool Procmon::RemoveMonitoredPid(pid_t pid, ProcessInfo* process_info) {
  absl::MutexLock lock(&monitored_process_lock_);
  auto pid_and_info = processes_.find(pid);
  if (pid_and_info == processes_.end())
    return false;
  *process_info = pid_and_info->second;
  processes_.erase(pid);
  return true;
}

void* Procmon::StartMonitorThreadFunc(void* self_ptr) {
  static_cast<Procmon*>(self_ptr)->MonitorThreadFunc();
  return nullptr;
}

void Procmon::MonitorThreadFunc() {
  while (true) {
    usleep(kManagedProcessPollingIntervalMs * 1000);
    {
      absl::MutexLock lock(&monitor_thread_lock_);
      if (!monitor_thread_running_) break;
    }
    absl::MutexLock lock(&monitored_process_lock_);

    for (auto& pid_and_process_info : processes_) {
      pid_t pid = pid_and_process_info.first;
      ProcessInfo* process_info = &pid_and_process_info.second;
      if (process_info->running) {
        // Check if the process has exited.
        int status;
        pid_t ret = process_interface_->Waitpid(pid, &status, WNOHANG);
        if (ret == -1) {
          LOG(ERROR) << "Error in waitpid for process "
                     << process_info->configuration.label() << " with pid "
                     << pid << ".";
        } else if (ret > 0) {
          AddEvent(ProcmonEvent(
              status == 0 ? PROCESS_EXIT_OK : PROCESS_EXIT_ERR, pid));
          // Mark the process as stopped so we don't check its status again.
          process_info->running = false;
          process_info->exit_status = status;
        }  // otherwise the process hasn't exited.
      }
    }
  }
}

}  // namespace procmon
}  // namespace hercules
}  // namespace google
