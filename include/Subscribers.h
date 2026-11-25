#pragma once

#include <map>

#include "BuildEvents.h"
#include "Daos10.h"

// Підписник 1: журнал проєкту. Зберігає кожну подію в таблиці project_log через ProjectLogDao.
class ProjectLogSubscriber : public IBuildObserver
{
public:
    explicit ProjectLogSubscriber(ProjectLogDao& dao) : dao_(dao) {}

    void onEvent(const BuildEvent& e) override
    {
        std::string msg = "Збірка #" + std::to_string(e.buildId) + " -> " + e.status;
        if (e.type == BuildEventType::BuildFinished)
            msg += " (" + std::to_string(e.durationSec) + " с)";
        dao_.add(ProjectLogEntry{0, e.projectId, toString(e.type), msg, e.timestamp});
    }

private:
    ProjectLogDao& dao_;
};

// Підписник 2: статистика виконання (у пам'яті, незалежно від БД).
class ExecutionStatsSubscriber : public IBuildObserver
{
public:
    void onEvent(const BuildEvent& e) override
    {
        if (e.type == BuildEventType::BuildStarted)
        {
            ++started_;
            return;
        }
        if (e.status == "completed") ++completed_;
        else                         ++failed_;
        totalDurationSec_ += e.durationSec;
        ++perProject_[e.projectId];
    }

    int started() const { return started_; }
    int completed() const { return completed_; }
    int failed() const { return failed_; }
    int totalDurationSec() const { return totalDurationSec_; }
    int finishedForProject(int projectId) const
    {
        auto it = perProject_.find(projectId);
        return it == perProject_.end() ? 0 : it->second;
    }

private:
    int started_ = 0, completed_ = 0, failed_ = 0, totalDurationSec_ = 0;
    std::map<int, int> perProject_;
};
