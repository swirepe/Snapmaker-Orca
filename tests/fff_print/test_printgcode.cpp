#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include "libslic3r/Print.hpp"
#include "libslic3r/libslic3r.h"
#include "libslic3r/GCodeReader.hpp"

#include "test_data.hpp"

#include <algorithm>
#include <regex>

using namespace Slic3r;
using namespace Slic3r::Test;

std::regex perimeters_regex("G1 X[-0-9.]* Y[-0-9.]* E[-0-9.]* ; perimeter");
std::regex infill_regex("G1 X[-0-9.]* Y[-0-9.]* E[-0-9.]* ; infill");
std::regex skirt_regex("G1 X[-0-9.]* Y[-0-9.]* E[-0-9.]* ; skirt");

TEST_CASE("Horizontal fuzzy skin preserves concentric surface paths", "[PrintGCode][FuzzySurface]")
{
    DynamicPrintConfig config = default_print_config();
    config.set_key_value("top_surface_pattern", new ConfigOptionEnum<InfillPattern>(ipConcentric));
    config.set_key_value("bottom_surface_pattern", new ConfigOptionEnum<InfillPattern>(ipConcentric));
    config.set_key_value("fuzzy_skin", new ConfigOptionEnum<FuzzySkinType>(FuzzySkinType::External));
    config.set_key_value("fuzzy_skin_thickness", new ConfigOptionFloat(0.1));
    config.set_key_value("fuzzy_skin_point_distance", new ConfigOptionFloat(0.5));
    config.set_key_value("fuzzy_skin_top_surface", new ConfigOptionBool(true));
    config.set_key_value("fuzzy_skin_bed_surface", new ConfigOptionBool(true));
    config.set_key_value("machine_start_gcode", new ConfigOptionString{});

    const std::string gcode = Slic3r::Test::slice({make_cube(10., 10., 1.)}, config);
    GCodeReader       reader;
    reader.apply_config(config);
    bool   top            = false;
    bool   bottom         = false;
    size_t top_moves      = 0;
    size_t bottom_moves   = 0;
    size_t top_z_moves    = 0;
    size_t bottom_z_moves = 0;
    size_t diagonal_moves = 0;
    reader.parse_buffer(gcode, [&](GCodeReader& state, const GCodeReader::GCodeLine& line) {
        if ((line.comment().find("FEATURE:") != std::string_view::npos || line.comment().find("TYPE:") != std::string_view::npos)) {
            top    = line.comment().find("Top surface") != std::string_view::npos;
            bottom = line.comment().find("Bottom surface") != std::string_view::npos;
        }
        if ((!top && !bottom) || !line.extruding(state) || line.dist_XY(state) <= 0.001)
            return;
        // Square rings are axis aligned; the concentric filler may add short
        // diagonal connectors between rings. A rectilinear fallback would make
        // most of the fill diagonal instead.
        if (std::abs(line.dist_X(state)) >= 0.002 && std::abs(line.dist_Y(state)) >= 0.002)
            ++diagonal_moves;
        if (top) {
            ++top_moves;
            top_z_moves += std::abs(line.dist_Z(state)) > 0.001;
        } else {
            ++bottom_moves;
            bottom_z_moves += std::abs(line.dist_Z(state)) > 0.001;
        }
    });
    REQUIRE(top_moves > 20);
    REQUIRE(bottom_moves > 20);
    REQUIRE(top_z_moves > 10);
    REQUIRE(bottom_z_moves > 10);
    REQUIRE(diagonal_moves < (top_moves + bottom_moves) / 20);
}

