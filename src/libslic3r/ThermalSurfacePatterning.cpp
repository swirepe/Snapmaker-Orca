#include "ThermalSurfacePatterning.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>

namespace Slic3r {

namespace {

constexpr double epsilon = 1e-9;

template<class T> void hash_combine(std::size_t &seed, const T &value)
{
    seed ^= std::hash<T>{}(value) + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
}

std::uint64_t splitmix64(std::uint64_t value)
{
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

} // namespace

std::size_t ThermalPatternSettings::hash() const
{
    std::size_t out = 0;
    hash_combine(out, seed);
    hash_combine(out, max_level);
    hash_combine(out, top_max_level);
    hash_combine(out, band_median);
    hash_combine(out, band_sigma);
    hash_combine(out, band_min);
    hash_combine(out, band_max);
    hash_combine(out, dark_band_narrowing);
    hash_combine(out, stay_weight);
    hash_combine(out, adjacent_weight);
    hash_combine(out, two_away_weight);
    hash_combine(out, far_weight);
    hash_combine(out, darkness_bias);
    hash_combine(out, trend_persistence);
    hash_combine(out, trend_strength);
    hash_combine(out, accent_chance);
    hash_combine(out, accent_boost);
    hash_combine(out, accent_min);
    hash_combine(out, accent_max);
    return out;
}

std::size_t ThermalPatternGenerator::CacheKeyHash::operator()(const CacheKey &key) const
{
    std::size_t out = std::hash<std::uint64_t>{}(key.object_id);
    hash_combine(out, key.settings_hash);
    return out;
}

std::uint64_t ThermalPatternGenerator::mixed_seed(int seed, std::uint64_t object_id, std::uint64_t salt)
{
    return splitmix64(static_cast<std::uint64_t>(static_cast<std::uint32_t>(seed)) ^
                      splitmix64(object_id) ^ splitmix64(salt));
}

int ThermalPatternGenerator::choose_level(std::mt19937_64 &rng, int current, double trend,
                                          int cap, const ThermalPatternSettings &settings)
{
    cap = std::max(0, cap);
    current = std::clamp(current, 0, cap);
    std::vector<double> weights;
    weights.reserve(static_cast<std::size_t>(cap + 1));
    for (int candidate = 0; candidate <= cap; ++candidate) {
        const int distance = std::abs(candidate - current);
        const double transition = distance == 0 ? settings.stay_weight :
                                  distance == 1 ? settings.adjacent_weight :
                                  distance == 2 ? settings.two_away_weight : settings.far_weight;
        const double directional = std::exp(settings.trend_strength * trend * (candidate - current));
        const double darkness = std::pow(std::max(epsilon, settings.darkness_bias), candidate);
        weights.push_back(std::max(0.0, transition) * directional * darkness);
    }
    if (std::none_of(weights.begin(), weights.end(), [](double weight) { return weight > 0.0; }))
        return current;
    std::discrete_distribution<int> distribution(weights.begin(), weights.end());
    return distribution(rng);
}

double ThermalPatternGenerator::evolve_trend(std::mt19937_64 &rng, double trend,
                                             const ThermalPatternSettings &settings)
{
    const double persistence = std::clamp(settings.trend_persistence, 0.0, 0.999);
    std::normal_distribution<double> innovation(0.0, 0.75);
    const double scale = std::sqrt(std::max(0.0, 1.0 - persistence * persistence));
    return std::clamp(persistence * trend + scale * innovation(rng), -1.5, 1.5);
}

double ThermalPatternGenerator::band_thickness(std::mt19937_64 &rng, int level,
                                                const ThermalPatternSettings &settings)
{
    const double median = std::max(0.01, settings.band_median);
    std::lognormal_distribution<double> distribution(std::log(median), std::max(0.0, settings.band_sigma));
    const double narrowing = std::max(0.25, 1.0 - std::max(0.0, settings.dark_band_narrowing) * level);
    return std::clamp(distribution(rng) * narrowing,
                      std::max(0.01, settings.band_min),
                      std::max(std::max(0.01, settings.band_min), settings.band_max));
}

void ThermalPatternGenerator::append_next_band(ObjectPattern &pattern, const ThermalPatternSettings &settings)
{
    const int cap = std::max(0, settings.max_level);
    if (pattern.level < 0) {
        std::vector<double> initial_weights;
        initial_weights.reserve(static_cast<std::size_t>(cap + 1));
        for (int candidate = 0; candidate <= cap; ++candidate)
            initial_weights.push_back(std::pow(std::max(epsilon, settings.darkness_bias), candidate));
        std::discrete_distribution<int> initial(initial_weights.begin(), initial_weights.end());
        pattern.level = initial(pattern.rng);
    }

    pattern.trend = evolve_trend(pattern.rng, pattern.trend, settings);
    const double thickness = band_thickness(pattern.rng, pattern.level, settings);
    pattern.bands.push_back({pattern.cursor, pattern.cursor + thickness, pattern.level, false});
    pattern.cursor += thickness;

    std::uniform_real_distribution<double> unit(0.0, 1.0);
    if (unit(pattern.rng) < std::clamp(settings.accent_chance, 0.0, 1.0)) {
        const double lo = std::max(0.01, settings.accent_min);
        const double hi = std::max(lo, settings.accent_max);
        std::uniform_real_distribution<double> accent_size(lo, hi);
        const double accent_thickness = accent_size(pattern.rng);
        pattern.bands.push_back({pattern.cursor, pattern.cursor + accent_thickness,
                                 std::min(cap, pattern.level + std::max(0, settings.accent_boost)), true});
        pattern.cursor += accent_thickness;
    }

    pattern.level = choose_level(pattern.rng, pattern.level, pattern.trend, cap, settings);
}

int ThermalPatternGenerator::level_for(std::uint64_t object_id, double z, const ThermalPatternSettings &settings)
{
    const CacheKey key {object_id, settings.hash()};
    auto [it, inserted] = m_patterns.try_emplace(key, mixed_seed(settings.seed, object_id));
    ObjectPattern &pattern = it->second;
    const double query_z = std::max(0.0, z);
    while (pattern.bands.empty() || pattern.cursor <= query_z + epsilon)
        append_next_band(pattern, settings);
    const auto band = std::lower_bound(pattern.bands.begin(), pattern.bands.end(), query_z,
        [](const ThermalPatternBand &candidate, double value) { return candidate.end_z <= value + epsilon; });
    return band == pattern.bands.end() ? pattern.bands.back().level : band->level;
}

int ThermalPatternGenerator::top_level_for(std::uint64_t object_id, double z, std::size_t group_index,
                                            const ThermalPatternSettings &settings) const
{
    const int cap = std::clamp(settings.top_max_level, 0, std::max(0, settings.max_level));
    const std::uint64_t z_key = static_cast<std::uint64_t>(std::llround(std::max(0.0, z) * 1000.0));
    std::mt19937_64 rng(mixed_seed(settings.seed, object_id, z_key ^ 0x5a17ULL));
    std::vector<double> initial_weights;
    for (int candidate = 0; candidate <= cap; ++candidate)
        initial_weights.push_back(std::pow(std::max(epsilon, settings.darkness_bias), candidate));
    std::discrete_distribution<int> initial(initial_weights.begin(), initial_weights.end());
    int level = initial(rng);
    double trend = 0.0;
    for (std::size_t i = 0; i < group_index; ++i) {
        trend = evolve_trend(rng, trend, settings);
        level = choose_level(rng, level, trend, cap, settings);
    }
    return level;
}

void ThermalPatternGenerator::clear()
{
    m_patterns.clear();
}

unsigned int ThermalPatternGenerator::target_temperature(double base_temperature, int level, double step,
                                                          double filament_ceiling, double machine_ceiling)
{
    const double ceiling = std::max(0.0, std::min(filament_ceiling, machine_ceiling));
    const double target = std::clamp(base_temperature + std::max(0, level) * std::max(0.0, step), 0.0, ceiling);
    return static_cast<unsigned int>(std::lround(target));
}

double ThermalPatternGenerator::predicted_temperature(double actual, double target, double seconds,
                                                       double heat_tau, double cool_tau)
{
    if (seconds <= 0.0)
        return actual;
    const double tau = std::max(epsilon, target >= actual ? heat_tau : cool_tau);
    return target + (actual - target) * std::exp(-seconds / tau);
}

double ThermalPatternGenerator::settle_time(double actual, double target, double tolerance,
                                            double heat_tau, double cool_tau)
{
    const double delta = std::abs(actual - target);
    const double bounded_tolerance = std::max(0.1, tolerance);
    if (delta <= bounded_tolerance)
        return 0.0;
    const double tau = std::max(epsilon, target >= actual ? heat_tau : cool_tau);
    return tau * std::log(delta / bounded_tolerance);
}

double ThermalPatternGenerator::assisted_speed(double original_speed, double segment_seconds, double predicted, double target,
                                                const ThermalPatternSettings &settings)
{
    if (original_speed <= 0.0 || segment_seconds <= 0.0 || target <= predicted + settings.thermal_tolerance)
        return original_speed;
    const double needed_seconds = settle_time(predicted, target, settings.thermal_tolerance,
                                              settings.heat_tau, settings.cool_tau);
    if (needed_seconds <= segment_seconds + epsilon)
        return original_speed;

    const double requested_factor = needed_seconds / segment_seconds;
    const double maximum_factor = std::max(1.0, settings.speed_max_factor);
    const double speed_floor = std::max(0.1, settings.speed_min_mm_s);
    const double floor_factor = original_speed / speed_floor;
    const double factor = std::clamp(std::min(requested_factor, floor_factor), 1.0, maximum_factor);
    return original_speed / factor;
}

} // namespace Slic3r
