#include "BuildState.h"

#include "BuildStatus.h"

std::unique_ptr<BuildState> BuildState::start() const    { throw InvalidTransition(name(), "start"); }
std::unique_ptr<BuildState> BuildState::complete() const { throw InvalidTransition(name(), "complete"); }
std::unique_ptr<BuildState> BuildState::fail() const     { throw InvalidTransition(name(), "fail"); }
std::unique_ptr<BuildState> BuildState::retry() const    { throw InvalidTransition(name(), "retry"); }

const std::string& CreatedState::name() const   { return BuildStatus::Created; }
const std::string& RunningState::name() const   { return BuildStatus::Running; }
const std::string& CompletedState::name() const { return BuildStatus::Completed; }
const std::string& FailedState::name() const    { return BuildStatus::Failed; }

std::unique_ptr<BuildState> CreatedState::start() const { return std::make_unique<RunningState>(); }
std::unique_ptr<BuildState> CreatedState::fail() const  { return std::make_unique<FailedState>(); }

std::unique_ptr<BuildState> RunningState::complete() const { return std::make_unique<CompletedState>(); }
std::unique_ptr<BuildState> RunningState::fail() const     { return std::make_unique<FailedState>(); }

std::unique_ptr<BuildState> FailedState::retry() const { return std::make_unique<CreatedState>(); }

std::unique_ptr<BuildState> makeStateFromName(const std::string& name)
{
    if (name == BuildStatus::Created)   return std::make_unique<CreatedState>();
    if (name == BuildStatus::Running)   return std::make_unique<RunningState>();
    if (name == BuildStatus::Completed) return std::make_unique<CompletedState>();
    if (name == BuildStatus::Failed)    return std::make_unique<FailedState>();
    throw std::invalid_argument("Невідомий стан збірки: " + name);
}
