#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "libslic3r/PrintConfig.hpp"
#include "libslic3r/GCode/GCodeProcessor.hpp"
#include "libslic3r/GCodeWriter.hpp"
#include "libslic3r/Model.hpp"
#include "libslic3r/Print.hpp"
#include <boost/filesystem.hpp>
#include <boost/scope_exit.hpp>
#include <fstream>
#include <set>
#include "libslic3r/Preset.hpp"
#include "libslic3r/ThermalSurfacePatterning.hpp"

using namespace Slic3r;

TEST_CASE("Thermal calibration rejects non-finite and unsafe dimensions", "[thermal_pattern]")
{
    REQUIRE(thermal_pattern_calibration_values_valid(210.0, 10.0, 6, 5.0));
    REQUIRE_FALSE(thermal_pattern_calibration_values_valid(0.0, 10.0, 6, 5.0));
    REQUIRE_FALSE(thermal_pattern_calibration_values_valid(std::numeric_limits<double>::quiet_NaN(), 10.0, 6, 5.0));
    REQUIRE_FALSE(thermal_pattern_calibration_values_valid(210.0, std::numeric_limits<double>::infinity(), 6, 5.0));
    REQUIRE_FALSE(thermal_pattern_calibration_values_valid(210.0, 10.0, 0, 5.0));
    REQUIRE_FALSE(thermal_pattern_calibration_values_valid(210.0, 10.0, 21, 5.0));
    REQUIRE_FALSE(thermal_pattern_calibration_values_valid(210.0, 10.0, 6, std::numeric_limits<double>::quiet_NaN()));
    REQUIRE_FALSE(thermal_pattern_calibration_values_valid(210.0, 10.0, 6, 0.39));
    REQUIRE_FALSE(thermal_pattern_calibration_values_valid(
        std::numeric_limits<double>::max(), std::numeric_limits<double>::max(), 20, 5.0));
}

TEST_CASE("Thermal calibration updates one flow variant without clobbering its neighbors", "[thermal_pattern][config]")
{
    DynamicPrintConfig config = DynamicPrintConfig::full_print_config();
    config.set_key_value("thermal_pattern_enabled", new ConfigOptionBools{false, false, true});
    config.set_key_value("thermal_pattern_temperature_step", new ConfigOptionFloats{5.0, 10.0, 15.0});
    config.set_key_value("thermal_pattern_max_temperature", new ConfigOptionInts{240, 260, 280});

    apply_thermal_pattern_calibration(config, 1, 12.5, 295);

    const auto* enabled = config.option<ConfigOptionBools>("thermal_pattern_enabled");
    const auto* step    = config.option<ConfigOptionFloats>("thermal_pattern_temperature_step");
    const auto* ceiling = config.option<ConfigOptionInts>("thermal_pattern_max_temperature");
    REQUIRE(enabled->values.size() == 3);
    REQUIRE(step->values == std::vector<double>{5.0, 12.5, 15.0});
    REQUIRE(ceiling->values == std::vector<int>{240, 295, 280});
    REQUIRE_FALSE(enabled->get_at(0));
    REQUIRE(enabled->get_at(1));
    REQUIRE(enabled->get_at(2));
}

TEST_CASE("Thermal surface pattern defaults pass command-line validation", "[thermal_pattern]")
{
    DynamicPrintConfig config = DynamicPrintConfig::full_print_config();
    const auto errors = config.validate(true);

    REQUIRE(errors.find("thermal_pattern_trend_persistence") == errors.end());
}

