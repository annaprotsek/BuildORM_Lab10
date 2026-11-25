#pragma once

#include <string>

// Імена станів збірки так, як вони зберігаються в полі builds.status.
namespace BuildStatus
{
inline const std::string Created   = "created";     // Створена
inline const std::string Running   = "running";     // Виконується
inline const std::string Completed = "completed";   // Завершена
inline const std::string Failed    = "failed";      // Помилка
}