TEST_CASE("Painted top surfaces export fuzzy skin and ironing independently", "[PrintGCode][FuzzySurface][painting]")
{
    bool fuzzy   = false;
    bool ironing = false;
    SECTION("plain") {}
    SECTION("fuzzy") { fuzzy = true; }
    SECTION("ironing") { ironing = true; }
    SECTION("fuzzy and ironing") { fuzzy = ironing = true; }

    DynamicPrintConfig config = default_print_config();
    config.set_key_value("fuzzy_skin", new ConfigOptionEnum<FuzzySkinType>(FuzzySkinType::None));
    config.set_key_value("fuzzy_skin_top_surface", new ConfigOptionBool(false));
    config.set_key_value("fuzzy_skin_ironing", new ConfigOptionBool(false));
    config.set_key_value("fuzzy_skin_thickness", new ConfigOptionFloat(0.1));
    config.set_key_value("fuzzy_skin_point_distance", new ConfigOptionFloat(0.5));
    config.set_key_value("ironing_type", new ConfigOptionEnum<IroningType>(IroningType::NoIroning));
    config.set_key_value("machine_start_gcode", new ConfigOptionString{});
    Print print;
    Model model;
    init_print({make_cube(10., 10., 1.)}, print, model, config);
    ModelVolume*                volume = model.objects.front()->volumes.front();
    const indexed_triangle_set& mesh   = volume->mesh().its;
    TriangleSelector            fuzzy_paint(volume->mesh());
    TriangleSelector            ironing_paint(volume->mesh());
    bool                        fuzzy_triangle_painted = false;
    const float                 top_z                  = volume->mesh().bounding_box().max.z();
    for (size_t index = 0; index < mesh.indices.size(); ++index) {
        const auto& triangle = mesh.indices[index];
        if (mesh.vertices[triangle[0]].z() < top_z - 0.001f || mesh.vertices[triangle[1]].z() < top_z - 0.001f ||
            mesh.vertices[triangle[2]].z() < top_z - 0.001f)
            continue;
        // Paint half of the top fuzzy, leaving a plain region to catch accidental
        // merging of ironing paths with different regional height fields.
        if (fuzzy && !fuzzy_triangle_painted) {
            fuzzy_paint.set_facet(int(index), EnforcerBlockerType::FUZZY_SKIN);
            fuzzy_triangle_painted = true;
        }
        if (ironing)
            ironing_paint.set_facet(int(index), EnforcerBlockerType::ENFORCER);
    }
    volume->fuzzy_skin_facets.set(fuzzy_paint);
    volume->ironing_facets.set(ironing_paint);
    REQUIRE(fuzzy_triangle_painted == fuzzy);
    REQUIRE(volume->ironing_facets.empty() == !ironing);
    print.apply(model, config);
    const std::string output = Slic3r::Test::gcode(print);
    GCodeReader       reader;
    reader.apply_config(config);
    bool   top_role               = false;
    bool   ironing_role           = false;
    size_t textured_top_moves     = 0;
    size_t ironing_moves          = 0;
    size_t textured_ironing_moves = 0;
    size_t planar_ironing_moves   = 0;
    reader.parse_buffer(output, [&](GCodeReader& state, const GCodeReader::GCodeLine& line) {
        if ((line.comment().find("FEATURE:") != std::string_view::npos || line.comment().find("TYPE:") != std::string_view::npos)) {
            top_role     = line.comment().find("Top surface") != std::string_view::npos;
            ironing_role = line.comment().find("Ironing") != std::string_view::npos;
        }
        if (!line.extruding(state) || line.dist_XY(state) <= 0.001)
            return;
        if (top_role && line.new_Z(state) > 1.001)
            ++textured_top_moves;
        if (ironing_role) {
            ++ironing_moves;
            if (line.new_Z(state) > 1.001)
                ++textured_ironing_moves;
            else
                ++planar_ironing_moves;
        }
    });
    CHECK((textured_top_moves > 0) == fuzzy);
    CHECK((ironing_moves > 0) == ironing);
    CHECK((textured_ironing_moves > 0) == (fuzzy && ironing));
    if (ironing)
        CHECK(planar_ironing_moves > 0);
}

TEST_CASE("All-solid ironing only textures exposed fuzzy tops", "[PrintGCode][FuzzySurface][ironing]")
{
    DynamicPrintConfig config = default_print_config();
    config.set_key_value("fuzzy_skin", new ConfigOptionEnum<FuzzySkinType>(FuzzySkinType::External));
    config.set_key_value("fuzzy_skin_top_surface", new ConfigOptionBool(true));
    config.set_key_value("fuzzy_skin_ironing", new ConfigOptionBool(true));
    config.set_key_value("fuzzy_skin_thickness", new ConfigOptionFloat(0.1));
    config.set_key_value("fuzzy_skin_point_distance", new ConfigOptionFloat(0.5));
    config.set_key_value("ironing_type", new ConfigOptionEnum<IroningType>(IroningType::AllSolid));
    config.set_key_value("sparse_infill_density", new ConfigOptionPercent(100));
    config.set_key_value("machine_start_gcode", new ConfigOptionString{});
    // Toggle after slicing to exercise invalidation of cached ironing paths.
    config.set_key_value("fuzzy_skin_ironing", new ConfigOptionBool(false));
    Print print;
    Model model;
    init_print({make_cube(10., 10., 1.)}, print, model, config);
    const std::string planar_output = Slic3r::Test::gcode(print);
    REQUIRE(planar_output.find("Ironing") != std::string::npos);
    config.set_key_value("fuzzy_skin_ironing", new ConfigOptionBool(true));
    print.apply(model, config);
    const std::string output = Slic3r::Test::gcode(print);
    GCodeReader       reader;
    reader.apply_config(config);
    bool   ironing_role = false;
    size_t buried_moves = 0;
    size_t top_z_moves  = 0;
    reader.parse_buffer(output, [&](GCodeReader& state, const GCodeReader::GCodeLine& line) {
        if ((line.comment().find("FEATURE:") != std::string_view::npos || line.comment().find("TYPE:") != std::string_view::npos))
            ironing_role = line.comment().find("Ironing") != std::string_view::npos;
        if (!ironing_role || !line.extruding(state) || line.dist_XY(state) <= 0.001)
            return;
        if (line.new_Z(state) < 0.999) {
            ++buried_moves;
            CHECK(std::abs(line.dist_Z(state)) < 0.001);
        } else if (std::abs(line.dist_Z(state)) > 0.001) {
            ++top_z_moves;
        }
    });
    REQUIRE(buried_moves > 0);
    REQUIRE(top_z_moves > 0);
}