TEST_CASE("Every thermal surface option belongs to a persisted preset domain", "[thermal_pattern][config]")
{
    const std::vector<std::string> process_keys{"thermal_pattern_mode",
                                                "thermal_pattern_preset",
                                                "thermal_pattern_seed",
                                                "thermal_pattern_outer_walls",
                                                "thermal_pattern_top_surfaces",
                                                "thermal_pattern_max_level",
                                                "thermal_pattern_top_max_level",
                                                "thermal_pattern_band_median",
                                                "thermal_pattern_band_sigma",
                                                "thermal_pattern_band_min",
                                                "thermal_pattern_band_max",
                                                "thermal_pattern_dark_band_narrowing",
                                                "thermal_pattern_stay_weight",
                                                "thermal_pattern_adjacent_weight",
                                                "thermal_pattern_two_away_weight",
                                                "thermal_pattern_far_weight",
                                                "thermal_pattern_darkness_bias",
                                                "thermal_pattern_trend_persistence",
                                                "thermal_pattern_trend_strength",
                                                "thermal_pattern_accent_chance",
                                                "thermal_pattern_accent_boost",
                                                "thermal_pattern_accent_min",
                                                "thermal_pattern_accent_max",
                                                "thermal_pattern_top_group_min_time",
                                                "thermal_pattern_top_group_max_lines",
                                                "thermal_pattern_heat_tau",
                                                "thermal_pattern_cool_tau",
                                                "thermal_pattern_tolerance",
                                                "thermal_pattern_surface_heat_credit",
                                                "thermal_pattern_min_base_dwell",
                                                "thermal_pattern_max_preheat",
                                                "thermal_pattern_protect_risky_features",
                                                "thermal_pattern_internal_policy",
                                                "thermal_pattern_speed_assist",
                                                "thermal_pattern_speed_max_factor",
                                                "thermal_pattern_speed_min"};
    const std::vector<std::string> filament_keys{"thermal_pattern_enabled", "thermal_pattern_temperature_step",
                                                 "thermal_pattern_max_temperature"};

    const auto&             persisted_process     = Preset::print_options();
    const auto&             persisted_filament    = Preset::filament_options();
    const auto&             flow_variant_filament = filament_flow_variant_options();
    const PrintRegionConfig region_defaults;
    const FullPrintConfig   full_defaults;
    for (const std::string& key : process_keys) {
        CAPTURE(key);
        REQUIRE(std::find(persisted_process.begin(), persisted_process.end(), key) != persisted_process.end());
        REQUIRE(region_defaults.has(key));
        REQUIRE(print_config_def.get(key)->category == "Thermal Surface Patterning");
    }
    for (const std::string& key : filament_keys) {
        CAPTURE(key);
        REQUIRE(std::find(persisted_filament.begin(), persisted_filament.end(), key) != persisted_filament.end());
        REQUIRE(std::find(flow_variant_filament.begin(), flow_variant_filament.end(), key) != flow_variant_filament.end());
        REQUIRE(full_defaults.has(key));
    }

    const auto& persisted_printer  = Preset::printer_options();
    const auto& per_nozzle_printer = Preset::nozzle_options();
    REQUIRE(std::find(persisted_printer.begin(), persisted_printer.end(), "machine_max_nozzle_temperature") != persisted_printer.end());
    REQUIRE(std::find(per_nozzle_printer.begin(), per_nozzle_printer.end(), "machine_max_nozzle_temperature") != per_nozzle_printer.end());
}

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

TEST_CASE("Thermal preview tracks the addressed heater", "[thermal_pattern][gcode_processor]")
{
    PrintConfig config;
    config.nozzle_diameter.values = {0.4, 0.4};
    config.filament_diameter.values = {1.75, 1.75};
    config.single_extruder_multi_material.value = false;
    config.gcode_flavor.value = gcfMarlinFirmware;
    GCodeProcessor processor;
    processor.apply_config(config);
    processor.initialize("thermal-preview.gcode");
    processor.process_buffer("G90\nM83\nT0\nM104 S210 T0\nM104 S240 T1\nG1 X10 E1 F600\n");
    REQUIRE(processor.get_result().moves.back().temperature == 210.0f);
    processor.process_buffer("T1\nG1 X20 E1\n");
    REQUIRE(processor.get_result().moves.back().temperature == 240.0f);
    processor.process_buffer("M109 S220 T0\nG1 X30 E1\n");
    REQUIRE(processor.get_result().moves.back().temperature == 240.0f);
    processor.process_buffer("M104 S250 T1\nG1 X40 E1\n");
    REQUIRE(processor.get_result().moves.back().temperature == 250.0f);
    processor.process_buffer("M104 S999 T-1\nM109 R999 T999\nG1 X50 E1\n");
    REQUIRE(processor.get_result().moves.back().temperature == 250.0f);
    processor.process_buffer("T0\nG1 X60 E1\n");
    REQUIRE(processor.get_result().moves.back().temperature == 220.0f);
}

