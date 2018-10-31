#include "stratum/lib/statemachine/example_state_machine.h"

#include "gmock/gmock.h"

namespace stratum {

namespace state_machine {
namespace {

using ::testing::HasSubstr;
using ::testing::Return;
using ::testing::Sequence;
using ::stratum::test_utils::StatusIs;

class StateMachineTest : public ::testing::Test {
 protected:
  StateMachineTest() { example_sm_.StartStateMachine(); }
  ExampleStateMachine example_sm_;
};

TEST_F(StateMachineTest, BasicExecution) {
  // Check that the default initial state is set correctly.
  ASSERT_EQ(example_sm_.CurrentState(), ExampleStateMachine::State::STATE0);

  // Test a transition from the initial state, ensuring that exit actions of
  // STATE0 are performed before the entry actions of STATE1.
  Sequence ExitBeforeEntry;
  EXPECT_CALL(example_sm_, ExitState0(ExampleStateMachine::Event::FROM0,
    ExampleStateMachine::State::STATE1, nullptr))
    .InSequence(ExitBeforeEntry);
  EXPECT_CALL(example_sm_, EnterState1(ExampleStateMachine::Event::FROM0,
    ExampleStateMachine::State::STATE1, nullptr))
    .InSequence(ExitBeforeEntry);
  EXPECT_OK(example_sm_.ProcessEvent(ExampleStateMachine::Event::FROM0,
    "Add FROM0 event", nullptr));
  EXPECT_EQ(example_sm_.CurrentState(), ExampleStateMachine::State::STATE1);

  // Test a transition from a subsequent state. Notice there is no ExitState1
  // step, which is OK since entry and exit actions are optional for each state.
  EXPECT_CALL(example_sm_, EnterFailed(ExampleStateMachine::Event::FAULT,
    ExampleStateMachine::State::FAILED, nullptr));
  EXPECT_OK(example_sm_.ProcessEvent(ExampleStateMachine::Event::FAULT,
    "Add FAULT event", nullptr));
  EXPECT_EQ(example_sm_.CurrentState(), ExampleStateMachine::State::FAILED);
}

TEST_F(StateMachineTest, InvalidTransitionShouldNotChangeState) {
  // Check that current state is maintained when an invalid event is added.
  EXPECT_THAT(example_sm_.ProcessEvent(ExampleStateMachine::Event::FROM1,
    "Check processing of invalid transitions", nullptr),
    StatusIs(util::error::INTERNAL, HasSubstr("Invalid transition.")));
  EXPECT_EQ(example_sm_.CurrentState(), ExampleStateMachine::State::STATE0);
}

TEST(ResumeStateMachineTest, CallbackFailureShouldNotChangeState) {
  ExampleStateMachine example_sm;
  ExampleStateMachine::Event error_event;
  example_sm.StartStateMachine(ExampleStateMachine::State::STATE1);

  // Check that the initial state is set correctly (resume behavior).
  EXPECT_EQ(example_sm.CurrentState(), ExampleStateMachine::State::STATE1);

  // Check that the transition to STATE2 fails.
  auto fail = ::util::Status(util::error::INTERNAL, "Failures are fun!");
  EXPECT_CALL(example_sm, EnterState2(ExampleStateMachine::Event::FROM1,
    ExampleStateMachine::State::STATE2, &error_event))
    .WillOnce(Return(fail));
  EXPECT_THAT(example_sm.ProcessEvent(ExampleStateMachine::Event::FROM1,
    "Check processing of failed callbacks", &error_event),
    StatusIs(util::error::INTERNAL, HasSubstr("Failures are fun!")));
  EXPECT_EQ(example_sm.CurrentState(), ExampleStateMachine::State::STATE1);
}

TEST_F(StateMachineTest, AddEventsAfterCallbackFailure) {
  auto fail = ::util::Status(util::error::INTERNAL, "Fail on first try!");
  EXPECT_CALL(example_sm_, ExitState0)
    .WillOnce(Return(fail))
    .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(example_sm_, EnterState1);

  EXPECT_THAT(example_sm_.ProcessEvent(ExampleStateMachine::Event::FROM0,
    "Add specified event", nullptr),
    StatusIs(util::error::INTERNAL, HasSubstr("Fail on first try!")));
  EXPECT_OK(example_sm_.ProcessEvent(ExampleStateMachine::Event::FROM0,
    "Add FROM0 event", nullptr));
  EXPECT_EQ(example_sm_.CurrentState(), ExampleStateMachine::State::STATE1);
}

// Stores information required by a thread when calling ProcessEventCallback.
struct ThreadData {
  ThreadData(ExampleStateMachine* example_sm,
    ExampleStateMachine::Event event, ::util::Status* status)
    : example_sm(example_sm), event(event), status(status) {}
  ExampleStateMachine* example_sm;
  ExampleStateMachine::Event event;
  ::util::Status* status;
};

// Callback function for adding events to the ExampleStateMachine.
void* ProcessEventCallback(void* in_data) {
  auto thread_data = static_cast<ThreadData*>(in_data);
  *(thread_data->status) = thread_data->example_sm->ProcessEvent(
    thread_data->event, "Add specified event", nullptr);
  return nullptr;
}

TEST_F(StateMachineTest, DuplicateEventsFromDifferentThreads) {
  ::util::Status status_1, status_2;
  ThreadData data_1(&example_sm_, ExampleStateMachine::Event::FROM0, &status_1);
  ThreadData data_2(&example_sm_, ExampleStateMachine::Event::FROM0, &status_2);

  // The transition should only occur once.
  EXPECT_CALL(example_sm_, ExitState0);
  EXPECT_CALL(example_sm_, EnterState1);

  pthread_t thread_1, thread_2;
  pthread_create(&thread_1, nullptr, ProcessEventCallback, &data_1);
  pthread_create(&thread_2, nullptr, ProcessEventCallback, &data_2);
  pthread_join(thread_1, nullptr);
  pthread_join(thread_2, nullptr);

  // One of the FROM0 events should be discarded since it yields an invalid
  // transition from STATE1 once the first FROM0 event is processed.
  EXPECT_TRUE(status_1.ok() ^ status_2.ok());
  EXPECT_EQ(example_sm_.CurrentState(), ExampleStateMachine::State::STATE1);
}

}  // namespace
}  // namespace state_machine

}  // namespace stratum
