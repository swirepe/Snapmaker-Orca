#include <catch2/catch_test_macros.hpp>

#include "libslic3r/libslic3r.h"
#include "libslic3r/Print.hpp"
#include "libslic3r/Layer.hpp"
#include "libslic3r/FuzzySkinCalibration.hpp"

#include "test_data.hpp"

using namespace Slic3r;
using namespace Slic3r::Test;

TEST_CASE("Vertical fuzzy skin contributes to height and clearance safety", "[Print][FuzzySurface]")
{
    DynamicPrintConfig config = default_print_config();
    config.set_key_value("printable_height", new ConfigOptionFloat(2.5));
    config.set_key_value("nozzle_height", new ConfigOptionFloat(2.5));
    config.set_key_value("fuzzy_skin", new ConfigOptionEnum<FuzzySkinType>(FuzzySkinType::External));
    config.set_key_value("fuzzy_skin_thickness", new ConfigOptionFloat(0.5));
    config.set_key_value("fuzzy_skin_point_distance", new ConfigOptionFloat(0.3));
    config.set_key_value("fuzzy_skin_top_surface", new ConfigOptionBool(true));

    Model        model;
    ModelObject *object = model.add_object("fuzzy height", "", make_cube(10.0, 10.0, 2.3));
    object->add_instance();
    Print print;
    print.auto_assign_extruders(object);
    print.apply(model, config);

    REQUIRE_FALSE(print.is_all_objects_are_short());
    REQUIRE_FALSE(print.validate().string.empty());
}

SCENARIO("PrintObject: Perimeter generation", "[PrintObject]") {
    GIVEN("20mm cube and default config") {
        WHEN("make_perimeters() is called")  {
            Slic3r::Print print;
            Slic3r::Test::init_and_process_print({TestMesh::cube_20x20x20}, print, { { "sparse_infill_density", 0 } });
			const PrintObject &object = *print.objects().front();
			THEN("100 layers exist in the model") {
                REQUIRE(object.layers().size() == 100);
            }
            THEN("Every layer in region 0 has 1 island of perimeters") {
                for (const Layer *layer : object.layers())
                    REQUIRE(layer->regions().front()->perimeters.entities.size() == 1);
            }
			THEN("Every layer in region 0 has 2 paths in its perimeters list.") {
                for (const Layer *layer : object.layers())
                    REQUIRE(layer->regions().front()->perimeters.items_count() == 2);
            }
        }
    }
}

SCENARIO("Print: Skirt generation", "[Print]") {
    GIVEN("20mm cube and default config") {
        WHEN("Skirts is set to 2 loops")  {
            Slic3r::Print print;
            Slic3r::Test::init_and_process_print({TestMesh::cube_20x20x20}, print, {
            	{ "skirt_height", 	1 },
        		{ "skirt_distance", 1 },
                { "skirt_loops",   2 }
            });
            THEN("Skirt Extrusion collection has 2 loops in it") {
                REQUIRE(print.skirt().items_count() == 2);
                REQUIRE(print.skirt().flatten().entities.size() == 2);
            }
        }
    }
}

SCENARIO("Print: Changing number of solid surfaces does not cause all surfaces to become internal.", "[Print]") {
    GIVEN("sliced 20mm cube and config with top_solid_surfaces = 2 and bottom_solid_surfaces = 1") {
        Slic3r::DynamicPrintConfig config = Slic3r::Test::default_print_config();
		config.set_deserialize_strict({
			{ "top_shell_layers",		2 },
			{ "bottom_shell_layers",	1 },
			{ "layer_height",			0.25 }, // get a known number of layers
			{ "initial_layer_print_height", 0.25 }
			});
        Slic3r::Print print;
        Slic3r::Model model;
        Slic3r::Test::init_print({TestMesh::cube_20x20x20}, print, model, config);
        // Precondition: Ensure that the model has 2 solid top layers (39, 38)
        // and one solid bottom layer (0).
		auto test_is_solid_infill = [&print](size_t obj_id, size_t layer_id) {
		    const Layer &layer = *(print.objects().at(obj_id)->get_layer((int)layer_id));
		    // iterate over all of the regions in the layer
		    for (const LayerRegion *region : layer.regions()) {
		        // for each region, iterate over the fill surfaces
		        for (const Surface &surface : region->fill_surfaces.surfaces)
		            CHECK(surface.is_solid());
		    }
		};
        print.process();
        test_is_solid_infill(0,  0); // should be solid
        test_is_solid_infill(0, 79); // should be solid
        test_is_solid_infill(0, 78); // should be solid
        WHEN("Model is re-sliced with top_solid_layers == 3") {
			config.set_deserialize_strict("top_shell_layers", "3");
			print.apply(model, config);
            print.process();
            THEN("Print object does not have 0 solid bottom layers.") {
                test_is_solid_infill(0, 0);
            }
            AND_THEN("Print object has 3 top solid layers") {
                test_is_solid_infill(0, 79);
                test_is_solid_infill(0, 78);
                test_is_solid_infill(0, 77);
            }
        }
    }
}