TEST_CASE("RepRapFirmware thermal commands change preview temperature without retracting", "[thermal_pattern][gcode_processor]")
{
    PrintConfig config;
    config.gcode_flavor.value = gcfRepRapFirmware;
    GCodeProcessor processor;
    processor.apply_config(config);
    processor.initialize("thermal-preview.gcode");
    processor.process_buffer("G90\nM83\nG1 X10 E1 F600\n");
    const size_t moves = processor.get_result().moves.size();
    processor.process_buffer("G10 S240 P0\n");
    REQUIRE(processor.get_result().moves.size() == moves);
    processor.process_buffer("G1 X20 E1\n");
    REQUIRE(processor.get_result().moves.back().temperature == 240.0f);
    processor.process_buffer("G10 S260 P0\nG1 X30 E1\n");
    REQUIRE(processor.get_result().moves.back().temperature == 260.0f);
}

TEST_CASE("Thermal temperature annotations preserve physical heater addressing", "[thermal_pattern][gcode_writer]")
{
    PrintConfig config;
    config.gcode_flavor.value = gcfMarlinFirmware;
    config.single_extruder_multi_material.value = true;
    GCodeWriter writer;
    writer.apply_print_config(config);
    writer.set_extruders({0, 1});
    REQUIRE(writer.set_temperature(240, false, 1, "THERMAL_PATTERN target") == "M104 S240 ; THERMAL_PATTERN target\n");

    config.single_extruder_multi_material.value = false;
    writer.apply_print_config(config);
    writer.set_extruders({0, 1});
    REQUIRE(writer.set_temperature(240, false, 1, "THERMAL_PATTERN target") == "M104 S240 T1 ; THERMAL_PATTERN target\n");
}

TEST_CASE("Native thermal patterning produces varied targets in exported preview", "[thermal_pattern][integration]")
{
    DynamicPrintConfig config;
    const FullPrintConfig& defaults = FullPrintConfig::defaults();
    config.apply(static_cast<const PrintObjectConfig&>(defaults), true);
    config.apply(static_cast<const PrintRegionConfig&>(defaults), true);
    config.apply(static_cast<const PrintConfig&>(defaults), true);
    config.set_key_value("thermal_pattern_enabled", new ConfigOptionBools{true});
    config.set_key_value("thermal_pattern_mode", new ConfigOptionEnum<ThermalPatternMode>(ThermalPatternMode::AllSurfaces));
    config.set_key_value("thermal_pattern_temperature_step", new ConfigOptionFloats{10.0});
    config.set_key_value("thermal_pattern_max_temperature", new ConfigOptionInts{270});
    config.set_key_value("nozzle_temperature", new ConfigOptionInts{210});
    config.set_key_value("nozzle_temperature_initial_layer", new ConfigOptionInts{210});
    config.set_key_value("machine_max_nozzle_temperature", new ConfigOptionInts{300});
    config.set_key_value("gcode_flavor", new ConfigOptionEnum<GCodeFlavor>(gcfMarlinFirmware));
    config.set_key_value("enable_arc_fitting", new ConfigOptionBool(false));

    Model model;
    ModelObject* object = model.add_object();
    object->name = "thermal-cube.stl";
    object->add_volume(make_cube(10, 10, 10));
    object->add_instance();
    object->ensure_on_bed();
    Print print;
    print.set_status_silent();
    print.auto_assign_extruders(object);
    print.apply(model, config);
    print.process();

    const auto output = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("thermal-%%%%-%%%%.gcode");
    BOOST_SCOPE_EXIT(&output) { boost::system::error_code ec; boost::filesystem::remove(output, ec); } BOOST_SCOPE_EXIT_END
    GCodeProcessorResult result;
    print.export_gcode(output.string(), &result, nullptr);
    std::set<float> targets;
    for (const auto& move : result.moves)
        if (move.type == EMoveType::Extrude)
            targets.insert(move.temperature);
    REQUIRE(targets.size() > 1);
    REQUIRE(*targets.begin() == 210.0f);
    REQUIRE(*targets.rbegin() <= 270.0f);
    std::ifstream stream(output.string());
    const std::string gcode{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    REQUIRE(gcode.find("; THERMAL_PATTERN tool=T0") != std::string::npos);
}
