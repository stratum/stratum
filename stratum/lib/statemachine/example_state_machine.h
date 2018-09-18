#ifndef PLATFORMS_NETWORKING_HERCULES_LIB_STATEMACHINE_EXAMPLE_STATE_MACHINE_H_
#define PLATFORMS_NETWORKING_HERCULES_LIB_STATEMACHINE_EXAMPLE_STATE_MACHINE_H_

#include "platforms/networking/hercules/lib/statemachine/state_machine.h"
#include "testing/base/public/gmock.h"
#include "absl/container/flat_hash_map.h"
#include "util/task/status.h"

namespace google {
namespace hercules {
namespace state_machine {

// This is a client of the StateMachine class which defines the states, events,
// valid transitions and corresponding entry/exit actions.
// Usage:
//   ExampleStateMachine example_sm;
//   example_sm.StartStateMachine(STATE1);
//   Event recovery_event;
//   if (!example_sm.ProcessEvent(FROM1, "", &recovery_event).ok()) {
//     // Process the recovery event.
//   }
class ExampleStateMachine {
 public:
  enum State {
    STATE0 = 0,
    STATE1 = 1,
    STATE2 = 2,
    FAILED = 3,  // FAILED is not treated any differently by the StateMachine.
  };

  enum Event {
    FROM0 = 0,
    FROM1 = 1,
    FAULT = 2,
  };

  ExampleStateMachine() { FillTransitionTable(); }

  State CurrentState() const { return sm_->CurrentState(); }
  // Creates the StateMachine with the default initial state and sets it up
  // with entry and exit functions. Only one of the StartStateMachine(...)
  // functions should be used, and it should be called exactly once per
  // ExampleStateMachine instance.
  void StartStateMachine();
  // Creates the StateMachine with the initial state specified and sets it up
  // with entry and exit functions. Only one of the StartStateMachine(...)
  // functions should be used, and it should be called exactly once per
  // ExampleStateMachine instance.
  void StartStateMachine(State initial_state);
  // Adds an event for the StateMachine to process, with an optional reason
  // describing why the event is added. In the case where an entry/exit action
  // fails during a transition, the recovery_event is set based on the
  // recommended course of action.
  ::util::Status ProcessEvent(Event event, absl::string_view reason,
                              Event* recovery_event);

  // Mock entry and exit actions to be executed by the StateMachine.
  MOCK_METHOD3(ExitState0, ::util::Status(Event event, State next_state,
                                          Event* recovery_event));
  MOCK_METHOD3(EnterState1, ::util::Status(Event event, State next_state,
                                           Event* recovery_event));
  MOCK_METHOD3(EnterState2, ::util::Status(Event event, State next_state,
                                           Event* recovery_event));
  MOCK_METHOD3(EnterFailed, ::util::Status(Event event, State next_state,
                                           Event* recovery_event));

 private:
  // Defines all valid transitions for the example state machine.
  // The transition table represents the following state diagram:
  // STATE0 STATE1 STATE2 FAILED
  //   ||____↑||     ↑|    ↑↑↑↺
  //   | FROM0||_____||____|||FAULT
  //   |      | FROM1  FAULT||
  //   |      |_____________||
  //   |           FAULT     |
  //   |_____________________|
  //      FAULT
  void FillTransitionTable();
  // Adds the entry and exit functions for each state.
  void AddCallbackFunctions();

  std::unique_ptr<StateMachine<State, Event>> sm_;
  absl::flat_hash_map<State, absl::flat_hash_map<Event, State>> table_;
};

}  // namespace state_machine
}  // namespace hercules
}  // namespace google

#endif  // PLATFORMS_NETWORKING_HERCULES_LIB_STATEMACHINE_EXAMPLE_STATE_MACHINE_H_
