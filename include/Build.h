#pragma once

#include <string>

// Залежна (дочірня) сутність: одна збірка CI/CD проєкту.
struct Build
{
    int id = 0;
    int projectId = 0;          // зовнішній ключ -> Project::id
    int buildNumber = 0;
    std::string branch;
    std::string status;         // стан збірки, див. BuildStatus.h: created | running | completed | failed
    int durationSec = 0;        // числове поле для агрегатів
    std::string startedAt;      // "YYYY-MM-DD HH:MM:SS"
};
