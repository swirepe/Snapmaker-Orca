#ifndef slic3r_ThermalSurfacePatterning_hpp_
#define slic3r_ThermalSurfacePatterning_hpp_

#include <cstddef>
#include <cstdint>
#include <random>
#include <unordered_map>
#include <vector>

namespace Slic3r {

struct ThermalPatternSettings
{
    int    seed                  {1};
    int    max_level             {6};
    int    top_max_level         {3};
    double band_median           {1.2};
    double band_sigma            {0.60};
    double band_min              {0.36};
    double band_max              {6.0};
    double dark_band_narrowing   {0.08};
    double stay_weight           {10.0};
    double adjacent_weight       {7.0};
    double two_away_weight       {2.0};
    double far_weight            {0.05};
    double darkness_bias         {0.75};
    double trend_persistence     {0.88};
    double trend_strength        {0.65};
    double accent_chance         {0.08};
    int    accent_boost          {2};
    double accent_min            {0.24};
    double accent_max            {0.48};
    double heat_tau              {5.0};
    double cool_tau              {7.5};
    double thermal_tolerance     {3.0};
    double speed_max_factor      {4.0};
    double speed_min_mm_s        {30.0};

    std::size_t hash() const;
};

struct ThermalPatternBand
{
    double start_z {0.0};
    double end_z   {0.0};
    int    level   {0};
    bool   accent  {false};
};

class ThermalPatternGenerator
{
public:
    int level_for(std::uint64_t object_id, double z, const ThermalPatternSettings &settings);
    int top_level_for(std::uint64_t object_id, double z, std::size_t group_index,
                      const ThermalPatternSettings &settings) const;
    void clear();

    static unsigned int target_temperature(double base_temperature, int level, double step,
                                           double filament_ceiling, double machine_ceiling);
    static double predicted_temperature(double actual, double target, double seconds,
                                        double heat_tau, double cool_tau);
    static double settle_time(double actual, double target, double tolerance,
                              double heat_tau, double cool_tau);
    static double assisted_speed(double original_speed, double segment_seconds, double predicted, double target,
                                 const ThermalPatternSettings &settings);

private:
    struct CacheKey
    {
        std::uint64_t object_id {0};
        std::size_t settings_hash {0};

        bool operator==(const CacheKey &rhs) const
        {
            return object_id == rhs.object_id && settings_hash == rhs.settings_hash;
        }
    };

    struct CacheKeyHash
    {
        std::size_t operator()(const CacheKey &key) const;
    };

    struct ObjectPattern
    {
        explicit ObjectPattern(std::uint64_t seed) : rng(seed) {}

        std::mt19937_64 rng;
        std::vector<ThermalPatternBand> bands;
        double cursor {0.0};
        double trend {0.0};
        int level {-1};
    };

    std::unordered_map<CacheKey, ObjectPattern, CacheKeyHash> m_patterns;

    static std::uint64_t mixed_seed(int seed, std::uint64_t object_id, std::uint64_t salt = 0);
    static int choose_level(std::mt19937_64 &rng, int current, double trend,
                            int cap, const ThermalPatternSettings &settings);
    static double evolve_trend(std::mt19937_64 &rng, double trend,
                               const ThermalPatternSettings &settings);
    static double band_thickness(std::mt19937_64 &rng, int level,
                                 const ThermalPatternSettings &settings);
    static void append_next_band(ObjectPattern &pattern, const ThermalPatternSettings &settings);
};

} // namespace Slic3r

#endif
