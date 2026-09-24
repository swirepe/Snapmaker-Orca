#include <catch2/catch_test_macros.hpp>

#include "libslic3r/Model.hpp"
#include "libslic3r/Format/3mf.hpp"
#include "libslic3r/Format/STL.hpp"
#include "libslic3r/PrintConfig.hpp"

#include <boost/filesystem/operations.hpp>

using namespace Slic3r;

SCENARIO("Reading 3mf file", "[3mf]") {
    GIVEN("umlauts in the path of the file") {
        Model model;
        WHEN("3mf model is read") {
        	std::string path = std::string(TEST_DATA_DIR) + "/test_3mf/Geräte/Büchse.3mf";
        	DynamicPrintConfig config;
            ConfigSubstitutionContext ctxt{ ForwardCompatibilitySubstitutionRule::Disable };
            bool ret = load_3mf(path.c_str(), config, ctxt, &model, false);
            THEN("load should succeed") {
                REQUIRE(ret);
            }
        }
    }
}

SCENARIO("Export+Import geometry to/from 3mf file cycle", "[3mf]") {
    GIVEN("world vertices coordinates before save") {
        // load a model from stl file
        Model src_model;
        std::string src_file = std::string(TEST_DATA_DIR) + "/test_3mf/Prusa.stl";
        load_stl(src_file.c_str(), &src_model);
        src_model.add_default_instances();

        ModelObject* src_object = src_model.objects.front();

        // apply generic transformation to the 1st volume
        Geometry::Transformation src_volume_transform;
        src_volume_transform.set_offset({ 10.0, 20.0, 0.0 });
        src_volume_transform.set_rotation({ Geometry::deg2rad(25.0), Geometry::deg2rad(35.0), Geometry::deg2rad(45.0) });
        src_volume_transform.set_scaling_factor({ 1.1, 1.2, 1.3 });
        src_volume_transform.set_mirror({ -1.0, 1.0, -1.0 });
        src_object->volumes.front()->set_transformation(src_volume_transform);

        // apply generic transformation to the 1st instance
        Geometry::Transformation src_instance_transform;
        src_instance_transform.set_offset({ 5.0, 10.0, 0.0 });
        src_instance_transform.set_rotation({ Geometry::deg2rad(12.0), Geometry::deg2rad(13.0), Geometry::deg2rad(14.0) });
        src_instance_transform.set_scaling_factor({ 0.9, 0.8, 0.7 });
        src_instance_transform.set_mirror({ 1.0, -1.0, -1.0 });
        src_object->instances.front()->set_transformation(src_instance_transform);

        WHEN("model is saved+loaded to/from 3mf file") {
            // save the model to 3mf file
            std::string test_file = std::string(TEST_DATA_DIR) + "/test_3mf/prusa.3mf";
            store_3mf(test_file.c_str(), &src_model, nullptr, false);

            // load back the model from the 3mf file
            Model dst_model;
            DynamicPrintConfig dst_config;
            {
                ConfigSubstitutionContext ctxt{ ForwardCompatibilitySubstitutionRule::Disable };
                load_3mf(test_file.c_str(), dst_config, ctxt, &dst_model, false);
            }
            boost::filesystem::remove(test_file);

            // compare meshes
            TriangleMesh src_mesh = src_model.mesh();
            TriangleMesh dst_mesh = dst_model.mesh();

            bool res = src_mesh.its.vertices.size() == dst_mesh.its.vertices.size();
            if (res) {
                for (size_t i = 0; i < dst_mesh.its.vertices.size(); ++i) {
                    res &= dst_mesh.its.vertices[i].isApprox(src_mesh.its.vertices[i]);
                }
            }
            THEN("world vertices coordinates after load match") {
                REQUIRE(res);
            }
        }
    }
}

