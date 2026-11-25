#include <gtest/gtest.h>

#include "BuildPipelineFacade.h"
#include "BuildStatus.h"
#include "ProjectRepository.h"
#include "Subscribers.h"

namespace
{
// Кожен тест працює з власною базою SQLite у пам'яті.
class PipelineTest : public ::testing::Test
{
protected:
    PipelineTest()
        : storage(openStorage(":memory:")),
          facade(storage, [] { return std::string("2026-11-25 12:00:00"); }),
          logDao(storage)
    {
        seedDatabase(storage);     // 5 проєктів + 18 збірок (проєкт 1: 3 успішні збірки: 312, 340, 305)
    }

    Storage storage;
    BuildPipelineFacade facade;
    ProjectLogDao logDao;
    const int projectId = 1;
};

// ---------- State ----------

TEST(StateTest, AllowedTransitions)
{
    EXPECT_EQ(CreatedState().start()->name(), BuildStatus::Running);
    EXPECT_EQ(CreatedState().fail()->name(), BuildStatus::Failed);
    EXPECT_EQ(RunningState().complete()->name(), BuildStatus::Completed);
    EXPECT_EQ(RunningState().fail()->name(), BuildStatus::Failed);
    EXPECT_EQ(FailedState().retry()->name(), BuildStatus::Created);
}

TEST(StateTest, ForbiddenTransitionsThrow)
{
    EXPECT_THROW(CreatedState().complete(), InvalidTransition);
    EXPECT_THROW(CreatedState().retry(), InvalidTransition);
    EXPECT_THROW(RunningState().start(), InvalidTransition);
    EXPECT_THROW(RunningState().retry(), InvalidTransition);
    EXPECT_THROW(CompletedState().start(), InvalidTransition);
    EXPECT_THROW(CompletedState().complete(), InvalidTransition);
    EXPECT_THROW(CompletedState().fail(), InvalidTransition);
    EXPECT_THROW(CompletedState().retry(), InvalidTransition);
    EXPECT_THROW(FailedState().start(), InvalidTransition);
    EXPECT_THROW(FailedState().complete(), InvalidTransition);
}

TEST(StateTest, FactoryRestoresStateByName)
{
    for (const auto* n : {&BuildStatus::Created, &BuildStatus::Running, &BuildStatus::Completed, &BuildStatus::Failed})
        EXPECT_EQ(makeStateFromName(*n)->name(), *n);
    EXPECT_THROW(makeStateFromName("unknown"), std::invalid_argument);
}

TEST_F(PipelineTest, FullLifecycleIsPersistedWithHistory)
{
    int id = facade.createBuild(projectId, 100, "main");
    facade.start(id);
    facade.complete(id, 320);

    auto b = facade.find(id);
    ASSERT_TRUE(b.has_value());
    EXPECT_EQ(b->status, BuildStatus::Completed);
    EXPECT_EQ(b->durationSec, 320);

    auto h = facade.history(id);
    ASSERT_EQ(h.size(), 3u);
    EXPECT_EQ(h[0].fromState, "");
    EXPECT_EQ(h[0].toState, BuildStatus::Created);
    EXPECT_EQ(h[1].fromState, BuildStatus::Created);
    EXPECT_EQ(h[1].toState, BuildStatus::Running);
    EXPECT_EQ(h[2].fromState, BuildStatus::Running);
    EXPECT_EQ(h[2].toState, BuildStatus::Completed);
}

TEST_F(PipelineTest, FailedBuildCanBeRetried)
{
    int id = facade.createBuild(projectId, 101, "main");
    facade.start(id);
    facade.fail(id, 50, "помилка тесту");
    EXPECT_EQ(facade.find(id)->status, BuildStatus::Failed);
    facade.retry(id);
    EXPECT_EQ(facade.find(id)->status, BuildStatus::Created);
    EXPECT_EQ(facade.history(id).size(), 4u);
}

TEST_F(PipelineTest, InvalidTransitionLeavesDatabaseUntouched)
{
    int id = facade.createBuild(projectId, 102, "main");
    EXPECT_THROW(facade.complete(id, 10), InvalidTransition);
    EXPECT_EQ(facade.find(id)->status, BuildStatus::Created);
    EXPECT_EQ(facade.history(id).size(), 1u);
}

TEST_F(PipelineTest, CompletedIsTerminal)
{
    int id = facade.createBuild(projectId, 103, "main");
    facade.start(id);
    facade.complete(id, 100);
    EXPECT_THROW(facade.start(id), InvalidTransition);
    EXPECT_THROW(facade.fail(id, 1, "x"), InvalidTransition);
    EXPECT_THROW(facade.retry(id), InvalidTransition);
    EXPECT_EQ(facade.history(id).size(), 3u);
}

TEST_F(PipelineTest, UnknownBuildAndNegativeDurationAreRejected)
{
    EXPECT_THROW(facade.start(9999), std::runtime_error);
    int id = facade.createBuild(projectId, 104, "main");
    facade.start(id);
    EXPECT_THROW(facade.complete(id, -5), std::invalid_argument);
    EXPECT_EQ(facade.find(id)->status, BuildStatus::Running);
}

// ---------- Транзакції ----------

TEST_F(PipelineTest, FailureBetweenUpdateAndHistoryRollsBack)
{
    int id = facade.createBuild(projectId, 105, "main");
    facade.setFaultInjector([] { throw std::runtime_error("збій"); });
    EXPECT_THROW(facade.start(id), std::runtime_error);
    facade.setFaultInjector(nullptr);

    // UPDATE відкотився разом із відсутнім INSERT
    EXPECT_EQ(facade.find(id)->status, BuildStatus::Created);
    EXPECT_EQ(facade.history(id).size(), 1u);

    // Після відкоту система працездатна
    EXPECT_NO_THROW(facade.start(id));
    EXPECT_EQ(facade.find(id)->status, BuildStatus::Running);
}

TEST_F(PipelineTest, NoEventIsPublishedWhenTransactionFails)
{
    auto stats = std::make_shared<ExecutionStatsSubscriber>();
    facade.events().subscribe(stats);
    int id = facade.createBuild(projectId, 106, "main");
    facade.setFaultInjector([] { throw std::runtime_error("збій"); });
    EXPECT_THROW(facade.start(id), std::runtime_error);
    EXPECT_EQ(stats->started(), 0);
}

// ---------- Strategy ----------

TEST(StrategyTest, ThreeAlgorithms)
{
    DurationParams p{450, 319, 60};
    EXPECT_DOUBLE_EQ(AbsoluteDeviation().evaluate(p), 131.0);
    EXPECT_NEAR(RelativeDeviation().evaluate(p), 131.0 / 319.0, 1e-12);
    EXPECT_NEAR(NormalizedDeviation().evaluate(p), 131.0 / 60.0, 1e-12);
}

TEST(StrategyTest, DeviationIsSymmetricForFasterBuild)
{
    DurationParams p{200, 300, 50};
    EXPECT_DOUBLE_EQ(AbsoluteDeviation().evaluate(p), 100.0);
    EXPECT_DOUBLE_EQ(NormalizedDeviation().evaluate(p), 2.0);
}

TEST(StrategyTest, LimitsAndTolerance)
{
    DurationParams p{330, 300, 40};        // відхилення 30 с, допуск 40 с -> у нормі
    DurationAnalyzer a(std::make_unique<AbsoluteDeviation>());
    EXPECT_TRUE(a.analyze(p).withinTolerance);
    a.setStrategy(std::make_unique<RelativeDeviation>());
    EXPECT_TRUE(a.analyze(p).withinTolerance);
    a.setStrategy(std::make_unique<NormalizedDeviation>());
    EXPECT_TRUE(a.analyze(p).withinTolerance);

    DurationParams bad{400, 300, 40};      // відхилення 100 с > допуск
    for (const char* n : {"absolute", "relative", "normalized"})
    {
        a.setStrategy(makeStrategy(n));
        EXPECT_FALSE(a.analyze(bad).withinTolerance) << n;
    }
}

TEST(StrategyTest, StrategyCanBeReplacedAtRuntime)
{
    DurationAnalyzer a(std::make_unique<AbsoluteDeviation>());
    DurationParams p{150, 100, 25};
    EXPECT_EQ(a.analyze(p).strategy, "absolute");
    EXPECT_DOUBLE_EQ(a.analyze(p).value, 50.0);
    a.setStrategy(std::make_unique<NormalizedDeviation>());
    EXPECT_EQ(a.analyze(p).strategy, "normalized");
    EXPECT_DOUBLE_EQ(a.analyze(p).value, 2.0);
}

TEST(StrategyTest, InvalidParametersAndNullStrategy)
{
    EXPECT_THROW(RelativeDeviation().evaluate({10, 0, 5}), std::invalid_argument);
    EXPECT_THROW(NormalizedDeviation().evaluate({10, 5, 0}), std::invalid_argument);
    EXPECT_THROW(DurationAnalyzer(nullptr), std::invalid_argument);
    EXPECT_THROW(makeStrategy("nope"), std::invalid_argument);
}

TEST_F(PipelineTest, EvaluateUsesNormativeFromDatabase)
{
    // Норматив проєкту 1 = середнє з 312, 340, 305 = 319
    int id = facade.createBuild(projectId, 107, "main");
    facade.start(id);
    facade.complete(id, 450);
    facade.analyzer().setStrategy(std::make_unique<AbsoluteDeviation>());
    auto r = facade.evaluate(id, 60);
    EXPECT_NEAR(r.value, 450.0 - 319.0 , 1e-9) << "норматив має враховувати лише ІНШІ успішні збірки";
}

TEST_F(PipelineTest, EvaluateRequiresFinishedBuild)
{
    int id = facade.createBuild(projectId, 108, "main");
    EXPECT_THROW(facade.evaluate(id, 60), std::logic_error);
}

// ---------- Observer ----------

TEST_F(PipelineTest, TwoIndependentSubscribersReceiveBothEventTypes)
{
    auto logger = std::make_shared<ProjectLogSubscriber>(logDao);
    auto stats = std::make_shared<ExecutionStatsSubscriber>();
    facade.events().subscribe(logger);
    facade.events().subscribe(stats);

    int id = facade.createBuild(projectId, 109, "main");
    facade.start(id);
    facade.complete(id, 300);

    EXPECT_EQ(stats->started(), 1);
    EXPECT_EQ(stats->completed(), 1);
    EXPECT_EQ(stats->totalDurationSec(), 300);
    auto log = logDao.getByProject(projectId);
    ASSERT_EQ(log.size(), 2u);
    EXPECT_EQ(log[0].eventType, "BuildStarted");
    EXPECT_EQ(log[1].eventType, "BuildFinished");
}

TEST_F(PipelineTest, FailedBuildIsCountedAsFailure)
{
    auto stats = std::make_shared<ExecutionStatsSubscriber>();
    facade.events().subscribe(stats);
    int id = facade.createBuild(projectId, 110, "main");
    facade.start(id);
    facade.fail(id, 40, "помилка");
    EXPECT_EQ(stats->failed(), 1);
    EXPECT_EQ(stats->completed(), 0);
}

TEST_F(PipelineTest, UnsubscribedObserverStopsReceivingEvents)
{
    auto logger = std::make_shared<ProjectLogSubscriber>(logDao);
    auto stats = std::make_shared<ExecutionStatsSubscriber>();
    facade.events().subscribe(logger);
    facade.events().subscribe(stats);
    facade.events().unsubscribe(logger);

    int id = facade.createBuild(projectId, 111, "main");
    facade.start(id);
    EXPECT_EQ(stats->started(), 1);
    EXPECT_TRUE(logDao.getByProject(projectId).empty());
    EXPECT_EQ(facade.events().subscriberCount(), 1u);
}

TEST_F(PipelineTest, DestroyedSubscriberIsSkippedSafely)
{
    {
        auto stats = std::make_shared<ExecutionStatsSubscriber>();
        facade.events().subscribe(stats);
    }   // stats знищено: видавець зберігає лише weak_ptr
    EXPECT_EQ(facade.events().subscriberCount(), 0u);
    int id = facade.createBuild(projectId, 112, "main");
    EXPECT_NO_THROW(facade.start(id));
}

TEST_F(PipelineTest, DuplicateSubscriptionIsIgnored)
{
    auto stats = std::make_shared<ExecutionStatsSubscriber>();
    facade.events().subscribe(stats);
    facade.events().subscribe(stats);
    int id = facade.createBuild(projectId, 113, "main");
    facade.start(id);
    EXPECT_EQ(stats->started(), 1);
}

struct ThrowingObserver : IBuildObserver
{
    void onEvent(const BuildEvent&) override { throw std::runtime_error("збій підписника"); }
};

TEST_F(PipelineTest, FaultySubscriberDoesNotBlockOthers)
{
    auto bad = std::make_shared<ThrowingObserver>();
    auto stats = std::make_shared<ExecutionStatsSubscriber>();
    facade.events().subscribe(bad);
    facade.events().subscribe(stats);
    int id = facade.createBuild(projectId, 114, "main");
    EXPECT_NO_THROW(facade.start(id));
    EXPECT_EQ(stats->started(), 1);
    EXPECT_EQ(facade.find(id)->status, BuildStatus::Running);   // стан збережено попри збій підписника
}

struct SelfUnsubscribing : IBuildObserver
{
    BuildEventPublisher* pub = nullptr;
    std::shared_ptr<IBuildObserver> self;
    int calls = 0;
    void onEvent(const BuildEvent&) override { ++calls; pub->unsubscribe(self); }
};

TEST_F(PipelineTest, ObserverMayUnsubscribeDuringNotification)
{
    auto o = std::make_shared<SelfUnsubscribing>();
    o->pub = &facade.events();
    o->self = o;
    facade.events().subscribe(o);
    int id = facade.createBuild(projectId, 115, "main");
    facade.start(id);
    facade.fail(id, 5, "x");
    EXPECT_EQ(o->calls, 1);
    o->self.reset();            // розриваємо цикл shared_ptr
}

// ---------- Інтеграція з ORM ----------

TEST_F(PipelineTest, HistoryRowsRequireExistingBuild)
{
    HistoryDao dao(storage);
    EXPECT_THROW(dao.add(BuildStateHistory{0, 99999, "", "created", "t", "n"}), std::system_error);
}

TEST_F(PipelineTest, SeedHasHistoryAndLab8DataIsPreserved)
{
    EXPECT_EQ(storage.count<Project>(), 5);
    EXPECT_EQ(storage.count<Build>(), 18);
    EXPECT_EQ(storage.count<BuildStateHistory>(), 18);
}
}
