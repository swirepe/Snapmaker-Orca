#include <catch2/catch_test_macros.hpp>

#include "libslic3r/Fill/Fill.hpp"
#include "libslic3r/Format/3mf.hpp"
#include "libslic3r/Model.hpp"
#include "libslic3r/PaneCalibration.hpp"
#include "libslic3r/Preset.hpp"
#include "libslic3r/PrintConfig.hpp"

#include <boost/filesystem/operations.hpp>

#include <array>
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>

using namespace Slic3r;

TEST_CASE("Pane calibration produces inclusive linear levels", "[PaneCalibration]")
{
    const std::vector<double> values = pane_calibration_linear_values(260., 275., 4);
    REQUIRE(values == std::vector<double>{260., 265., 270., 275.});
}

TEST_CASE("Pane calibration rejects degenerate numeric factors", "[PaneCalibration]")
{
    REQUIRE_THROWS_AS(pane_calibration_linear_values(10., 10., 4), std::invalid_argument);
    REQUIRE_THROWS_AS(pane_calibration_linear_values(0., std::numeric_limits<double>::infinity(), 4),
                      std::invalid_argument);

    PaneCalibrationConfig config = default_pane_calibration_config(PaneCalibrationTool::Ironing);
    config.design = PaneCalibrationDesign::Linear;
    for (PaneCalibrationFactorSetting &factor : config.factors) {
        factor.enabled = factor.factor == PaneCalibrationFactor::IroningSpacing;
        if (factor.enabled) {
            factor.minimum = 0.;
            factor.maximum = 0.2;
        }
    }
    REQUIRE_THROWS_AS(build_pane_calibration_plan(config), std::invalid_argument);

    for (PaneCalibrationFactorSetting &factor : config.factors) {
        factor.enabled = factor.factor == PaneCalibrationFactor::IroningSpeed;
        if (factor.enabled) {
            factor.minimum = 0.;
            factor.maximum = 20.;
        }
    }
    REQUIRE_THROWS_AS(build_pane_calibration_plan(config), std::invalid_argument);
}

TEST_CASE("Pane calibration defaults select four factors", "[PaneCalibration]")
{
    for (PaneCalibrationTool tool : {PaneCalibrationTool::ClearFilament, PaneCalibrationTool::Ironing}) {
        const PaneCalibrationConfig config = default_pane_calibration_config(tool);
        const PaneCalibrationPlan   plan   = build_pane_calibration_plan(config);
        REQUIRE(plan.array_name == "L16");
        REQUIRE(plan.rows.size() == 16);
        REQUIRE(plan.rows.front().size() == 4);
    }
}

TEST_CASE("Supported Taguchi arrays are pairwise balanced", "[PaneCalibration]")
{
    const std::array<std::pair<unsigned, size_t>, 7> cases {{{2, 3}, {2, 7}, {2, 15}, {3, 4}, {3, 7}, {3, 13}, {4, 5}}};
    for (const auto &[levels, columns] : cases) {
        const auto [name, array] = pane_calibration_taguchi_array(levels, columns);
        INFO(name);
        REQUIRE(!array.empty());
        for (size_t left = 0; left < columns; ++left)
            for (size_t right = left + 1; right < columns; ++right) {
                std::map<std::pair<unsigned, unsigned>, size_t> counts;
                for (const auto &row : array)
                    ++counts[{row[left], row[right]}];
                REQUIRE(counts.size() == levels * levels);
                const size_t expected = array.size() / (levels * levels);
                for (const auto &[combination, count] : counts)
                    REQUIRE(count == expected);
            }
    }
}

TEST_CASE("Pane labels keep one factor on each baseline", "[PaneCalibration]")
{
    PaneCalibrationRow row {
        {PaneCalibrationFactor::NozzleTemperature, 270., 2},
        {PaneCalibrationFactor::PrintSpeed, 41.7, 2},
        {PaneCalibrationFactor::FlowRatio, 1.04, 2},
        {PaneCalibrationFactor::LayerHeight, 0.3, 2},
    };
    const std::vector<std::string> lines = pane_calibration_label_lines(row, 4, 17);
    REQUIRE(lines == std::vector<std::string>{"T=270", "S=41.7", "FR=1.04", "LH=0.3"});
}

