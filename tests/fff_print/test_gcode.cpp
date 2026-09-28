#include <catch2/catch_test_macros.hpp>

#include <memory>

#include "libslic3r/GCode.hpp"
#include "libslic3r/GCode/CoolingBuffer.hpp"

using namespace Slic3r;

namespace Slic3r {

class GCodeRegionProcessOverridesTest
{
public:
    struct Transition
    {
        std::string entering;
        std::string leaving;
    };

    struct FanLayers
    {
        std::string first;
        std::string second;
    };

    static Transition run()
    {
        GCode gcodegen;
        gcodegen.apply_print_config(static_cast<const PrintConfig &>(FullPrintConfig::defaults()));
        gcodegen.m_config.apply(static_cast<const PrintRegionConfig &>(FullPrintConfig::defaults()), true);
        gcodegen.writer().set_extruders({0});
        gcodegen.writer().set_extruder(0);
        gcodegen.reset_region_process_overrides();

        gcodegen.m_config.nozzle_temperature.values             = {210};
        gcodegen.m_config.nozzle_temperature_override.value     = 275;
        gcodegen.m_config.fan_speed_override.value              = 60;
        gcodegen.m_config.wall_fan_speed_override.value         = 40;
        gcodegen.m_config.auxiliary_fan.value                   = true;
        gcodegen.m_config.additional_cooling_fan_speed.values   = {15};
        gcodegen.m_config.auxiliary_fan_speed_override.value    = 35;
        const std::string entering = gcodegen.set_region_process_overrides(erPerimeter);

        gcodegen.m_config.nozzle_temperature_override.value  = 0;
        gcodegen.m_config.fan_speed_override.value           = -1;
        gcodegen.m_config.wall_fan_speed_override.value      = -1;
        gcodegen.m_config.auxiliary_fan_speed_override.value = -1;
        const std::string leaving = gcodegen.set_region_process_overrides(erSolidInfill);

        return {entering, leaving};
    }

    static FanLayers run_fan_layer_transition()
    {
        GCode gcodegen;
        gcodegen.apply_print_config(static_cast<const PrintConfig &>(FullPrintConfig::defaults()));
        gcodegen.writer().set_extruders({0});
        gcodegen.writer().set_extruder(0);
        gcodegen.m_config.close_fan_the_first_x_layers.values = {1};
        gcodegen.m_config.full_fan_speed_layer.values          = {0};
        gcodegen.m_config.fan_min_speed.values                 = {20.0};
        gcodegen.m_config.fan_max_speed.values                 = {20.0};
        gcodegen.m_config.slow_down_for_layer_cooling.values   = {false};

        CoolingBuffer cooling(gcodegen);
        std::string first = cooling.process_layer(
            std::string(REGION_FAN_SPEED_MARKER) + "40\n"
                "G1 X1 Y0 E0.1 F600 ;_EXTRUDE_SET_SPEED\n"
                ";_EXTRUDE_END\n",
            0, true);
        std::string second = cooling.process_layer(
            "G1 X2 Y0 E0.2 F600 ;_EXTRUDE_SET_SPEED\n"
            ";_EXTRUDE_END\n",
            1, true);
        return {std::move(first), std::move(second)};
    }

    static std::string run_fan_role_transition()
    {
        GCode gcodegen;
        gcodegen.apply_print_config(static_cast<const PrintConfig &>(FullPrintConfig::defaults()));
        gcodegen.writer().set_extruders({0});
        gcodegen.writer().set_extruder(0);
        gcodegen.m_config.close_fan_the_first_x_layers.values = {0};
        gcodegen.m_config.full_fan_speed_layer.values          = {0};
        gcodegen.m_config.fan_min_speed.values                 = {20.0};
        gcodegen.m_config.fan_max_speed.values                 = {20.0};
        gcodegen.m_config.overhang_fan_speed.values            = {100};
        gcodegen.m_config.slow_down_for_layer_cooling.values   = {false};

        CoolingBuffer cooling(gcodegen);
        return cooling.process_layer(
            std::string(REGION_FAN_SPEED_MARKER) + "40\n"
                ";_OVERHANG_FAN_START\n"
                "G1 X1 Y0 E0.1 F600 ;_EXTRUDE_SET_SPEED\n" +
                REGION_FAN_SPEED_MARKER + "-1\n"
                "G1 X2 Y0 E0.2 F600\n"
                ";_OVERHANG_FAN_END\n"
                "G1 X3 Y0 E0.3 F600\n"
                ";_EXTRUDE_END\n",
            0, true);
    }
};

} // namespace Slic3r

SCENARIO("Origin manipulation", "[GCode]") {
	Slic3r::GCode gcodegen;
	WHEN("set_origin to (10,0)") {
    	gcodegen.set_origin(Vec2d(10,0));
    	REQUIRE(gcodegen.origin() == Vec2d(10, 0));
    }
	WHEN("set_origin to (10,0) and translate by (5, 5)") {
		gcodegen.set_origin(Vec2d(10,0));
		gcodegen.set_origin(gcodegen.origin() + Vec2d(5, 5));
		THEN("origin returns reference to point") {
    		REQUIRE(gcodegen.origin() == Vec2d(15,5));
    	}
    }
}

TEST_CASE("Regional nozzle and fan overrides restore inherited state", "[GCode][PaneCalibration]")
{
    const GCodeRegionProcessOverridesTest::Transition transition = GCodeRegionProcessOverridesTest::run();

    CHECK(transition.entering.find("M104 S275") != std::string::npos);
    CHECK(transition.entering.find(std::string(REGION_FAN_SPEED_MARKER) + "40") != std::string::npos);
    CHECK(transition.entering.find("M106 P2 S89") != std::string::npos);

    CHECK(transition.leaving.find("M104 S210") != std::string::npos);
    CHECK(transition.leaving.find(std::string(REGION_FAN_SPEED_MARKER) + "-1") != std::string::npos);
    CHECK(transition.leaving.find("M106 P2 S38") != std::string::npos);
}

TEST_CASE("Regional fan overrides survive layer cooling changes", "[GCode][regional_settings]")
{
    const GCodeRegionProcessOverridesTest::FanLayers layers = GCodeRegionProcessOverridesTest::run_fan_layer_transition();

    CHECK(layers.first.find("M106 S102") != std::string::npos);
    CHECK(layers.second.find("M106 S51") == std::string::npos);
    CHECK(layers.second.find("M106") == std::string::npos);
    CHECK(layers.first.find(REGION_FAN_SPEED_MARKER) == std::string::npos);
    CHECK(layers.second.find(REGION_FAN_SPEED_MARKER) == std::string::npos);
}

TEST_CASE("Regional fan overrides take precedence over role cooling", "[GCode][regional_settings]")
{
    const std::string gcode = GCodeRegionProcessOverridesTest::run_fan_role_transition();
    const size_t      regional = gcode.find("M106 S102");
    const size_t      overhang = gcode.find("M106 S255", regional);
    const size_t      resumed  = gcode.find("M106 S51", overhang);

    REQUIRE(regional != std::string::npos);
    REQUIRE(overhang != std::string::npos);
    CHECK(resumed != std::string::npos);
    CHECK(regional < overhang);
    CHECK(overhang < resumed);
    CHECK(gcode.find(REGION_FAN_SPEED_MARKER) == std::string::npos);
}
