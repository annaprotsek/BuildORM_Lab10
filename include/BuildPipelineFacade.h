#pragma once

// Додатковий патерн: Facade. Єдина точка входу, яка приховує взаємодію
// State + Strategy + Observer + ORM-репозиторіїв + транзакцій.

#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "BuildDao.h"
#include "BuildEvents.h"
#include "BuildState.h"
#include "Daos10.h"
#include "Database.h"
#include "DurationStrategy.h"

class BuildPipelineFacade
{
public:
    using Clock = std::function<std::string()>;

    explicit BuildPipelineFacade(Storage& storage, Clock clock = nullptr);

    BuildEventPublisher& events() { return publisher_; }
    DurationAnalyzer& analyzer() { return analyzer_; }

    // Створює збірку у стані Created (+ перший запис історії), атомарно.
    int createBuild(int projectId, int buildNumber, const std::string& branch);

    // Переходи State. Кожен змінює builds.status і додає запис до історії в одній транзакції.
    void start(int buildId);
    void complete(int buildId, int durationSec);
    void fail(int buildId, int durationSec, const std::string& reason);
    void retry(int buildId);

    // Strategy: оцінка тривалості збірки поточною стратегією.
    DeviationResult evaluate(int buildId, double toleranceSec);

    std::vector<BuildStateHistory> history(int buildId);
    std::optional<Build> find(int buildId);

    // Тільки для тестів: викликається між UPDATE і INSERT усередині транзакції.
    void setFaultInjector(std::function<void()> f) { fault_ = std::move(f); }

private:
    enum class Action { Start, Complete, Fail, Retry };

    void transition(int buildId, Action action, int durationSec, const std::string& note);
    std::string now() const;

    Storage& storage_;
    BuildDao builds_;
    HistoryDao history_;
    BuildEventPublisher publisher_;
    DurationAnalyzer analyzer_;
    Clock clock_;
    std::function<void()> fault_;
};
