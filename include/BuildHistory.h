#pragma once

#include <string>

// Запис історії переходів між станами збірки (таблиця build_state_history).
struct BuildStateHistory
{
    int id = 0;
    int buildId = 0;            // зовнішній ключ -> Build::id
    std::string fromState;      // порожній рядок для першого запису
    std::string toState;
    std::string changedAt;      // "YYYY-MM-DD HH:MM:SS"
    std::string note;
};

// Запис журналу проєкту (таблиця project_log), створюється підписником Observer.
struct ProjectLogEntry
{
    int id = 0;
    int projectId = 0;          // зовнішній ключ -> Project::id
    std::string eventType;
    std::string message;
    std::string createdAt;
};
