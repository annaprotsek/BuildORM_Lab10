#pragma once

// Патерн Strategy: оцінка відхилення тривалості збірки від нормативної.

#include <memory>
#include <string>

struct DurationParams
{
    double actualSec = 0;       // фактична тривалість
    double normativeSec = 0;    // нормативна тривалість
    double toleranceSec = 0;    // допустиме відхилення, с
};

class IDurationStrategy
{
public:
    virtual ~IDurationStrategy() = default;
    virtual std::string name() const = 0;
    // Значення показника відхилення.
    virtual double evaluate(const DurationParams& p) const = 0;
    // Допустима межа показника в одиницях цієї ж стратегії.
    virtual double limit(const DurationParams& p) const = 0;
};

// |actual - normative| (секунди)
class AbsoluteDeviation : public IDurationStrategy
{
public:
    std::string name() const override { return "absolute"; }
    double evaluate(const DurationParams& p) const override;
    double limit(const DurationParams& p) const override;
};

// |actual - normative| / normative (частка)
class RelativeDeviation : public IDurationStrategy
{
public:
    std::string name() const override { return "relative"; }
    double evaluate(const DurationParams& p) const override;
    double limit(const DurationParams& p) const override;
};

// |actual - normative| / tolerance (кількість допусків; <= 1 означає "в межах норми")
class NormalizedDeviation : public IDurationStrategy
{
public:
    std::string name() const override { return "normalized"; }
    double evaluate(const DurationParams& p) const override;
    double limit(const DurationParams& p) const override;
};

struct DeviationResult
{
    std::string strategy;
    double value = 0;
    double limit = 0;
    bool withinTolerance = false;
};

// Контекст стратегії: алгоритм можна замінити під час виконання програми.
class DurationAnalyzer
{
public:
    explicit DurationAnalyzer(std::unique_ptr<IDurationStrategy> strategy);

    void setStrategy(std::unique_ptr<IDurationStrategy> strategy);
    const IDurationStrategy& strategy() const { return *strategy_; }
    DeviationResult analyze(const DurationParams& p) const;

private:
    std::unique_ptr<IDurationStrategy> strategy_;
};

// Вибір алгоритму за назвою ("absolute" | "relative" | "normalized").
std::unique_ptr<IDurationStrategy> makeStrategy(const std::string& name);
