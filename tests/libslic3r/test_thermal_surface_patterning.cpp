#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "libslic3r/ThermalSurfacePatterning.hpp"

using namespace Slic3r;

TEST_CASE("Thermal surface patterns are deterministic per object", "[thermal_pattern]")
{
    ThermalPatternSettings settings;
    settings.seed = 12345;

    ThermalPatternGenerator first;
    ThermalPatternGenerator second;
    bool objects_differ = false;
    for (int sample = 0; sample < 100; ++sample) {
        const double z = sample * 0.2;
        REQUIRE(first.level_for(41, z, settings) == second.level_for(41, z, settings));
        objects_differ |= first.level_for(41, z, settings) != first.level_for(42, z, settings);
    }
    REQUIRE(objects_differ);
}

TEST_CASE("Thermal surface levels obey configured limits", "[thermal_pattern]")
{
    ThermalPatternSettings settings;
    settings.seed = 9;
    settings.max_level = 4;
    settings.top_max_level = 2;
    ThermalPatternGenerator generator;

    for (int sample = 0; sample < 100; ++sample) {
        const int wall_level = generator.level_for(7, sample * 0.25, settings);
        const int top_level = generator.top_level_for(7, 2.4, sample, settings);
        REQUIRE(wall_level >= 0);
        REQUIRE(wall_level <= 4);
        REQUIRE(top_level >= 0);
        REQUIRE(top_level <= 2);
        REQUIRE(top_level == generator.top_level_for(7, 2.4, sample, settings));
    }
}

TEST_CASE("Thermal targets never exceed their ceilings", "[thermal_pattern]")
{
    REQUIRE(ThermalPatternGenerator::target_temperature(210., 5, 10., 280., 300.) == 260);
    REQUIRE(ThermalPatternGenerator::target_temperature(210., 12, 10., 280., 300.) == 280);
    REQUIRE(ThermalPatternGenerator::target_temperature(210., 12, 10., 320., 300.) == 300);
    REQUIRE(ThermalPatternGenerator::target_temperature(210., -2, 10., 280., 300.) == 210);
}

TEST_CASE("Thermal response and speed assistance are bounded", "[thermal_pattern]")
{
    const double heated = ThermalPatternGenerator::predicted_temperature(200., 240., 5., 5., 8.);
    REQUIRE(heated == Catch::Approx(240. - 40. / std::exp(1.)).epsilon(1e-6));
    const double cooled = ThermalPatternGenerator::predicted_temperature(240., 200., 8., 5., 8.);
    REQUIRE(cooled == Catch::Approx(200. + 40. / std::exp(1.)).epsilon(1e-6));
    REQUIRE(ThermalPatternGenerator::settle_time(200., 240., 3., 5., 8.) ==
            Catch::Approx(5. * std::log(40. / 3.)).epsilon(1e-6));
    REQUIRE(ThermalPatternGenerator::settle_time(240., 200., 3., 5., 8.) ==
            Catch::Approx(8. * std::log(40. / 3.)).epsilon(1e-6));

    ThermalPatternSettings settings;
    settings.speed_max_factor = 4.;
    settings.speed_min_mm_s = 30.;
    REQUIRE(ThermalPatternGenerator::assisted_speed(100., 1., 200., 250., settings) >= 30.);
    REQUIRE(ThermalPatternGenerator::assisted_speed(100., 1., 248., 250., settings) == 100.);
    REQUIRE(ThermalPatternGenerator::assisted_speed(100., 30., 200., 250., settings) == 100.);
}
