#pragma once

// Патерн Observer: події збірки, підписники та видавець.

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

enum class BuildEventType
{
    BuildStarted,       // запуск збірки
    BuildFinished       // завершення збірки (успішне або з помилкою)
};

inline const char* toString(BuildEventType t)
{
    return t == BuildEventType::BuildStarted ? "BuildStarted" : "BuildFinished";
}

struct BuildEvent
{
    BuildEventType type = BuildEventType::BuildStarted;
    int buildId = 0;
    int projectId = 0;
    std::string status;         // новий стан збірки
    int durationSec = 0;
    std::string timestamp;
};

class IBuildObserver
{
public:
    virtual ~IBuildObserver() = default;
    virtual void onEvent(const BuildEvent& event) = 0;
};

// Видавець не володіє підписниками: зберігає std::weak_ptr.
class BuildEventPublisher
{
public:
    void subscribe(const std::shared_ptr<IBuildObserver>& observer)
    {
        if (!observer) return;
        for (const auto& w : observers_)
            if (w.lock() == observer) return;       // повторна підписка ігнорується
        observers_.push_back(observer);
    }

    void unsubscribe(const std::shared_ptr<IBuildObserver>& observer)
    {
        std::erase_if(observers_, [&](const std::weak_ptr<IBuildObserver>& w) {
            auto p = w.lock();
            return !p || p == observer;
        });
    }

    size_t subscriberCount() const
    {
        return static_cast<size_t>(std::count_if(observers_.begin(), observers_.end(),
            [](const std::weak_ptr<IBuildObserver>& w) { return !w.expired(); }));
    }

    // Синхронне сповіщення. Помилка одного підписника не заважає іншим.
    // Повертає кількість підписників, що завершили обробку без помилки.
    int publish(const BuildEvent& event)
    {
        std::erase_if(observers_, [](const std::weak_ptr<IBuildObserver>& w) { return w.expired(); });

        std::vector<std::shared_ptr<IBuildObserver>> active;   // копія: підписник може відписатись під час обробки
        for (const auto& w : observers_)
            if (auto p = w.lock()) active.push_back(std::move(p));

        int ok = 0;
        for (const auto& o : active)
        {
            try
            {
                o->onEvent(event);
                ++ok;
            }
            catch (const std::exception& e)
            {
                std::cerr << "[Observer] помилка підписника: " << e.what() << '\n';
            }
        }
        return ok;
    }

private:
    std::vector<std::weak_ptr<IBuildObserver>> observers_;
};
