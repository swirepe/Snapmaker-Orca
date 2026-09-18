#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "libslic3r/Feature/FuzzySkin/FuzzySkin.hpp"
#include "libslic3r/FuzzySkinCalibration.hpp"
#include "libslic3r/PrintConfig.hpp"

using namespace Slic3r;
using namespace Slic3r::Feature::FuzzySkin;

TEST_CASE("Horizontal fuzzy noise is deterministic and bounded", "[FuzzySurface]")
{
    FuzzySurfaceConfig config;
    config.noise_scale       = 0.7;
    config.noise_octaves     = 3;
    config.noise_persistence = 0.6;

    for (NoiseType type : {NoiseType::Classic, NoiseType::Perlin, NoiseType::Billow, NoiseType::RidgedMulti, NoiseType::Voronoi}) {
        config.noise_type   = type;
        const double first  = fuzzy_surface_noise(Vec2d(3.25, -1.75), 2.4, config);
        const double second = fuzzy_surface_noise(Vec2d(3.25, -1.75), 2.4, config);
        CAPTURE(type, first, second);
        REQUIRE(first == Catch::Approx(second));
        REQUIRE(first >= 0.0);
        REQUIRE(first <= 1.0);
    }
}

TEST_CASE("Horizontal fuzzy paths are subdivided and joined to their boundaries", "[FuzzySurface]")
{
    Polyline polyline;
    polyline.points = {Point(scale_(0.0), scale_(0.0)), Point(scale_(1.0), scale_(0.0))};

    FuzzySurfaceConfig config;
    config.point_distance     = 0.25;
    config.displacement       = 0.2;
    config.connect_boundaries = true;

    const std::vector<FuzzySurfacePoint> points = fuzzy_surface_points(polyline, 1.0, FuzzySurfaceType::Top, config);
    REQUIRE(points.size() == 5);
    REQUIRE(points.front().point == polyline.first_point());
    REQUIRE(points.back().point == polyline.last_point());
    REQUIRE(points.front().z_offset == Catch::Approx(0.0));
    REQUIRE(points.back().z_offset == Catch::Approx(0.0));

    bool has_displaced_interior = false;
    for (size_t index = 0; index < points.size(); ++index) {
        REQUIRE(points[index].z_offset >= 0.0);
        REQUIRE(points[index].z_offset <= config.displacement);
        REQUIRE(points[index].extrusion_multiplier >= 1.0);
        if (index > 0 && index + 1 < points.size() && points[index].z_offset > EPSILON)
            has_displaced_interior = true;
        if (index > 0)
            REQUIRE(unscale<double>((points[index].point - points[index - 1].point).cast<double>().norm()) <=
                    config.point_distance + EPSILON);
    }
    REQUIRE(has_displaced_interior);
}

TEST_CASE("Lower fuzzy paths displace downward and use bridge compensation", "[FuzzySurface]")
{
    Polyline polyline;
    polyline.points = {Point(scale_(0.0), scale_(0.0)), Point(scale_(1.0), scale_(0.0))};

    FuzzySurfaceConfig config;
    config.point_distance                 = 0.2;
    config.displacement                   = 0.15;
    config.connect_boundaries             = false;
    config.bridge_compensation_multiplier = 3.0;

    const auto top   = fuzzy_surface_points(polyline, 0.8, FuzzySurfaceType::Top, config);
    const auto lower = fuzzy_surface_points(polyline, 0.8, FuzzySurfaceType::Lower, config);
    REQUIRE(top.size() == lower.size());
    for (size_t index = 0; index < top.size(); ++index) {
        REQUIRE(lower[index].z_offset == Catch::Approx(-top[index].z_offset));
        REQUIRE(lower[index].z_offset <= 0.0);
        REQUIRE(lower[index].z_offset >= -config.displacement);
        REQUIRE(lower[index].extrusion_multiplier >= top[index].extrusion_multiplier);
        if (index > 0) {
            const double xy_length = unscale<double>((top[index].point - top[index - 1].point).cast<double>().norm());
            const double ratio     = std::hypot(xy_length, top[index].z_offset - top[index - 1].z_offset) / xy_length;
            REQUIRE(top[index].extrusion_multiplier == Catch::Approx(ratio));
            REQUIRE(lower[index].extrusion_multiplier == Catch::Approx(std::pow(ratio, 3.0)));
        }
    }
}

