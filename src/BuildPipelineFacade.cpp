#include "BuildPipelineFacade.h"

#include <ctime>

#include "BuildStatus.h"

namespace
{
std::string utcNow()
{
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}
}

BuildPipelineFacade::BuildPipelineFacade(Storage& storage, Clock clock)
    : storage_(storage),
      builds_(storage),
      history_(storage),
      analyzer_(std::make_unique<AbsoluteDeviation>()),
      clock_(std::move(clock))
{
}

std::string BuildPipelineFacade::now() const
{
    return clock_ ? clock_() : utcNow();
}

int BuildPipelineFacade::createBuild(int projectId, int buildNumber, const std::string& branch)
{
    int id = 0;
    const std::string ts = now();
    storage_.transaction([&] {
        id = builds_.add(Build{0, projectId, buildNumber, branch, BuildStatus::Created, 0, ts});
        history_.add(BuildStateHistory{0, id, "", BuildStatus::Created, ts, "збірку створено"});
        return true;
    });
    return id;
}

void BuildPipelineFacade::start(int id)   { transition(id, Action::Start, 0, "запуск збірки"); }
void BuildPipelineFacade::complete(int id, int sec) { transition(id, Action::Complete, sec, "збірку завершено успішно"); }
void BuildPipelineFacade::fail(int id, int sec, const std::string& reason) { transition(id, Action::Fail, sec, reason); }
void BuildPipelineFacade::retry(int id)   { transition(id, Action::Retry, 0, "повторна спроба"); }

void BuildPipelineFacade::transition(int buildId, Action action, int durationSec, const std::string& note)
{
    if (durationSec < 0)
        throw std::invalid_argument("Тривалість не може бути від'ємною");

    auto found = builds_.getById(buildId);
    if (!found)
        throw std::runtime_error("Збірку не знайдено: " + std::to_string(buildId));
    Build build = *found;

    // State: об'єкт стану вирішує, чи допустимий перехід. Якщо ні -> InvalidTransition,
    // і до бази даних ми навіть не звертаємось.
    auto current = makeStateFromName(build.status);
    std::unique_ptr<BuildState> next;
    switch (action)
    {
    case Action::Start:    next = current->start();    break;
    case Action::Complete: next = current->complete(); break;
    case Action::Fail:     next = current->fail();     break;
    case Action::Retry:    next = current->retry();    break;
    }

    const std::string from = build.status;
    const std::string ts = now();
    build.status = next->name();
    if (action == Action::Complete || action == Action::Fail)
        build.durationSec = durationSec;
    if (action == Action::Start || action == Action::Retry)
        build.startedAt = ts;

    // Атомарність: зміна статусу та запис історії - одна транзакція SQLite.
    // Виняток усередині лямбди -> ROLLBACK.
    storage_.transaction([&] {
        storage_.update(build);
        if (fault_) fault_();
        history_.add(BuildStateHistory{0, buildId, from, build.status, ts, note});
        return true;
    });

    // Observer: сповіщення лише ПІСЛЯ успішного COMMIT.
    if (action == Action::Start)
    {
        publisher_.publish(BuildEvent{BuildEventType::BuildStarted, buildId, build.projectId,
                                      build.status, 0, ts});
    }
    else if (action == Action::Complete || action == Action::Fail)
    {
        publisher_.publish(BuildEvent{BuildEventType::BuildFinished, buildId, build.projectId,
                                      build.status, build.durationSec, ts});
    }
}

DeviationResult BuildPipelineFacade::evaluate(int buildId, double toleranceSec)
{
    using namespace sqlite_orm;
    auto b = builds_.getById(buildId);
    if (!b)
        throw std::runtime_error("Збірку не знайдено: " + std::to_string(buildId));
    if (b->status != BuildStatus::Completed && b->status != BuildStatus::Failed)
        throw std::logic_error("Оцінювати можна лише збірки зі зафіксованою тривалістю");

    // Нормативна тривалість - середня тривалість успішних збірок цього проєкту (дані через Repository/DAO).
    auto cond = where(c(&Build::projectId) == b->projectId
                      && c(&Build::status) == BuildStatus::Completed
                      && c(&Build::id) != buildId);
    if (storage_.count<Build>(cond) == 0)
        throw std::runtime_error("Немає успішних збірок проєкту для визначення нормативу");
    const double normative = storage_.avg(&Build::durationSec, cond);

    DurationParams p{static_cast<double>(b->durationSec), normative, toleranceSec};
    return analyzer_.analyze(p);
}

std::vector<BuildStateHistory> BuildPipelineFacade::history(int buildId)
{
    return history_.getByBuild(buildId);
}

std::optional<Build> BuildPipelineFacade::find(int buildId)
{
    return builds_.getById(buildId);
}
