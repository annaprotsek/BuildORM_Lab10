#include <iomanip>
#include <iostream>

#include "BuildPipelineFacade.h"
#include "Subscribers.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace
{
void header(const std::string& t) { std::cout << "\n=== " << t << " ===\n"; }

void printHistory(BuildPipelineFacade& f, int buildId)
{
    for (const auto& h : f.history(buildId))
        std::cout << "  [" << h.changedAt << "] " << std::setw(9) << (h.fromState.empty() ? "-" : h.fromState)
                  << " -> " << std::setw(9) << h.toState << "  | " << h.note << '\n';
}

void tryAction(const std::string& what, const std::function<void()>& f)
{
    try { f(); std::cout << "  OK: " << what << '\n'; }
    catch (const std::exception& e) { std::cout << "  ПОМИЛКА: " << what << " -> " << e.what() << '\n'; }
}
}

int main()
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    Storage storage = openStorage("buildorm_lab10.db");
    clearDatabase(storage);
    seedDatabase(storage);

    BuildPipelineFacade facade(storage);
    ProjectLogDao logDao(storage);

    // --- Observer: два незалежні підписники ---
    auto logger = std::make_shared<ProjectLogSubscriber>(logDao);
    auto stats = std::make_shared<ExecutionStatsSubscriber>();
    facade.events().subscribe(logger);
    facade.events().subscribe(stats);

    const int projectId = 1;   // WebShop (має 3 успішні збірки в початкових даних)

    header("1. State: успішний життєвий цикл збірки");
    int b1 = facade.createBuild(projectId, 5, "main");
    facade.start(b1);
    facade.complete(b1, 320);
    printHistory(facade, b1);

    header("2. State: збірка з помилкою та повторна спроба");
    int b2 = facade.createBuild(projectId, 6, "develop");
    facade.start(b2);
    facade.fail(b2, 80, "тест PaymentTest провалено");
    facade.retry(b2);
    facade.start(b2);
    facade.complete(b2, 450);
    printHistory(facade, b2);

    header("3. State: недопустимі переходи");
    tryAction("complete() для Completed-збірки", [&] { facade.complete(b1, 1); });
    tryAction("start() для Completed-збірки", [&] { facade.start(b1); });
    int b3 = facade.createBuild(projectId, 7, "main");
    tryAction("complete() для Created-збірки", [&] { facade.complete(b3, 1); });
    std::cout << "  Стан b3 залишився: " << facade.find(b3)->status
              << ", записів історії: " << facade.history(b3).size() << '\n';

    header("4. Транзакція: збій між UPDATE та INSERT історії -> ROLLBACK");
    facade.setFaultInjector([] { throw std::runtime_error("імітація збою диска"); });
    tryAction("start() зі збоєм", [&] { facade.start(b3); });
    facade.setFaultInjector(nullptr);
    std::cout << "  Стан b3 після ROLLBACK: " << facade.find(b3)->status
              << ", записів історії: " << facade.history(b3).size() << '\n';

    header("5. Strategy: три алгоритми, заміна під час роботи");
    for (int id : {b1, b2})
    {
        std::cout << "  Збірка #" << id << " (" << facade.find(id)->durationSec << " с), допуск 60 с:\n";
        for (const char* name : {"absolute", "relative", "normalized"})
        {
            facade.analyzer().setStrategy(makeStrategy(name));   // заміна алгоритму під час виконання
            auto r = facade.evaluate(id, 60.0);
            std::cout << "    " << std::left << std::setw(11) << r.strategy << std::right
                      << " значення=" << std::fixed << std::setprecision(3) << r.value
                      << "  межа=" << r.limit
                      << "  в межах норми: " << (r.withinTolerance ? "так" : "ні") << '\n';
        }
    }

    header("6. Observer: відключення підписника");
    facade.events().unsubscribe(logger);
    int b4 = facade.createBuild(projectId, 8, "main");
    facade.start(b4);
    facade.complete(b4, 300);
    std::cout << "  Підписників зараз: " << facade.events().subscriberCount() << '\n';
    facade.events().subscribe(logger);   // повторне підключення
    int b5 = facade.createBuild(projectId, 9, "main");
    facade.start(b5);
    facade.fail(b5, 40, "помилка компіляції");

    header("Журнал проєкту (підписник 1, таблиця project_log)");
    for (const auto& e : logDao.getByProject(projectId))
        std::cout << "  [" << e.createdAt << "] " << std::left << std::setw(14) << e.eventType << std::right
                  << e.message << '\n';

    header("Статистика виконання (підписник 2)");
    std::cout << "  запущено: " << stats->started() << ", завершено успішно: " << stats->completed()
              << ", з помилкою: " << stats->failed() << ", сумарна тривалість: " << stats->totalDurationSec()
              << " с\n";
    std::cout << "  завершених збірок проєкту " << projectId << ": " << stats->finishedForProject(projectId) << '\n';
    return 0;
}
