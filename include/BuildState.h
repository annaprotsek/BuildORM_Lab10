#pragma once

// Патерн State: стани збірки CI/CD та допустимі переходи між ними.
//
//   Created --start--> Running --complete--> Completed
//      |                  |
//      +-----fail---------+--fail--> Failed --retry--> Created
//
// Недопустимий перехід ніколи не змінює стан: метод кидає InvalidTransition.

#include <memory>
#include <stdexcept>
#include <string>

class InvalidTransition : public std::runtime_error
{
public:
    InvalidTransition(const std::string& from, const std::string& action)
        : std::runtime_error("Недопустимий перехід: дія '" + action + "' у стані '" + from + "'")
    {
    }
};

class BuildState
{
public:
    virtual ~BuildState() = default;

    virtual const std::string& name() const = 0;

    // Кожна дія повертає наступний стан або кидає InvalidTransition.
    virtual std::unique_ptr<BuildState> start() const;
    virtual std::unique_ptr<BuildState> complete() const;
    virtual std::unique_ptr<BuildState> fail() const;
    virtual std::unique_ptr<BuildState> retry() const;
};

class CreatedState : public BuildState
{
public:
    const std::string& name() const override;
    std::unique_ptr<BuildState> start() const override;
    std::unique_ptr<BuildState> fail() const override;
};

class RunningState : public BuildState
{
public:
    const std::string& name() const override;
    std::unique_ptr<BuildState> complete() const override;
    std::unique_ptr<BuildState> fail() const override;
};

class CompletedState : public BuildState   // кінцевий стан
{
public:
    const std::string& name() const override;
};

class FailedState : public BuildState
{
public:
    const std::string& name() const override;
    std::unique_ptr<BuildState> retry() const override;
};

// Відновлює об'єкт стану за значенням поля builds.status.
std::unique_ptr<BuildState> makeStateFromName(const std::string& name);
