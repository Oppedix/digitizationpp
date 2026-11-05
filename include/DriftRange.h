#pragma once
#include <cmath>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

// Active drift interval [min_drift, max_drift] in mm used for the detector response
// (upstream PR #59 keys). Omitted keys give the upstream defaults 0 and 510 mm.
// Rules: 0 <= min_drift < max_drift, both finite; the removed 'drift_length' key is rejected.
inline std::pair<double,double> activeDriftRange(const std::map<std::string,std::string>& options) {
    if (options.count("drift_length"))
        throw std::invalid_argument("drift_length was replaced by min_drift/max_drift (mm)");
    auto value = [&](const std::string& key, double fallback) {
        const auto it = options.find(key);
        if (it == options.end() || it->second.empty()) return fallback;
        size_t used = 0;
        double v = 0.;
        try { v = std::stod(it->second, &used); } catch (const std::exception&) { used = 0; }
        if (used == 0 || used != it->second.size())
            throw std::invalid_argument(key + " must be a number (mm)");
        return v;
    };
    const double lo = value("min_drift", 0.);
    const double hi = value("max_drift", 510.);
    if (!std::isfinite(lo) || lo < 0.)
        throw std::invalid_argument("min_drift must be finite and >= 0 (mm)");
    if (!std::isfinite(hi) || hi <= lo)
        throw std::invalid_argument("max_drift must be finite and larger than min_drift (mm)");
    return {lo, hi};
}
