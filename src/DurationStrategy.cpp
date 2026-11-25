#include "DurationStrategy.h"

#include <cmath>
#include <stdexcept>

namespace
{
double diff(const DurationParams& p) { return std::fabs(p.actualSec - p.normativeSec); }

void requirePositive(double v, const char* what)
{
    if (!(v > 0))
        throw std::invalid_argument(std::string("Параметр має бути додатним: ") + what);
}
}

double AbsoluteDeviation::evaluate(const DurationParams& p) const { return diff(p); }
double AbsoluteDeviation::limit(const DurationParams& p) const { return p.toleranceSec; }

double RelativeDeviation::evaluate(const DurationParams& p) const
{
    requirePositive(p.normativeSec, "normativeSec");
    return diff(p) / p.normativeSec;
}
double RelativeDeviation::limit(const DurationParams& p) const
{
    requirePositive(p.normativeSec, "normativeSec");
    return p.toleranceSec / p.normativeSec;
}

double NormalizedDeviation::evaluate(const DurationParams& p) const
{
    requirePositive(p.toleranceSec, "toleranceSec");
    return diff(p) / p.toleranceSec;
}
double NormalizedDeviation::limit(const DurationParams&) const { return 1.0; }

DurationAnalyzer::DurationAnalyzer(std::unique_ptr<IDurationStrategy> strategy)
{
    setStrategy(std::move(strategy));
}

void DurationAnalyzer::setStrategy(std::unique_ptr<IDurationStrategy> strategy)
{
    if (!strategy)
        throw std::invalid_argument("Стратегія не може бути порожньою");
    strategy_ = std::move(strategy);
}

DeviationResult DurationAnalyzer::analyze(const DurationParams& p) const
{
    DeviationResult r;
    r.strategy = strategy_->name();
    r.value = strategy_->evaluate(p);
    r.limit = strategy_->limit(p);
    r.withinTolerance = r.value <= r.limit;
    return r;
}

std::unique_ptr<IDurationStrategy> makeStrategy(const std::string& name)
{
    if (name == "absolute")   return std::make_unique<AbsoluteDeviation>();
    if (name == "relative")   return std::make_unique<RelativeDeviation>();
    if (name == "normalized") return std::make_unique<NormalizedDeviation>();
    throw std::invalid_argument("Невідома стратегія: " + name);
}
