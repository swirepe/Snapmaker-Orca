#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "slic3r/GUI/GUI_Factories.hpp"

using namespace Slic3r::GUI;

TEST_CASE("Regional process controls appear once in object and part quick settings", "[GUI][PaneCalibration]")
{
    const std::vector<std::string> regional_keys {
        "ironing_type",
        "ironing_pattern",
        "ironing_flow",
        "ironing_spacing",
        "ironing_inset",
        "ironing_angle",
        "ironing_speed",
        "nozzle_temperature_override",
        "fan_speed_override",
        "wall_fan_speed_override",
        "ironing_fan_speed_override",
        "auxiliary_fan_speed_override",
    };

    const auto part_it   = SettingsFactory::PART_CATEGORY_SETTINGS.find("Quality");
    const auto object_it = SettingsFactory::OBJECT_CATEGORY_SETTINGS.find("Quality");
    REQUIRE(part_it != SettingsFactory::PART_CATEGORY_SETTINGS.end());
    REQUIRE(object_it != SettingsFactory::OBJECT_CATEGORY_SETTINGS.end());

    for (const std::string &key : regional_keys) {
        const auto count_key = [&key](const std::vector<SimpleSettingData> &settings) {
            return std::count_if(settings.begin(), settings.end(), [&key](const SimpleSettingData &setting) {
                return setting.name == key;
            });
        };
        CHECK(count_key(part_it->second) == 1);
        CHECK(count_key(object_it->second) == 0);
    }

    for (const bool is_part : {true, false}) {
        const auto visible = SettingsFactory::get_visible_options("Quality", is_part);
        std::set<std::string> names;
        for (const SimpleSettingData &setting : visible)
            CHECK(names.insert(setting.name).second);
    }
}
