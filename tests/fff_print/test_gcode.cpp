#include <catch2/catch_test_macros.hpp>

#include <memory>

#include "libslic3r/GCode.hpp"

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
    CHECK(transition.entering.find("M106 S102") != std::string::npos);
    CHECK(transition.entering.find("M106 P2 S89") != std::string::npos);

    CHECK(transition.leaving.find("M104 S210") != std::string::npos);
    CHECK(transition.leaving.find(";_FORCE_RESUME_FAN_SPEED") != std::string::npos);
    CHECK(transition.leaving.find("M106 P2 S38") != std::string::npos);
}
