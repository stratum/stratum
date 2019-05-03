#ifndef STRATUM_LIB_STATEMACHINE_STATE_MACHINE_H_
#define STRATUM_LIB_STATEMACHINE_STATE_MACHINE_H_

#include <atomic>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/glue/status/statusor.h"

namespace stratum {
namespace state_machine {

// The StateMachine class is a general state machine framework which executes
// a client's callback functions upon entry and exit of each state.
// Specifically, entry actions are executed upon entry into a state, either when
// transitioning to the initial state or to a subsequent state. Conversely,
// exit actions are executed when transitioning out of a state. The public
// methods of the class are thread-safe. Please refer to example_state_machine.h
// for an example of how to use the StateMachine.
// State must be a primitive type (for the std::atomic<State> typecast to work),
// and enums require an integer binding.
template <typename State, typename Event>
class StateMachine {
 public:
  // The CallbackType is the function type for a client's entry and exit
  // actions. It requires the intended next_state and the incoming event which
  // triggered the transition. An internal error is returned when necessary, and
  // a recovery_event (a followup event in response to the issue) can be set
  // based on the recommended course of action. However, the recovery_event is
  // not automatically executed; it is merely a suggestion that may be used to
  // recover the state machine.
  using CallbackType = std::function< ::util::Status(
    Event event, State next_state, Event* recovery_event) >;
  // A TransitionTable stores the valid transitions, indexed by the outgoing
  // state and the incoming event; table_[current_state_][incoming_event] yields
  // the next state, if it exists.
  using TransitionTable =
    absl::flat_hash_map<State, absl::flat_hash_map<Event, State>>;

  // It is the client's responsibility to ensure that the initial state is safe
  // to enter before calling this constructor.
  explicit StateMachine(State initial_state, TransitionTable table)
    : current_state_(initial_state), table_(table) {}

  // Entry actions are executed in the order they are added.
  void AddEntryAction(State state, CallbackType callback)
    LOCKS_EXCLUDED(state_machine_mutex_) {
    absl::MutexLock lock(&state_machine_mutex_);
    entry_actions_[state].push_back(callback);
  }

  // Exit actions are executed in the order they are added.
  void AddExitAction(State state, CallbackType callback)
    LOCKS_EXCLUDED(state_machine_mutex_) {
    absl::MutexLock lock(&state_machine_mutex_);
    exit_actions_[state].push_back(callback);
  }

  // Evaluates whether the given event triggers a state transition. If so, it
  // performs any entry and exit actions. The reason parameter describes why the
  // event was added to the StateMachine.
  ::util::Status ProcessEvent(Event event, absl::string_view reason,
    Event* recovery_event) LOCKS_EXCLUDED(state_machine_mutex_) {
    absl::MutexLock lock(&state_machine_mutex_);
    return ProcessEventUnlocked(event, reason, recovery_event);
  }

  State CurrentState() const { return current_state_; }

 private:
  // Performs the actions of ProcessEvent.
  ::util::Status ProcessEventUnlocked(Event event, absl::string_view reason,
    Event* recovery_event) EXCLUSIVE_LOCKS_REQUIRED(state_machine_mutex_) {
    // Do not change states if the transition is invalid.
    ASSIGN_OR_RETURN(State next_state, NextState(current_state_, event));
    // FIXME: Wait for ASSIGN_OR_RETURN impl with error message
      // _.LogWarning() << "Event " << event << " [" << reason <<
      //"] was discarded in State " << current_state_);

    // Perform exit actions for the current state.
    for (const auto& exit_action : exit_actions_[current_state_]) {
      RETURN_IF_ERROR_WITH_APPEND(exit_action(event, next_state, recovery_event)) <<
        "Failed to perform exit action of state " << current_state_ <<
        " in transition to " << next_state << ".";
    }

    // Perform entry actions for the next state.
    for (const auto& entry_action : entry_actions_[next_state]) {
      RETURN_IF_ERROR_WITH_APPEND(entry_action(event, next_state, recovery_event)) <<
        "Failed to perform entry action of state " << next_state <<
        " in transition from " << current_state_ << ".";
    }

    // Update only if the entry and exit actions were successful.
    current_state_ = next_state;
    VLOG(1) << "Changing current state to " << current_state_;
    return ::util::OkStatus();
  }

  // Returns whether a given state-event pair results in a valid transition.
  ::util::StatusOr<State> NextState(State from_state, Event event) const {
    // No transitions exist for this state.
    const auto transitions = table_.find(from_state);
    if (transitions == table_.end()) {
      return ::util::Status(util::error::INTERNAL, "Invalid transition.");
    }
    // No transition from this state with the given event.
    const auto next_state_iter = transitions->second.find(event);
    if (next_state_iter == transitions->second.end()) {
      return ::util::Status(util::error::INTERNAL, "Invalid transition.");
    }
    // A valid transition was found; return the next state.
    return next_state_iter->second;
  }

  // The current state of the StateMachine, initialized in the constructor.
  std::atomic<State> current_state_;
  // A TransitionTable that stores valid transitions from any given state.
  TransitionTable table_;
  // A lock that is required when altering entry and exit actions and when
  // processing incoming events.
  absl::Mutex state_machine_mutex_;
  // A vector of actions that are executed upon entry to any given state.
  absl::flat_hash_map<State, std::vector<CallbackType>> entry_actions_
    GUARDED_BY(state_machine_mutex_);
  // A vector of actions that are executed upon exit from any given state.
  absl::flat_hash_map<State, std::vector<CallbackType>> exit_actions_
    GUARDED_BY(state_machine_mutex_);
};

}  // namespace state_machine
}  // namespace stratum

#endif  // STRATUM_LIB_STATEMACHINE_STATE_MACHINE_H_