TEST_CASE("Pane placement follows the printer sequential-clearance thresholds", "[PaneCalibration]")
{
    PaneCalibrationConfig config = default_pane_calibration_config(PaneCalibrationTool::ClearFilament);
    config.labels = true;

    const PaneCalibrationPlacementConstraints short_panes = pane_calibration_placement_constraints(
        config, 16, 2.5, 72.5, 27.5, 140.);
    REQUIRE(std::abs(short_panes.object_height - 1.5) < 1e-9);
    REQUIRE(short_panes.effective_gap == 5.);
    REQUIRE_FALSE(short_panes.requires_toolhead_clearance);
    REQUIRE_FALSE(short_panes.one_per_row);

    config.label_relief = 2.;
    const PaneCalibrationPlacementConstraints toolhead_clearance = pane_calibration_placement_constraints(
        config, 16, 2.5, 72.5, 27.5, 140.);
    REQUIRE(toolhead_clearance.requires_toolhead_clearance);
    REQUIRE(toolhead_clearance.effective_gap == 72.5);
    REQUIRE_FALSE(toolhead_clearance.one_per_row);

    config.pane_height = 30.;
    REQUIRE(pane_calibration_placement_constraints(config, 16, 2.5, 72.5, 27.5, 140.).one_per_row);

    config.pane_height = 141.;
    REQUIRE_THROWS_AS(pane_calibration_placement_constraints(config, 16, 2.5, 72.5, 27.5, 140.),
                      std::invalid_argument);
}

TEST_CASE("Pane calibration maps categorical values by level", "[PaneCalibration]")
{
    PaneCalibrationConfig config = default_pane_calibration_config(PaneCalibrationTool::Ironing);
    const PaneCalibrationPlan plan = build_pane_calibration_plan(config);
    for (const PaneCalibrationRow &row : plan.rows) {
        const auto type = std::find_if(row.begin(), row.end(), [](const PaneCalibrationValue &entry) {
            return entry.factor == PaneCalibrationFactor::IroningType;
        });
        REQUIRE(type != row.end());
        REQUIRE(type->value == double(type->level));
        REQUIRE(pane_calibration_format_value(type->factor, type->value, 4) ==
                std::array<std::string, 4>{"OFF", "TOP", "ALT", "ALL"}[type->level]);
    }
}

TEST_CASE("Every-other-layer ironing alternates and always includes the top layer", "[PaneCalibration][Ironing]")
{
    REQUIRE_FALSE(ironing_every_other_layer_selected(0, false));
    REQUIRE(ironing_every_other_layer_selected(1, false));
    REQUIRE_FALSE(ironing_every_other_layer_selected(2, false));
    REQUIRE(ironing_every_other_layer_selected(3, false));
    REQUIRE(ironing_every_other_layer_selected(4, true));
}

TEST_CASE("Categorical factors keep their own level count", "[PaneCalibration]")
{
    PaneCalibrationConfig config = default_pane_calibration_config(PaneCalibrationTool::Ironing);
    config.design = PaneCalibrationDesign::Linear;
    for (PaneCalibrationFactorSetting &factor : config.factors)
        factor.enabled = factor.factor == PaneCalibrationFactor::IroningAngle;
    auto angle = std::find_if(config.factors.begin(), config.factors.end(), [](const PaneCalibrationFactorSetting &factor) {
        return factor.factor == PaneCalibrationFactor::IroningAngle;
    });
    REQUIRE(angle != config.factors.end());
    angle->levels = 2;

    const PaneCalibrationPlan plan = build_pane_calibration_plan(config);
    REQUIRE(plan.rows.size() == 2);
    REQUIRE(plan.rows[0][0].levels == 2);
    REQUIRE(pane_calibration_label_lines(plan.rows[1], 4, 10) == std::vector<std::string>{"IA=90"});
}

TEST_CASE("Pane calibration creates body ears and editable label meshes", "[PaneCalibration]")
{
    PaneCalibrationConfig config = default_pane_calibration_config(PaneCalibrationTool::ClearFilament);
    const size_t plain_facets = make_pane_calibration_body(config, 0.2).facets_count();
    config.mouse_ears = true;
    REQUIRE(make_pane_calibration_body(config, 0.2).facets_count() > plain_facets);
    const TriangleMesh label = make_pane_calibration_label({"T=270", "FR=1.04"}, 2., 0.5, 30., 30.);
    REQUIRE_FALSE(label.empty());
    REQUIRE(std::abs(label.bounding_box().max.z() - 0.5) < 1e-6);
}