TEST_CASE("Thermal surface patterning uses the regional nozzle temperature as its baseline", "[PrintGCode][thermal_pattern][regional_settings]")
{
    DynamicPrintConfig config = default_print_config();
    config.set_key_value("thermal_pattern_enabled", new ConfigOptionBools {true});
    config.set_key_value("thermal_pattern_temperature_step", new ConfigOptionFloats {0.0});
    config.set_key_value("thermal_pattern_mode", new ConfigOptionEnum<ThermalPatternMode>(ThermalPatternMode::AllSurfaces));
    config.set_key_value("nozzle_temperature_override", new ConfigOptionInt(235));
    config.set_key_value("machine_start_gcode", new ConfigOptionString {});

    const std::string gcode = Slic3r::Test::slice({make_cube(10., 10., 1.)}, config);

    REQUIRE(gcode.find("THERMAL_PATTERN") != std::string::npos);
    REQUIRE(gcode.find("target=235C") != std::string::npos);
}

SCENARIO( "PrintGCode basic functionality", "[PrintGCode]") {
    GIVEN("A default configuration and a print test object") {
        WHEN("the output is executed with no support material") {
            Slic3r::Print print;
            Slic3r::Model model;
            Slic3r::Test::init_print({TestMesh::cube_20x20x20}, print, model, {
                { "layer_height",					0.2 },
                { "initial_layer_print_height",		0.2 },
                { "initial_layer_line_width",	0 },
                { "gcode_comments",					true },
                { "machine_start_gcode",			"" }
                });
            std::string gcode = Slic3r::Test::gcode(print);
            THEN("Some text output is generated.") {
                REQUIRE(gcode.size() > 0);
            }
            THEN("Exported text contains slic3r version") {
                REQUIRE(gcode.find("generated by Snapmaker Orca") != std::string::npos);
            }
            //THEN("Exported text contains git commit id") {
            //    REQUIRE(gcode.find("; Git Commit") != std::string::npos);
            //    REQUIRE(gcode.find(SLIC3R_BUILD_ID) != std::string::npos);
            //}
            THEN("Exported text contains extrusion statistics.") {
                REQUIRE(gcode.find("; external perimeters extrusion width") != std::string::npos);
                REQUIRE(gcode.find("; perimeters extrusion width") != std::string::npos);
                REQUIRE(gcode.find("; infill extrusion width") != std::string::npos);
                REQUIRE(gcode.find("; solid infill extrusion width") != std::string::npos);
                REQUIRE(gcode.find("; top infill extrusion width") != std::string::npos);
                REQUIRE(gcode.find("; support material extrusion width") == std::string::npos);
                REQUIRE(gcode.find("; first layer extrusion width") == std::string::npos);
            }
            THEN("Exported text does not contain cooling markers (they were consumed)") {
                REQUIRE(gcode.find(";_EXTRUDE_SET_SPEED") == std::string::npos);
            }

            THEN("GCode preamble is emitted.") {
                REQUIRE(gcode.find("G21") != std::string::npos);
            }

            THEN("Config options emitted for print config, default region config, default object config") {
                REQUIRE(gcode.find("; first_layer_temperature") != std::string::npos);
                REQUIRE(gcode.find("; layer_height") != std::string::npos);
                REQUIRE(gcode.find("; sparse_infill_density") != std::string::npos);
            }
            THEN("Infill is emitted.") {
                std::smatch has_match;
                REQUIRE(std::regex_search(gcode, has_match, infill_regex));
            }
            THEN("Perimeters are emitted.") {
				std::smatch has_match;
                REQUIRE(std::regex_search(gcode, has_match, perimeters_regex));
            }
            THEN("Skirt is emitted.") {
                std::smatch has_match;
                REQUIRE(std::regex_search(gcode, has_match, skirt_regex));
            }
            THEN("final Z height is 20mm") {
                double final_z = 0.0;
                GCodeReader reader;
                reader.apply_config(print.config());
                reader.parse_buffer(gcode, [&final_z] (GCodeReader& self, const GCodeReader::GCodeLine& line) {
                    final_z = std::max<double>(final_z, static_cast<double>(self.z())); // record the highest Z point we reach
                });
                REQUIRE_THAT(final_z, WithinRel(20.4, 0.001));
            }
        }
        WHEN("output is executed with complete objects and two differently-sized meshes") {
            Slic3r::Print print;
            Slic3r::Model model;
            Slic3r::Test::init_print({TestMesh::cube_20x20x20,TestMesh::cube_20x20x20}, print, model, {
                { "initial_layer_line_width",    0 },
                { "initial_layer_print_height",     0.3 },
                { "layer_height",                   0.2 },
                { "enable_support",                 false },
                { "raft_layers",                    0 },
                { "print_sequence",                 "by object" },
                { "gcode_comments",                 true },
                { "printing_by_object_gcode",       "; between-object-gcode" }
                });
            std::string gcode = Slic3r::Test::gcode(print);
            THEN("Some text output is generated.") {
                REQUIRE(gcode.size() > 0);
            }
            THEN("Infill is emitted.") {
                std::smatch has_match;
                REQUIRE(std::regex_search(gcode, has_match, infill_regex));
            }
            THEN("Perimeters are emitted.") {
                std::smatch has_match;
                REQUIRE(std::regex_search(gcode, has_match, perimeters_regex));
            }
            THEN("Skirt is emitted.") {
                std::smatch has_match;
                REQUIRE(std::regex_search(gcode, has_match, skirt_regex));
            }
            THEN("Between-object-gcode is emitted.") {
                REQUIRE(gcode.find("; between-object-gcode") != std::string::npos);
            }
            THEN("final Z height is 20.4mm") {
                double final_z = 0.0;
                GCodeReader reader;
                reader.apply_config(print.config());
                reader.parse_buffer(gcode, [&final_z] (GCodeReader& self, const GCodeReader::GCodeLine& line) {
                    final_z = std::max(final_z, static_cast<double>(self.z())); // record the highest Z point we reach
                });
                REQUIRE_THAT(final_z, WithinRel(20.5, 0.001));
            }
            THEN("Z height resets on object change") {
                double final_z = 0.0;
                bool reset = false;
                GCodeReader reader;
                reader.apply_config(print.config());
                reader.parse_buffer(gcode, [&final_z, &reset] (GCodeReader& self, const GCodeReader::GCodeLine& line) {
                    if (final_z > 0 && std::abs(self.z() - 0.3) < 0.01 ) { // saw higher Z before this, now it's lower
                        reset = true;
                    } else {
                        final_z = std::max(final_z, static_cast<double>(self.z())); // record the highest Z point we reach
                    }
                });
                REQUIRE(reset == true);
            }
            THEN("Shorter object is printed before taller object.") {
                double final_z = 0.0;
                bool reset = false;
                GCodeReader reader;
                reader.apply_config(print.config());
                reader.parse_buffer(gcode, [&final_z, &reset] (GCodeReader& self, const GCodeReader::GCodeLine& line) {
                    if (final_z > 0 && std::abs(self.z() - 0.3) < 0.01 ) { 
                        reset = (final_z > 20.0);
                    } else {
                        final_z = std::max(final_z, static_cast<double>(self.z())); // record the highest Z point we reach
                    }
                });
                REQUIRE(reset == true);
            }
        }
        WHEN("the output is executed with support material") {
            std::string gcode = ::Test::slice({TestMesh::cube_20x20x20}, {
                { "initial_layer_line_width",    0 },
                { "enable_support",                 true },
                { "raft_layers",                    3 },
                { "gcode_comments",                 true }
                });
            THEN("Some text output is generated.") {
                REQUIRE(gcode.size() > 0);
            }
            THEN("Exported text contains extrusion statistics.") {
                REQUIRE(gcode.find("; external perimeters extrusion width") != std::string::npos);
                REQUIRE(gcode.find("; perimeters extrusion width") != std::string::npos);
                REQUIRE(gcode.find("; infill extrusion width") != std::string::npos);
                REQUIRE(gcode.find("; solid infill extrusion width") != std::string::npos);
                REQUIRE(gcode.find("; top infill extrusion width") != std::string::npos);
                REQUIRE(gcode.find("; support material extrusion width") != std::string::npos);
                REQUIRE(gcode.find("; first layer extrusion width") == std::string::npos);
            }
            THEN("Raft is emitted.") {
                REQUIRE(gcode.find("; raft") != std::string::npos);
            }
        }
        WHEN("the output is executed with a separate first layer extrusion width") {
			std::string gcode = ::Test::slice({ TestMesh::cube_20x20x20 }, {
                { "initial_layer_line_width", "0.5" }
                });
            THEN("Some text output is generated.") {
                REQUIRE(gcode.size() > 0);
            }
            THEN("Exported text contains extrusion statistics.") {
                REQUIRE(gcode.find("; external perimeters extrusion width") != std::string::npos);
                REQUIRE(gcode.find("; perimeters extrusion width") != std::string::npos);
                REQUIRE(gcode.find("; infill extrusion width") != std::string::npos);
                REQUIRE(gcode.find("; solid infill extrusion width") != std::string::npos);
                REQUIRE(gcode.find("; top infill extrusion width") != std::string::npos);
                REQUIRE(gcode.find("; support material extrusion width") == std::string::npos);
                REQUIRE(gcode.find("; first layer extrusion width") != std::string::npos);
            }
        }
        WHEN("Cooling is enabled and the fan is disabled.") {
			std::string gcode = ::Test::slice({ TestMesh::cube_20x20x20 }, {
				{ "slow_down_for_layer_cooling",    true },
                { "close_fan_the_first_x_layers",  5 }
                });
            THEN("GCode to disable fan is emitted."){
                REQUIRE(gcode.find("M106 S0") != std::string::npos);
            }
        }
        WHEN("end_gcode exists with layer_num and layer_z") {
			std::string gcode = ::Test::slice({ TestMesh::cube_20x20x20 }, {
				{ "machine_end_gcode",      "; Layer_num [layer_num]\n; Layer_z [layer_z]" },
                { "layer_height",           0.1 },
                { "initial_layer_print_height", 0.1 }
                });
            THEN("layer_num and layer_z are processed in the end gcode") {
                REQUIRE(gcode.find("; Layer_num 199") != std::string::npos);
                REQUIRE(gcode.find("; Layer_z 20") != std::string::npos);
            }
        }
        WHEN("current_extruder exists in start_gcode") {
            {
				std::string gcode = ::Test::slice({ TestMesh::cube_20x20x20 }, {
					{ "machine_start_gcode", "; Extruder [current_extruder]" }
                });
                THEN("current_extruder is processed in the start gcode and set for first extruder") {
                    REQUIRE(gcode.find("; Extruder 0") != std::string::npos);
                }
            }
			{
                DynamicPrintConfig config = Slic3r::Test::default_print_config();
                config.set_num_extruders(4);
                config.set_deserialize_strict({
                    { "machine_start_gcode",            "; Extruder [current_extruder]" },
                    { "infill_extruder",                2 },
                    { "solid_infill_extruder",          2 },
                    { "perimeter_extruder",             2 },
                    { "support_material_extruder",      2 },
                    { "support_material_interface_extruder", 2 }
                });
                std::string gcode = Slic3r::Test::slice({TestMesh::cube_20x20x20}, config);
                THEN("current_extruder is processed in the start gcode before tool changes") {
                    REQUIRE(gcode.find("; Extruder 0") != std::string::npos);
                }
            }
        }

        WHEN("layer_num represents the layer's index from z=0") {
			std::string gcode = ::Test::slice({ TestMesh::cube_20x20x20, TestMesh::cube_20x20x20 }, {
				{ "print_sequence",                 "by object" },
                { "gcode_comments",                 true },
                { "before_layer_change_gcode",      ";Layer:[layer_num] ([layer_z] mm)" },
                { "layer_height",                   0.1 },
                { "initial_layer_print_height",     0.1 }
                });
			// End of the 1st object.
            std::string token = ";Layer:199 ";
			size_t pos = gcode.find(token);
			THEN("First and second object last layer is emitted") {
				// First object
				REQUIRE(pos != std::string::npos);
				pos += token.size();
				REQUIRE(pos < gcode.size());
				double z = 0;
				REQUIRE((sscanf(gcode.data() + pos, "(%lf mm)", &z) == 1));
				REQUIRE_THAT(z, WithinRel(20., 0.001));
				// Second object
				pos = gcode.find(";Layer:399 ", pos);
				REQUIRE(pos != std::string::npos);
				pos += token.size();
				REQUIRE(pos < gcode.size());
				REQUIRE((sscanf(gcode.data() + pos, "(%lf mm)", &z) == 1));
				REQUIRE_THAT(z, WithinRel(20., 0.001));
			}
        }
    }
}
