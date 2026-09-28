#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <string>

#include "libslic3r/Feature/FuzzySkin/FuzzySkin.hpp"
#include "libslic3r/FuzzySkinCalibration.hpp"
#include "libslic3r/Preset.hpp"
#include "libslic3r/Model.hpp"
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
    REQUIRE(fuzzy_surface_path_length(points) > unscale<double>(polyline.length()));
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

TEST_CASE("Fuzzy surface extrusion compensation stays finite and bounded", "[FuzzySurface]")
{
    Polyline polyline;
    polyline.points = {Point(scale_(0.0), scale_(0.0)), Point(scale_(1.0), scale_(0.0))};

    FuzzySurfaceConfig config;
    config.point_distance                 = 0.01;
    config.displacement                   = 1.0;
    config.connect_boundaries             = false;
    config.compensate_extrusion            = true;
    config.bridge_compensation_multiplier = 10.0;

    const auto points = fuzzy_surface_points(polyline, 0.8, FuzzySurfaceType::Lower, config);
    REQUIRE_FALSE(points.empty());
    for (const FuzzySurfacePoint& point : points) {
        REQUIRE(std::isfinite(point.extrusion_multiplier));
        REQUIRE(point.extrusion_multiplier >= 1.0);
        REQUIRE(point.extrusion_multiplier <= 5.0);
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

TEST_CASE("Bed-facing displacement is capped below the next layer", "[FuzzySurface]")
{
    REQUIRE(fuzzy_bed_surface_displacement(0.5, 0.2) == Catch::Approx(0.05));
    REQUIRE(fuzzy_bed_surface_displacement(0.01, 0.2) == Catch::Approx(0.01));
    REQUIRE(fuzzy_bed_surface_displacement(0.5, -1.0) == Catch::Approx(0.0));
}

TEST_CASE("Bed-facing fuzzy classification is explicit and first-layer only", "[FuzzySurface]")
{
    REQUIRE(is_bed_fuzzy_surface(erBottomSurface, true, true));
    REQUIRE_FALSE(is_bed_fuzzy_surface(erBottomSurface, false, true));
    REQUIRE_FALSE(is_bed_fuzzy_surface(erBottomSurface, true, false));
    REQUIRE_FALSE(is_bed_fuzzy_surface(erTopSolidInfill, true, true));
    REQUIRE_FALSE(is_bed_fuzzy_surface(erBridgeInfill, true, true));
}

TEST_CASE("Horizontal fuzzy settings preserve legacy defaults", "[FuzzySurface][Config]")
{
    const PrintRegionConfig config;
    REQUIRE_FALSE(config.fuzzy_skin_top_surface.value);
    REQUIRE_FALSE(config.fuzzy_skin_lower_surface.value);
    REQUIRE_FALSE(config.fuzzy_skin_top_surface_first_layer.value);
    REQUIRE_FALSE(config.fuzzy_skin_bed_surface.value);
    REQUIRE(config.fuzzy_skin_connect_walls.value);
    REQUIRE(config.fuzzy_skin_compensate_extrusion.value);
    REQUIRE_FALSE(config.fuzzy_skin_ironing.value);
}

TEST_CASE("Every fuzzy skin quick-setting key is unique and registered", "[FuzzySurface][Config]")
{
    const PrintRegionConfig config;
    const auto&             preset_keys = Preset::print_options();
    std::set<std::string>   unique_keys;
    for (const char* key : config_option_keys) {
        CAPTURE(key);
        REQUIRE(unique_keys.emplace(key).second);
        REQUIRE(config.option(key) != nullptr);
        REQUIRE(std::find(preset_keys.begin(), preset_keys.end(), key) != preset_keys.end());
    }
    REQUIRE(unique_keys.size() == config_option_keys.size());
}

TEST_CASE("Fuzzy skin calibration builds the default four by four matrix", "[FuzzySurface][Calibration]")
{
    const FuzzySkinCalibrationConfig config;
    const FuzzySkinCalibrationPlan   plan = build_fuzzy_skin_calibration_plan(config);
    REQUIRE(plan.rows == 4);
    REQUIRE(plan.columns == 4);
    REQUIRE(plan.cells.size() == 16);
    REQUIRE_FALSE(plan.shared_object);
    REQUIRE(plan.cells.front().thickness == Catch::Approx(0.05));
    REQUIRE(plan.cells.front().distance == Catch::Approx(0.2));
    REQUIRE(plan.cells.back().thickness == Catch::Approx(0.5));
    REQUIRE(plan.cells.back().distance == Catch::Approx(2.0));
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
    const TriangleMesh bed_patch = make_fuzzy_skin_calibration_bed_patch(5.0, 0.2);
    REQUIRE_FALSE(bed_patch.empty());
    const BoundingBoxf3 patch_bounds = bed_patch.bounding_box();
    REQUIRE(patch_bounds.min.x() == Catch::Approx(-5.0));
    REQUIRE(patch_bounds.max.x() == Catch::Approx(5.0));
    REQUIRE(patch_bounds.min.z() == Catch::Approx(0.0));
    REQUIRE(patch_bounds.max.z() == Catch::Approx(0.2));
    REQUIRE_FALSE(make_fuzzy_skin_calibration_label("T=0.1 D=0.2", 1.6, 0.35).empty());
}

TEST_CASE("Every calibration card gets a bed-contacting circular patch", "[FuzzySurface][Calibration]")
{
    FuzzySkinCalibrationConfig config;
    const auto regular = fuzzy_skin_calibration_bed_patch(config);
    REQUIRE(regular.center_x == Catch::Approx(0.0));
    REQUIRE(regular.center_y == Catch::Approx(0.0));
    REQUIRE(regular.radius == Catch::Approx(6.25));

    config.mode = FuzzySkinCalibrationMode::SupportedUnderside;
    config.layout = FuzzySkinCalibrationLayout::BreakawayCoupons;
    const auto supported = fuzzy_skin_calibration_bed_patch(config);
    const double leg_width = std::min(3.0, 0.2 * config.coupon_width);
    const double leg_min_x = -0.5 * config.coupon_width;
    const double leg_max_x = leg_min_x + leg_width;
    REQUIRE(supported.radius > 0.0);
    REQUIRE(supported.center_x - supported.radius >= leg_min_x);
    REQUIRE(supported.center_x + supported.radius <= leg_max_x);

    const FuzzySkinCalibrationPlan plan = build_fuzzy_skin_calibration_plan(config);
    REQUIRE(plan.shared_object);
}

TEST_CASE("Calibration maps bed texture values proportionally into the safe cap", "[FuzzySurface][Calibration]")
{
    const FuzzySkinCalibrationConfig config;
    const double first = fuzzy_skin_calibration_bed_thickness(config, 0.05, 0.2);
    const double last  = fuzzy_skin_calibration_bed_thickness(config, 0.50, 0.2);
    REQUIRE(first == Catch::Approx(0.005));
    REQUIRE(last == Catch::Approx(0.05));
    REQUIRE(first < last);
}

TEST_CASE("Fuzzy calibration always uses by-layer printing", "[FuzzySurface][Calibration]")
{
    DynamicPrintConfig config = DynamicPrintConfig::full_print_config();
    config.set_key_value("print_sequence", new ConfigOptionEnum<PrintSequence>(PrintSequence::ByObject));

    apply_fuzzy_skin_calibration_print_config(config);

    REQUIRE(config.opt_enum<PrintSequence>("print_sequence") == PrintSequence::ByLayer);
}

TEST_CASE("Fuzzy skin calibration labels preserve hundredths", "[FuzzySurface][Calibration]")
{
    REQUIRE(fuzzy_skin_calibration_value_label(0.05) == "0.05");
    REQUIRE(fuzzy_skin_calibration_value_label(0.005) == "0.005");
    REQUIRE(fuzzy_skin_calibration_value_label(0.15) == "0.15");
    REQUIRE(fuzzy_skin_calibration_value_label(1.0) == "1");
    REQUIRE(fuzzy_skin_calibration_value_label(5.0) == "5");
}

TEST_CASE("Fuzzy skin calibration rejects values outside print option bounds", "[FuzzySurface][Calibration]")
{
    FuzzySkinCalibrationConfig config;
    config.thickness = {0.0005, 0.001, 0.0005};
    REQUIRE_THROWS_AS(build_fuzzy_skin_calibration_plan(config), std::invalid_argument);

    config.thickness = {0.5, 1.5, 0.5};
    REQUIRE_THROWS_AS(build_fuzzy_skin_calibration_plan(config), std::invalid_argument);

    config.thickness = {0.1, 0.4, 0.1};
    config.distance  = {0.005, 0.01, 0.005};
    REQUIRE_THROWS_AS(build_fuzzy_skin_calibration_plan(config), std::invalid_argument);

    config.distance  = {1.0, 6.0, 1.0};
    REQUIRE_THROWS_AS(build_fuzzy_skin_calibration_plan(config), std::invalid_argument);
}

TEST_CASE("Fuzzy skin calibration rejects step counts before integer conversion", "[FuzzySurface][Calibration]")
{
    const FuzzySkinCalibrationRange range{0.1, 0.2, std::numeric_limits<double>::denorm_min()};
    REQUIRE_THROWS_AS(fuzzy_skin_calibration_values(range), std::invalid_argument);
}

TEST_CASE("Bed and supported fuzzy paths regularly return to nominal bonding height", "[FuzzySurface]")
{
    Polyline polyline;
    polyline.points = {Point::new_scale(0.0, 0.0), Point::new_scale(0.7, 0.0), Point::new_scale(0.7, 4.1)};
    FuzzySurfaceConfig config;
    config.point_distance     = 0.31;
    config.anchor_distance    = 1.0;
    config.displacement       = 0.05;
    config.connect_boundaries = false;
    for (const FuzzySurfaceType type : {FuzzySurfaceType::Bed, FuzzySurfaceType::Lower}) {
        const auto points = fuzzy_surface_points(polyline, 0.2, type, config);
        REQUIRE(points.front().z_offset == 0.0);
        REQUIRE(points.back().z_offset == 0.0);
        for (double distance : {1.0, 2.0, 3.0, 4.0}) {
            const Point anchor = Point::new_scale(0.7, distance - 0.7);
            const auto  found  = std::find_if(points.begin(), points.end(), [&anchor](const FuzzySurfacePoint& point) {
                return (point.point - anchor).cast<double>().norm() <= 2.0;
            });
            REQUIRE(found != points.end());
            REQUIRE(found->z_offset == 0.0);
        }
        REQUIRE(
            std::any_of(points.begin(), points.end(), [](const FuzzySurfacePoint& point) { return std::abs(point.z_offset) > EPSILON; }));
    }
}

TEST_CASE("Four-treatment cubes cover complete walls and four equal top sectors", "[FuzzySurface][Calibration]")
{
    FuzzySkinCalibrationConfig config;
    config.mode     = FuzzySkinCalibrationMode::CubeSingle;
    config.labels   = false;
    const auto plan = build_fuzzy_skin_calibration_plan(config);
    REQUIRE(plan.cells.size() == 1);
    REQUIRE(plan.rows == 1);
    REQUIRE(plan.columns == 1);
    Model        model;
    ModelObject* object = model.add_object();
    populate_fuzzy_skin_calibration_cube(*object, config, plan.cells.front());
    REQUIRE(object->volumes.size() == 4);
    double total_volume = 0.0;
    for (size_t side = 0; side < 4; ++side) {
        const ModelVolume& volume  = *object->volumes[side];
        const bool         fuzzy   = side == 1 || side == 3;
        const bool         ironing = side >= 2;
        REQUIRE(its_volume(volume.mesh().its) == Catch::Approx(3906.25));
        total_volume += its_volume(volume.mesh().its);
        REQUIRE(volume.config.get().opt_enum<FuzzySkinType>("fuzzy_skin") == (fuzzy ? FuzzySkinType::External : FuzzySkinType::None));
        REQUIRE(volume.config.get().opt_bool("fuzzy_skin_top_surface") == fuzzy);
        REQUIRE(volume.config.get().opt_bool("fuzzy_skin_ironing") == (fuzzy && ironing));
        REQUIRE(volume.config.get().opt_enum<IroningType>("ironing_type") == (ironing ? IroningType::AllSolid : IroningType::NoIroning));
    }
    REQUIRE(total_volume == Catch::Approx(15625.0));
}

TEST_CASE("Cube series and L9 arrays balance thickness and point distance", "[FuzzySurface][Calibration]")
{
    FuzzySkinCalibrationConfig config;
    config.mode = FuzzySkinCalibrationMode::CubeSeries;
    REQUIRE(build_fuzzy_skin_calibration_plan(config).cells.size() == 16);
    config.mode     = FuzzySkinCalibrationMode::CubeOrthogonal;
    const auto plan = build_fuzzy_skin_calibration_plan(config);
    REQUIRE(plan.cells.size() == 9);
    std::set<std::pair<double, double>> pairs;
    std::map<double, size_t>            thickness_counts, distance_counts;
    for (const auto& cell : plan.cells) {
        REQUIRE(pairs.emplace(cell.thickness, cell.distance).second);
        ++thickness_counts[cell.thickness];
        ++distance_counts[cell.distance];
    }
    REQUIRE(thickness_counts.size() == 3);
    REQUIRE(distance_counts.size() == 3);
    for (const auto& count : thickness_counts)
        REQUIRE(count.second == 3);
    for (const auto& count : distance_counts)
        REQUIRE(count.second == 3);
    config.distance.maximum = config.distance.minimum;
    REQUIRE_THROWS(build_fuzzy_skin_calibration_plan(config));
}

TEST_CASE("Card label pedestals raise the glyphs above texture with a continuous smooth pad", "[FuzzySurface][Calibration]")
{
    const auto flat   = make_fuzzy_skin_calibration_label("T=0.5", 1.6, 0.35);
    const auto raised = make_fuzzy_skin_calibration_label("T=0.5", 1.6, 0.35, 0.9);
    REQUIRE(flat.bounding_box().max.z() == Catch::Approx(0.35));
    REQUIRE(raised.bounding_box().min.z() == Catch::Approx(0.0));
    REQUIRE(raised.bounding_box().max.z() == Catch::Approx(1.25));
    REQUIRE(raised.bounding_box().size().x() > flat.bounding_box().size().x());
    REQUIRE(raised.bounding_box().size().y() > flat.bounding_box().size().y());
    REQUIRE(its_volume(raised.its) > its_volume(flat.its));
    REQUIRE_THROWS(make_fuzzy_skin_calibration_label("T=0.5", 1.6, 0.35, -1.0));
    REQUIRE_FALSE(FuzzySkinCalibrationConfig().label_pedestal);
}

TEST_CASE("Cube labels stay smooth with card pedestals enabled or disabled", "[FuzzySurface][Calibration]")
{
    for (bool pedestal : {false, true}) {
        FuzzySkinCalibrationConfig config;
        config.mode           = FuzzySkinCalibrationMode::CubeSingle;
        config.label_pedestal = pedestal;
        Model        model;
        ModelObject* object = model.add_object();
        populate_fuzzy_skin_calibration_cube(*object, config, build_fuzzy_skin_calibration_plan(config).cells.front());
        REQUIRE(object->volumes.size() == 8);
        size_t labels = 0;
        for (const auto* volume : object->volumes) {
            if (volume->name.find("Wall label ") != 0)
                continue;
            ++labels;
            REQUIRE(volume->config.get().opt_enum<FuzzySkinType>("fuzzy_skin") == FuzzySkinType::None);
            REQUIRE_FALSE(volume->config.get().opt_bool("fuzzy_skin_top_surface"));
            REQUIRE_FALSE(volume->config.get().opt_bool("fuzzy_skin_ironing"));
            REQUIRE(volume->config.get().opt_enum<IroningType>("ironing_type") == IroningType::NoIroning);
        }
        REQUIRE(labels == 4);
    }
}

TEST_CASE("Readable calibration labels wrap inside the coupon footprint", "[FuzzySurface][Calibration]")
{
    for (double pad : {0.0, 0.9}) {
        const TriangleMesh label = make_fuzzy_skin_calibration_fitted_label("T=0.05 D=0.2 I=1", 23.0, 23.0, 0.6, pad);
        REQUIRE(label.bounding_box().size().x() <= 23.0);
        REQUIRE(label.bounding_box().size().y() <= 23.0);
        REQUIRE(label.bounding_box().size().y() > 3.15);
        REQUIRE(label.bounding_box().max.z() == Catch::Approx(pad + 0.6));
    }
    REQUIRE_THROWS_AS(make_fuzzy_skin_calibration_fitted_label("T=0.05 D=0.2", 8.0, 8.0, 0.6), std::invalid_argument);
    REQUIRE_THROWS_AS(make_fuzzy_skin_calibration_fitted_label("T=0.05", 23.0, 23.0, 0.6, 0.0, 0.2), std::invalid_argument);
    const auto coarse = make_fuzzy_skin_calibration_fitted_label("T=0.5", 23.0, 23.0, 0.6, 0.0, 0.66);
    REQUIRE(coarse.bounding_box().size().y() >= 4.62);
}

TEST_CASE("Calibration grids separate real labeled cubes and every coupon mode", "[FuzzySurface][Calibration]")
{
    for (auto mode : {FuzzySkinCalibrationMode::TextureMatrix, FuzzySkinCalibrationMode::IroningComparison,
                      FuzzySkinCalibrationMode::SupportedUnderside, FuzzySkinCalibrationMode::CubeSingle,
                      FuzzySkinCalibrationMode::CubeSeries, FuzzySkinCalibrationMode::CubeOrthogonal}) {
        for (auto layout : {FuzzySkinCalibrationLayout::ConnectedPanel, FuzzySkinCalibrationLayout::BreakawayCoupons}) {
            if (mode == FuzzySkinCalibrationMode::SupportedUnderside && layout == FuzzySkinCalibrationLayout::ConnectedPanel)
                continue;
            FuzzySkinCalibrationConfig config;
            config.mode                      = mode;
            config.layout                    = layout;
            config.coupon_depth              = 20.0; // Cubes deliberately use width in both axes.
            const auto                 plan  = build_fuzzy_skin_calibration_plan(config);
            const auto                 grid  = build_fuzzy_skin_calibration_grid(config, plan);
            const bool                 cube  = is_fuzzy_skin_calibration_cube(mode);
            const double               depth = cube ? config.coupon_width : config.coupon_depth;
            std::vector<BoundingBoxf3> bounds;
            for (const auto& cell : plan.cells) {
                const Vec2d   center = grid.cell_center(cell);
                BoundingBoxf3 box;
                if (cube) {
                    Model        model;
                    ModelObject* object = model.add_object();
                    populate_fuzzy_skin_calibration_cube(*object, config, cell);
                    for (const auto* volume : object->volumes)
                        box.merge(volume->mesh().bounding_box());
                } else {
                    box = make_fuzzy_skin_calibration_coupon(config.coupon_width, depth, config.coupon_height).bounding_box();
                }
                box.translate(Vec3d(center.x(), center.y(), 0.0));
                CAPTURE(int(mode), int(layout), cell.row, cell.column);
                REQUIRE(box.min.x() >= -0.5 * grid.width - 1e-5);
                REQUIRE(box.max.x() <= 0.5 * grid.width + 1e-5);
                REQUIRE(box.min.y() >= -0.5 * grid.depth - 1e-5);
                REQUIRE(box.max.y() <= 0.5 * grid.depth + 1e-5);
                for (const auto& other : bounds) {
                    const double gap_x = std::max(box.min.x() - other.max.x(), other.min.x() - box.max.x());
                    const double gap_y = std::max(box.min.y() - other.max.y(), other.min.y() - box.max.y());
                    REQUIRE(std::max(gap_x, gap_y) >= config.gap - 1e-5);
                }
                bounds.push_back(box);
                if (cell.column + 1 < plan.columns) {
                    auto next = cell;
                    ++next.column;
                    REQUIRE(grid.cell_center(next).x() - center.x() == Catch::Approx(grid.pitch_x));
                    REQUIRE(grid.pitch_x - config.coupon_width - 2.0 * grid.edge_clearance == Catch::Approx(config.gap));
                }
            }
            REQUIRE(grid.pitch_y - depth - 2.0 * grid.edge_clearance == Catch::Approx(config.gap));
        }
    }
}