TEST_CASE("Regional process overrides have safe defaults and survive project storage", "[PaneCalibration][3mf]")
{
    const std::vector<std::string> override_keys {
        "nozzle_temperature_override",
        "fan_speed_override",
        "wall_fan_speed_override",
        "ironing_fan_speed_override",
        "auxiliary_fan_speed_override",
    };
    for (const std::string &key : override_keys) {
        REQUIRE(print_config_def.get(key) != nullptr);
        REQUIRE(std::find(Preset::print_options().begin(), Preset::print_options().end(), key) != Preset::print_options().end());
    }

    const PrintRegionConfig defaults;
    REQUIRE(defaults.nozzle_temperature_override.value == 0);
    REQUIRE(defaults.fan_speed_override.value == -1);
    REQUIRE(defaults.wall_fan_speed_override.value == -1);
    REQUIRE(defaults.ironing_fan_speed_override.value == -1);
    REQUIRE(defaults.auxiliary_fan_speed_override.value == -1);

    Model        source;
    ModelObject *object = source.add_object();
    ModelVolume *volume = object->add_volume(make_cube(20., 20., 2.));
    volume->config.set_key_value("ironing_type", new ConfigOptionEnum<IroningType>(IroningType::EveryOtherLayer));
    volume->config.set_key_value("ironing_pattern", new ConfigOptionEnum<InfillPattern>(InfillPattern::ipConcentric));
    volume->config.set_key_value("ironing_flow", new ConfigOptionPercent(18.));
    volume->config.set_key_value("ironing_spacing", new ConfigOptionFloat(0.17));
    volume->config.set_key_value("ironing_inset", new ConfigOptionFloat(0.35));
    volume->config.set_key_value("ironing_angle", new ConfigOptionFloat(37.));
    volume->config.set_key_value("ironing_speed", new ConfigOptionFloats{23.});
    volume->config.set_key_value("nozzle_temperature_override", new ConfigOptionInt(275));
    volume->config.set_key_value("fan_speed_override", new ConfigOptionInt(60));
    volume->config.set_key_value("wall_fan_speed_override", new ConfigOptionInt(40));
    volume->config.set_key_value("ironing_fan_speed_override", new ConfigOptionInt(25));
    volume->config.set_key_value("auxiliary_fan_speed_override", new ConfigOptionInt(35));
    object->add_instance();

    const boost::filesystem::path path = boost::filesystem::temp_directory_path() /
                                         boost::filesystem::unique_path("pane-region-config-%%%%-%%%%.3mf");
    REQUIRE(store_3mf(path.string().c_str(), &source, nullptr, false));

    Model                     restored;
    DynamicPrintConfig        config;
    ConfigSubstitutionContext substitutions{ForwardCompatibilitySubstitutionRule::Disable};
    REQUIRE(load_3mf(path.string().c_str(), config, substitutions, &restored, false));
    boost::filesystem::remove(path);

    REQUIRE(restored.objects.size() == 1);
    REQUIRE(restored.objects.front()->volumes.size() == 1);
    const ModelConfig &restored_config = restored.objects.front()->volumes.front()->config;
    REQUIRE(restored_config.get().opt_enum<IroningType>("ironing_type") == IroningType::EveryOtherLayer);
    REQUIRE(restored_config.get().opt_enum<InfillPattern>("ironing_pattern") == InfillPattern::ipConcentric);
    REQUIRE(restored_config.opt_float("ironing_flow") == 18.);
    REQUIRE(restored_config.opt_float("ironing_spacing") == 0.17);
    REQUIRE(restored_config.opt_float("ironing_inset") == 0.35);
    REQUIRE(restored_config.opt_float("ironing_angle") == 37.);
    REQUIRE(restored_config.get().opt_float("ironing_speed", 0) == 23.);
    REQUIRE(restored_config.opt_int("nozzle_temperature_override") == 275);
    REQUIRE(restored_config.opt_int("fan_speed_override") == 60);
    REQUIRE(restored_config.opt_int("wall_fan_speed_override") == 40);
    REQUIRE(restored_config.opt_int("ironing_fan_speed_override") == 25);
    REQUIRE(restored_config.opt_int("auxiliary_fan_speed_override") == 35);
}