SCENARIO("Print: Brim generation", "[Print]") {
    GIVEN("20mm cube and default config, 1mm first layer width") {
        WHEN("Brim is set to 3mm")  {
	        Slic3r::Print print;
	        Slic3r::Test::init_and_process_print({TestMesh::cube_20x20x20}, print, {
	        	{ "initial_layer_line_width", 	1 },
                { "brim_type",                     "outer_only" },
	        	{ "brim_width", 					3 }
	        });
            THEN("Brim Extrusion collection has 2 loops in it") {
                size_t total_items = 0;
                for (const auto& pair : print.get_brimMap()) {
                    total_items += pair.second.items_count();
                }
                REQUIRE(total_items == 2);
            }
        }
        WHEN("Brim is set to 6mm")  {
	        Slic3r::Print print;
	        Slic3r::Test::init_and_process_print({TestMesh::cube_20x20x20}, print, {
	        	{ "initial_layer_line_width", 	1 },
                { "brim_type",                     "outer_only" },
	        	{ "brim_width", 					6 }
	        });
            THEN("Brim Extrusion collection has 6 loops in it") {
                size_t total_items = 0;
                for (const auto& pair : print.get_brimMap()) {
                    total_items += pair.second.items_count();
                }
                REQUIRE(total_items == 6);
            }
        }
        WHEN("Brim is set to 6mm, extrusion width 0.5mm")  {
	        Slic3r::Print print;
	        Slic3r::Test::init_and_process_print({TestMesh::cube_20x20x20}, print, {
	        	{ "initial_layer_line_width", 	1 },
                { "brim_type",                     "outer_only" },
	        	{ "brim_width", 					6 },
	        	{ "initial_layer_line_width", 	0.5 }
	        });
			print.process();
            THEN("Brim Extrusion collection has 12 loops in it") {
                size_t total_items = 0;
                for (const auto& pair : print.get_brimMap()) {
                    total_items += pair.second.items_count();
                }
                REQUIRE(total_items == 12);
            }
        }
    }
}

TEST_CASE("Four-treatment cube slices with distinct top and ironing regions", "[Print][FuzzySurface][Calibration]")
{
    FuzzySkinCalibrationConfig calibration;
    calibration.mode         = FuzzySkinCalibrationMode::CubeSingle;
    calibration.coupon_width = calibration.coupon_depth = 10.0;
    calibration.labels                                  = false;
    Model        model;
    ModelObject* object = model.add_object();
    populate_fuzzy_skin_calibration_cube(*object, calibration, build_fuzzy_skin_calibration_plan(calibration).cells.front());
    object->add_instance();
    DynamicPrintConfig config = default_print_config();
    config.set_key_value("sparse_infill_density", new ConfigOptionPercent(100));
    config.set_key_value("top_shell_layers", new ConfigOptionInt(3));
    Print print;
    print.auto_assign_extruders(object);
    print.apply(model, config);
    print.set_status_silent();
    const std::string output = gcode(print);
    REQUIRE_FALSE(output.empty());
    const Layer* top = print.objects().front()->layers().back();
    REQUIRE(top->regions().size() == 4);
    size_t fuzzy_regions = 0, ironing_regions = 0, fuzzy_ironing_regions = 0;
    for (const LayerRegion* region : top->regions()) {
        const auto& settings = region->region().config();
        fuzzy_regions += settings.fuzzy_skin_top_surface.value;
        fuzzy_ironing_regions += settings.fuzzy_skin_ironing.value;
        bool       has_top_fill = false, has_ironing = false;
        const auto flattened = region->fills.flatten();
        for (const auto* entity : flattened.entities) {
            has_top_fill |= entity->role() == erTopSolidInfill;
            has_ironing |= entity->role() == erIroning;
        }
        CAPTURE(settings.fuzzy_skin.value, settings.ironing_type.value, flattened.entities.size(), region->slices.surfaces.size());
        REQUIRE(has_top_fill);
        CHECK(has_ironing == (settings.ironing_type.value == IroningType::AllSolid));
        ironing_regions += has_ironing;
    }
    REQUIRE(fuzzy_regions == 2);
    REQUIRE(ironing_regions == 2);
    REQUIRE(fuzzy_ironing_regions == 1);
}
