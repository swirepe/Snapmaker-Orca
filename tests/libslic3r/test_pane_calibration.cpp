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

TEST_CASE("Transparent pane defaults respect small nozzles", "[PaneCalibration]")
{
    PaneCalibrationConfig config = default_pane_calibration_config(PaneCalibrationTool::ClearFilament, 0.2);
    const auto layer_height = std::find_if(config.factors.begin(), config.factors.end(), [](const PaneCalibrationFactorSetting& factor) {
        return factor.factor == PaneCalibrationFactor::LayerHeight;
    });
    REQUIRE(layer_height != config.factors.end());
    REQUIRE(layer_height->minimum == 0.1);
    REQUIRE(layer_height->maximum == 0.2);
    REQUIRE_NOTHROW(validate_pane_calibration_machine_limits(config, 0.2));

    layer_height->maximum = 0.21;
    REQUIRE_THROWS_AS(validate_pane_calibration_machine_limits(config, 0.2), std::invalid_argument);
    REQUIRE_THROWS_AS(validate_pane_calibration_machine_limits(config, 0.), std::invalid_argument);
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

TEST_CASE("Pane process controls and label scheduling survive project storage", "[PaneCalibration][3mf]")
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
    object->config.set_key_value("layer_height", new ConfigOptionFloat(0.24));
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
    volume->config.set_key_value("outer_wall_speed", new ConfigOptionFloats{31.});
    volume->config.set_key_value("inner_wall_speed", new ConfigOptionFloats{42.});
    volume->config.set_key_value("internal_solid_infill_speed", new ConfigOptionFloats{53.});
    volume->config.set_key_value("top_surface_speed", new ConfigOptionFloats{64.});
    volume->config.set_key_value("print_flow_ratio", new ConfigOptionFloat(1.07));
    volume->config.set_key_value("outer_wall_line_width", new ConfigOptionFloatOrPercent(0.41, false));
    volume->config.set_key_value("inner_wall_line_width", new ConfigOptionFloatOrPercent(0.42, false));
    volume->config.set_key_value("internal_solid_infill_line_width", new ConfigOptionFloatOrPercent(0.43, false));
    volume->config.set_key_value("top_surface_line_width", new ConfigOptionFloatOrPercent(0.44, false));
    volume->config.set_key_value("extruder", new ConfigOptionInt(1));

    ModelVolume* label = object->add_volume(make_cube(10., 10., 0.5), ModelVolumeType::MODEL_PART, false);
    label->set_offset(Vec3d(0., 0., 2.));
    label->config.set_key_value("extruder", new ConfigOptionInt(2));
    label->config.set_key_value("pane_calibration_label", new ConfigOptionBool(true));
    object->add_instance();

    const PaneCalibrationLabelSchedule source_schedule = pane_calibration_label_schedule(*object, Transform3d::Identity());
    REQUIRE(source_schedule.status == PaneCalibrationLabelScheduleStatus::Ready);
    REQUIRE(source_schedule.label_start_z == 2.);
    REQUIRE(source_schedule.label_extruder == 1);

    label->set_offset(Vec3d(0., 0., 1.9));
    REQUIRE(pane_calibration_label_schedule(*object, Transform3d::Identity()).status == PaneCalibrationLabelScheduleStatus::GeometryOverlap);
    label->set_offset(Vec3d(0., 0., 2.));

    const boost::filesystem::path path = boost::filesystem::temp_directory_path() /
                                         boost::filesystem::unique_path("pane-region-config-%%%%-%%%%.3mf");
    REQUIRE(store_3mf(path.string().c_str(), &source, nullptr, false));

    Model                     restored;
    DynamicPrintConfig        config;
    ConfigSubstitutionContext substitutions{ForwardCompatibilitySubstitutionRule::Disable};
    REQUIRE(load_3mf(path.string().c_str(), config, substitutions, &restored, false));
    boost::filesystem::remove(path);

    REQUIRE(restored.objects.size() == 1);
    REQUIRE(restored.objects.front()->volumes.size() == 2);
    REQUIRE(restored.objects.front()->config.opt_float("layer_height") == 0.24);
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
    REQUIRE(restored_config.get().opt_float("outer_wall_speed", 0) == 31.);
    REQUIRE(restored_config.get().opt_float("inner_wall_speed", 0) == 42.);
    REQUIRE(restored_config.get().opt_float("internal_solid_infill_speed", 0) == 53.);
    REQUIRE(restored_config.get().opt_float("top_surface_speed", 0) == 64.);
    REQUIRE(restored_config.opt_float("print_flow_ratio") == 1.07);
    REQUIRE(restored_config.opt_float("outer_wall_line_width") == 0.41);
    REQUIRE(restored_config.opt_float("inner_wall_line_width") == 0.42);
    REQUIRE(restored_config.opt_float("internal_solid_infill_line_width") == 0.43);
    REQUIRE(restored_config.opt_float("top_surface_line_width") == 0.44);

    const ModelVolume*      restored_label = restored.objects.front()->volumes.back();
    const ConfigOptionBool* label_marker   = restored_label->config.get().option<ConfigOptionBool>("pane_calibration_label");
    REQUIRE(label_marker != nullptr);
    REQUIRE(label_marker->value);
    const PaneCalibrationLabelSchedule restored_schedule = pane_calibration_label_schedule(*restored.objects.front(),
                                                                                           Transform3d::Identity());
    REQUIRE(restored_schedule.status == PaneCalibrationLabelScheduleStatus::Ready);
    REQUIRE(restored_schedule.label_start_z == 2.);
    REQUIRE(restored_schedule.label_extruder == 1);
}

TEST_CASE("Pane label phases follow transforms and reject conflicting label tools", "[PaneCalibration]")
{
    Model        model;
    ModelObject* object = model.add_object();
    object->config.set_key_value("extruder", new ConfigOptionInt(1));
    object->add_volume(make_cube(20., 20., 2.));
    ModelVolume* label = object->add_volume(make_cube(10., 10., 0.5), ModelVolumeType::MODEL_PART, false);
    label->set_offset(Vec3d(0., 0., 2.));
    label->config.set_key_value("pane_calibration_label", new ConfigOptionBool(true));
    label->config.set_key_value("extruder", new ConfigOptionInt(2));

    Transform3d transform = Transform3d::Identity();
    transform.scale(Vec3d(1., 1., 2.));
    const auto scaled = pane_calibration_label_schedule(*object, transform);
    REQUIRE(scaled.status == PaneCalibrationLabelScheduleStatus::Ready);
    REQUIRE(scaled.label_start_z == 4.);

    SECTION("a shared inherited extruder needs no second phase")
    {
        label->config.set_key_value("extruder", new ConfigOptionInt(0));
        REQUIRE(pane_calibration_label_schedule(*object, transform).status == PaneCalibrationLabelScheduleStatus::None);
    }
    SECTION("labels with different tools cannot share a label phase")
    {
        ModelVolume* other_label = object->add_volume(make_cube(2., 2., 0.5), ModelVolumeType::MODEL_PART, false);
        other_label->set_offset(Vec3d(0., 0., 2.));
        other_label->config.set_key_value("pane_calibration_label", new ConfigOptionBool(true));
        other_label->config.set_key_value("extruder", new ConfigOptionInt(3));
        REQUIRE(pane_calibration_label_schedule(*object, transform).status == PaneCalibrationLabelScheduleStatus::MultipleLabelExtruders);
    }
}
