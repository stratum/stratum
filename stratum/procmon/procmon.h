#ifndef PLATFORMS_NETWORKING_HERCULES_PROCMON_PROCMON_H_
#define PLATFORMS_NETWORKING_HERCULES_PROCMON_PROCMON_H_

#include <pthread.h>
#include <memory>
#include <queue>
#include <utility>

#include "stratum/procmon/procmon.pb.h"
#include "absl/synchronization/mutex.h"
#include "util/task/status.h"

namespace google {
namespace hercules {
namespace procmon {

enum ProcmonEventType {
  START_PROCESS = 1,
  PROCESS_EXIT_OK = 2,
  PROCESS_EXIT_ERR = 3
};

// Contains information about a single event to which procmon should respond.
// These are handled by the Procmon event queue.
struct ProcmonEvent {
  ProcmonEvent(ProcmonEventType type, pid_t pid, int startup_sequence)
      : event_type(type),
        affected_pid(pid),
        affected_startup_sequence(startup_sequence) {}
  ProcmonEvent(ProcmonEventType type, pid_t pid)
      : ProcmonEvent(type, pid, -1) {}
  ProcmonEventType event_type;
  pid_t affected_pid;             // Applies to PROCESS_EXIT_*
  int affected_startup_sequence;  // Applies to START_PROCESS
};

// Holds information about a single process which procmon is currently managing.
struct ProcessInfo {
  // The configuration used when setting up this process.
  ClientProcess configuration;
  // If false, the process has terminated.
  bool running;
  // The status returned when the process terminated. This value is only set if
  // running == false.
  int exit_status;
};

// This class wraps all process manipulation calls for testing.
class ProcessHandler {
 public:
  virtual ~ProcessHandler() {}
  virtual pid_t Fork();
  virtual pid_t Waitpid(pid_t pid, int* status, int options);
  virtual int Kill(pid_t pid, int sig);
};

// A Procmon starts a set of processes, monitors them as they run, and responds
// appropriately to any expected or unexpected termination. The specific
// processes, startup order, and termination behavior are determined by a
// ProcmonConfig. Within the switch stack, a single Procmon is initialized
// first, and is then responsible for starting every other process in the switch
// stack. This Procmon instance is typically started by procmon_main, which is
// run by /etc/init.d/procmond on a switch image.
//
// A design doc for this implementation can be found at
// https://docs.google.com/a/google.com/document/d/1ZVXcr1_F9G9vYynYnc1G0QluIa_GdSIkoW1hnBsWwwA/edit?usp=sharing
class Procmon {
 public:
  // Constructs a new Procmon that will use the given ProcessHandler for all
  // process creation/destruction system calls. This Procmon will not begin
  // managing processes until Run(...) is called with a configuration.
  explicit Procmon(std::shared_ptr<ProcessHandler> process_interface);
  virtual ~Procmon();

  // Initializes the processes specified in the given config, and begins
  // monitoring them. Run does not return unless something has gone wrong.
  ::util::Status Run(const ProcmonConfig& config);

 private:
  friend class ProcmonTest;
  // Starts the process monitoring thread and sets up the event queue.
  ::util::Status Initialize(const ProcmonConfig& config);

  // Reads an event off of the event queue and performs any necessary updates
  // to Procmon state. This function is responsible for all major process
  // management, e.g. starting a new process or aborting a running process.
  // Calls to HandleEvent will block until exactly one event has been handled.
  ::util::Status HandleEvent();

  // Fork off a new process with the given configuration.
  ::util::Status StartProcess(const ClientProcess& process);

  // StartProcess calls SetupForkedProcess in the forked process. If successful,
  // this function does not return.
  ::util::Status SetupForkedProcess(const ClientProcess& process);

  // These functions will observe the on_kill behavior of each monitored
  // process unless force_kill(_all_processes) is true. If a process has already
  // stopped on its own, it will be cleaned up rather than terminated.
  ::util::Status KillProcess(pid_t pid, bool force_kill)
      LOCKS_EXCLUDED(monitored_process_lock_);
  ::util::Status KillAll(bool force_kill_all_processes)
      LOCKS_EXCLUDED(monitored_process_lock_);

  // Perform all necessary actions based on the given processes on_death
  // behavior.
  ::util::Status HandleStoppedProcess(pid_t pid);

  // Adds an event to the event queue.
  void AddEvent(const ProcmonEvent& event) LOCKS_EXCLUDED(event_queue_lock_);

  // Blocks until an event is available, then pops it from the event queue.
  ProcmonEvent GetEvent() LOCKS_EXCLUDED(event_queue_lock_);

  // Add information about a new pid. The monitor thread will start monitoring
  // this pid as soon as AddMonitoredPid returns.

  void AddMonitoredPid(pid_t pid, const ProcessInfo& process_info)
      LOCKS_EXCLUDED(monitored_process_lock_);
  // Returns false if the given pid does not exist. Writes the most recent info
  // about the given process to process_info. The monitor thread will no longer
  // collect this process after RemoveMonitoredPid returns, so the caller must
  // call waitpid if process_info->running is still true. It is also okay to
  // pass the returned process_info back to AddMonitoredPid.

  bool RemoveMonitoredPid(pid_t pid, ProcessInfo* process_info)
      LOCKS_EXCLUDED(monitored_process_lock_);

  // A thin wrapper for MonitorThreadFunc(), to be used in calls to
  // pthread_create.
  static void* StartMonitorThreadFunc(void* self_ptr);

  // Continuously loops over the set of monitored processes, checking for any
  // that have exited. If any monitored process has exited, collects their
  // exit status and pushes the appropriate event to the procmon event queue.
  // MonitorThreadFunc will run until monitor_thread_running_ is set to false.
  void MonitorThreadFunc();

  // The interface used for all process creation/destruction syscalls.
  std::shared_ptr<ProcessHandler> process_interface_;

  // Mutex lock for protecting event queue.
  absl::Mutex event_queue_lock_;

  // This CondVar is used to implement blocking reads from the event queue.
  absl::CondVar event_queue_cond_var_;

  // This queue stores all events that should be handled by Procmon.
  std::queue<ProcmonEvent> event_queue_ GUARDED_BY(event_queue_lock_);

  // Mutex lock for protecting the map of processes.
  absl::Mutex monitored_process_lock_;

  // Stores information about every process managed by procmon that is
  // currently running or has recently exited.
  std::map<pid_t, ProcessInfo> processes_ GUARDED_BY(monitored_process_lock_);

  // Mutex lock for protecting the monitor_thread_running_ which specifies
  // when monitor thread must exit.
  absl::Mutex monitor_thread_lock_;

  // monitor_thread_running_ is constantly read by the process monitor thread.
  // When set to false, the monitor thread will exit its loop and return.
  bool monitor_thread_running_ GUARDED_BY(monitor_thread_lock_) = false;

  // Monitor thread id.
  pthread_t monitor_thread_id_;

  // The configuration passed when calling Run.
  ProcmonConfig config_;
};

}  // namespace procmon
}  // namespace hercules
}  // namespace google

#endif  // PLATFORMS_NETWORKING_HERCULES_PROCMON_PROCMON_H_
