#pragma once

#include <optional>
#include <vector>

#include "Database.h"

// Доступ до історії станів (Repository-стиль, поверх sqlite_orm).
class HistoryDao
{
public:
    explicit HistoryDao(Storage& storage) : storage_(storage) {}

    int add(const BuildStateHistory& h) { return storage_.insert(h); }

    std::vector<BuildStateHistory> getByBuild(int buildId)
    {
        using namespace sqlite_orm;
        return storage_.get_all<BuildStateHistory>(
            where(c(&BuildStateHistory::buildId) == buildId),
            order_by(&BuildStateHistory::id));
    }

private:
    Storage& storage_;
};

// Доступ до журналу проєкту.
class ProjectLogDao
{
public:
    explicit ProjectLogDao(Storage& storage) : storage_(storage) {}

    int add(const ProjectLogEntry& e) { return storage_.insert(e); }

    std::vector<ProjectLogEntry> getByProject(int projectId)
    {
        using namespace sqlite_orm;
        return storage_.get_all<ProjectLogEntry>(
            where(c(&ProjectLogEntry::projectId) == projectId),
            order_by(&ProjectLogEntry::id));
    }

private:
    Storage& storage_;
};