TEST_CASE("Horizontal fuzzy subdivision ignores degenerate source segments", "[FuzzySurface]")
{
    Polyline polyline;
    polyline.points = {Point(scale_(0.0), scale_(0.0)), Point(scale_(0.0), scale_(0.0)), Point(scale_(0.3), scale_(0.0))};
    FuzzySurfaceConfig config;
    config.point_distance = 0.1;
    config.displacement   = 0.1;
    const auto points     = fuzzy_surface_points(polyline, 0.4, FuzzySurfaceType::Top, config);
    REQUIRE(points.size() == 4);
    for (size_t index = 1; index < points.size(); ++index)
        REQUIRE(points[index].point != points[index - 1].point);
}

TEST_CASE("Supported lower displacement preserves the requested support clearance", "[FuzzySurface]")
{
    REQUIRE(fuzzy_surface_displacement(0.4, 0.5, 0.1, FuzzySurfaceType::Lower) == Catch::Approx(0.4));
    REQUIRE(fuzzy_surface_displacement(0.4, 0.3, 0.1, FuzzySurfaceType::Lower) == Catch::Approx(0.2));
    REQUIRE(fuzzy_surface_displacement(0.4, 0.1, 0.1, FuzzySurfaceType::Lower) == Catch::Approx(0.0));
    REQUIRE(fuzzy_surface_displacement(0.4, 0.0, 0.1, FuzzySurfaceType::Top) == Catch::Approx(0.4));
}

TEST_CASE("Horizontal fuzzy settings preserve legacy defaults", "[FuzzySurface][Config]")
{
    const PrintRegionConfig config;
    REQUIRE_FALSE(config.fuzzy_skin_top_surface.value);
    REQUIRE_FALSE(config.fuzzy_skin_lower_surface.value);
    REQUIRE_FALSE(config.fuzzy_skin_top_surface_first_layer.value);
    REQUIRE(config.fuzzy_skin_connect_walls.value);
    REQUIRE(config.fuzzy_skin_compensate_extrusion.value);
    REQUIRE_FALSE(config.fuzzy_skin_ironing.value);
}

TEST_CASE("Fuzzy skin calibration builds the default four by four matrix", "[FuzzySurface][Calibration]")
{
    const FuzzySkinCalibrationConfig config;
    const FuzzySkinCalibrationPlan   plan = build_fuzzy_skin_calibration_plan(config);
    REQUIRE(plan.rows == 4);
    REQUIRE(plan.columns == 4);
    REQUIRE(plan.cells.size() == 16);
    REQUIRE(plan.cells.front().thickness == Catch::Approx(0.1));
    REQUIRE(plan.cells.front().distance == Catch::Approx(0.2));
    REQUIRE(plan.cells.back().thickness == Catch::Approx(0.4));
    REQUIRE(plan.cells.back().distance == Catch::Approx(0.8));
}

TEST_CASE("Fuzzy ironing calibration pairs plain and following passes", "[FuzzySurface][Calibration]")
{
    FuzzySkinCalibrationConfig config;
    config.mode                         = FuzzySkinCalibrationMode::IroningComparison;
    const FuzzySkinCalibrationPlan plan = build_fuzzy_skin_calibration_plan(config);
    REQUIRE(plan.rows == 8);
    REQUIRE(plan.columns == 4);
    REQUIRE(plan.cells.size() == 32);
    REQUIRE_FALSE(plan.cells.front().fuzzy_ironing);
    REQUIRE(plan.cells.back().fuzzy_ironing);
}

TEST_CASE("Fuzzy skin calibration creates printable coupon meshes", "[FuzzySurface][Calibration]")
{
    REQUIRE_FALSE(make_fuzzy_skin_calibration_coupon(20.0, 20.0, 2.0).empty());
    REQUIRE_FALSE(make_fuzzy_skin_calibration_bridge(20.0, 20.0, 8.0, 2.0).empty());
    REQUIRE_FALSE(make_fuzzy_skin_calibration_label("T=0.1 D=0.2", 1.6, 0.35).empty());
}
