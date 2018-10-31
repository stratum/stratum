#include "stratum/lib/statemachine/example_state_machine.h"

namespace stratum {

namespace state_machine {

using StateMachineType =
    StateMachine<ExampleStateMachine::State, ExampleStateMachine::Event>;

void ExampleStateMachine::StartStateMachine() {
  CHECK(sm_ == nullptr);
  sm_ = absl::make_unique<StateMachineType>(STATE0, table_);
  AddCallbackFunctions();
}

void ExampleStateMachine::StartStateMachine(State initial_state) {
  CHECK(sm_ == nullptr);
  sm_ = absl::make_unique<StateMachineType>(initial_state, table_);
  AddCallbackFunctions();
}

// Warning: all callbacks must be used within the lifetime of the state machine.
void ExampleStateMachine::AddCallbackFunctions() {
  sm_->AddExitAction(STATE0,
    [this] (Event event, State next_state, Event* recovery_event)
    { return ExitState0(event, next_state, recovery_event); });
  sm_->AddEntryAction(STATE1,
    [this] (Event event, State next_state, Event* recovery_event)
    { return EnterState1(event, next_state, recovery_event); });
  sm_->AddEntryAction(STATE2,
    [this] (Event event, State next_state, Event* recovery_event)
    { return EnterState2(event, next_state, recovery_event); });
  sm_->AddEntryAction(FAILED,
    [this] (Event event, State next_state, Event* recovery_event)
    { return EnterFailed(event, next_state, recovery_event); });
}

::util::Status ExampleStateMachine::ProcessEvent(Event event,
                                                 absl::string_view reason,
                                                 Event* recovery_event) {
  LOG(INFO) << "Processing Event " << event << " [" << reason << "]"
            << " to the example SM.";
  return sm_->ProcessEvent(event, reason, recovery_event);
}

void ExampleStateMachine::FillTransitionTable() {
  //    [state][event] -> next state
  table_[STATE0][FROM0] = STATE1;
  table_[STATE0][FAULT] = FAILED;

  table_[STATE1][FROM1] = STATE2;
  table_[STATE1][FAULT] = FAILED;

  table_[STATE2][FAULT] = FAILED;

  table_[FAILED][FAULT] = FAILED;
}

}  // namespace state_machine

}  // namespace stratum