TEST_CASE("Fuzzy modifier settings survive a standard 3MF round trip", "[3mf][FuzzySurface]")
{
    Model source;
    const std::string source_file = std::string(TEST_DATA_DIR) + "/test_3mf/Prusa.stl";
    load_stl(source_file.c_str(), &source);
    source.add_default_instances();

    ModelConfig &config = source.objects.front()->volumes.front()->config;
    config.set_key_value("fuzzy_skin", new ConfigOptionEnum<FuzzySkinType>(FuzzySkinType::AllWalls));
    config.set_key_value("fuzzy_skin_thickness", new ConfigOptionFloat(0.47));
    config.set_key_value("fuzzy_skin_point_distance", new ConfigOptionFloat(0.83));
    config.set_key_value("fuzzy_skin_first_layer", new ConfigOptionBool(true));
    config.set_key_value("fuzzy_skin_noise_type", new ConfigOptionEnum<NoiseType>(NoiseType::Billow));
    config.set_key_value("fuzzy_skin_mode", new ConfigOptionEnum<FuzzySkinMode>(FuzzySkinMode::Combined));
    config.set_key_value("fuzzy_skin_scale", new ConfigOptionFloat(1.7));
    config.set_key_value("fuzzy_skin_octaves", new ConfigOptionInt(6));
    config.set_key_value("fuzzy_skin_persistence", new ConfigOptionFloat(0.73));
    config.set_key_value("fuzzy_skin_top_surface", new ConfigOptionBool(true));
    config.set_key_value("fuzzy_skin_lower_surface", new ConfigOptionBool(true));
    config.set_key_value("fuzzy_skin_top_surface_first_layer", new ConfigOptionBool(true));
    config.set_key_value("fuzzy_skin_bed_surface", new ConfigOptionBool(true));
    config.set_key_value("fuzzy_skin_connect_walls", new ConfigOptionBool(false));
    config.set_key_value("fuzzy_skin_compensate_extrusion", new ConfigOptionBool(false));
    config.set_key_value("fuzzy_skin_bridge_compensation_multiplier", new ConfigOptionFloat(2.4));
    config.set_key_value("fuzzy_skin_min_support_distance", new ConfigOptionFloat(0.31));
    config.set_key_value("fuzzy_skin_ironing", new ConfigOptionBool(true));

    const std::vector<std::string> keys {"fuzzy_skin",
                                         "fuzzy_skin_thickness",
                                         "fuzzy_skin_point_distance",
                                         "fuzzy_skin_first_layer",
                                         "fuzzy_skin_noise_type",
                                         "fuzzy_skin_mode",
                                         "fuzzy_skin_scale",
                                         "fuzzy_skin_octaves",
                                         "fuzzy_skin_persistence",
                                         "fuzzy_skin_top_surface",
                                         "fuzzy_skin_lower_surface",
                                         "fuzzy_skin_top_surface_first_layer",
                                         "fuzzy_skin_bed_surface",
                                         "fuzzy_skin_connect_walls",
                                         "fuzzy_skin_compensate_extrusion",
                                         "fuzzy_skin_bridge_compensation_multiplier",
                                         "fuzzy_skin_min_support_distance",
                                         "fuzzy_skin_ironing"};

    const boost::filesystem::path output = boost::filesystem::temp_directory_path() /
                                           boost::filesystem::unique_path("fuzzy-settings-%%%%-%%%%.3mf");
    REQUIRE(store_3mf(output.string().c_str(), &source, nullptr, false));

    Model loaded;
    DynamicPrintConfig loaded_config;
    ConfigSubstitutionContext context {ForwardCompatibilitySubstitutionRule::Disable};
    REQUIRE(load_3mf(output.string().c_str(), loaded_config, context, &loaded, false));
    boost::filesystem::remove(output);

    REQUIRE(loaded.objects.size() == 1);
    REQUIRE(loaded.objects.front()->volumes.size() == 1);
    const ModelConfig &loaded_volume_config = loaded.objects.front()->volumes.front()->config;
    for (const std::string &key : keys) {
        INFO(key);
        REQUIRE(loaded_volume_config.has(key));
        REQUIRE(loaded_volume_config.opt_serialize(key) == config.opt_serialize(key));
    }
}

SCENARIO("2D convex hull of sinking object", "[3mf]") {
    GIVEN("model") {
        // load a model
        Model model;
        std::string src_file = std::string(TEST_DATA_DIR) + "/test_3mf/Prusa.stl";
        load_stl(src_file.c_str(), &model);
        model.add_default_instances();

        WHEN("model is rotated, scaled and set as sinking") {
            ModelObject* object = model.objects.front();
            object->center_around_origin(false);

            // set instance's attitude so that it is rotated, scaled and sinking
            ModelInstance* instance = object->instances.front();
            instance->set_rotation(X, -M_PI / 4.0);
            instance->set_offset(Vec3d::Zero());
            instance->set_scaling_factor({ 2.0, 2.0, 2.0 });

            // calculate 2D convex hull
            Polygon hull_2d = object->convex_hull_2d(instance->get_transformation().get_matrix());

            // verify result
            Points result = {
                { -91501495, -15914144 },
                { 91501495, -15914144 },
                { 91501495, 13792823 },
                { 34846496, 14717717 },
                { -85501495, 13917981 },
                { -91501495, 13792823 }
            };

            // Allow 1um error due to floating point rounding.
            bool res = hull_2d.points.size() == result.size();
            if (res)
                for (size_t i = 0; i < result.size(); ++ i) {
                    const Point &p1 = result[i];
                    const Point &p2 = hull_2d.points[i];
                    if (std::abs(p1.x() - p2.x()) > 1 || std::abs(p1.y() - p2.y()) > 1) {
                        res = false;
                        break;
                    }
                }
            THEN("2D convex hull should match with reference") {
                REQUIRE(res);
            }
        }
    }
}
