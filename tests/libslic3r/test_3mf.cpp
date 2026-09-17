#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "libslic3r/Model.hpp"
#include "libslic3r/Preset.hpp"
#include "libslic3r/Format/3mf.hpp"
#include "libslic3r/Format/bbs_3mf.hpp"
#include "libslic3r/Format/STL.hpp"

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

TEST_CASE("Thermal surface paint survives a 3MF round trip", "[3mf][thermal_pattern]")
{
    Model source;
    const std::string source_file = std::string(TEST_DATA_DIR) + "/test_3mf/Prusa.stl";
    load_stl(source_file.c_str(), &source);
    source.add_default_instances();
    ModelVolume *source_volume = source.objects.front()->volumes.front();
    source_volume->thermal_pattern_facets.reserve(source_volume->mesh().facets_count());
    source_volume->thermal_pattern_facets.set_triangle_from_string(0, "1");
    source_volume->thermal_pattern_facets.shrink_to_fit();

    const boost::filesystem::path output = boost::filesystem::temp_directory_path() /
                                           boost::filesystem::unique_path("thermal-pattern-%%%%-%%%%.3mf");
    REQUIRE(store_3mf(output.string().c_str(), &source, nullptr, false));

    Model loaded;
    DynamicPrintConfig config;
    ConfigSubstitutionContext context {ForwardCompatibilitySubstitutionRule::Disable};
    REQUIRE(load_3mf(output.string().c_str(), config, context, &loaded, false));
    boost::filesystem::remove(output);

    REQUIRE(loaded.objects.size() == 1);
    REQUIRE(loaded.objects.front()->volumes.size() == 1);
    const FacetsAnnotation &loaded_paint = loaded.objects.front()->volumes.front()->thermal_pattern_facets;
    REQUIRE_FALSE(loaded_paint.empty());
    REQUIRE(loaded_paint.get_triangle_as_string(0) == "1");
}

TEST_CASE("Thermal surface paint survives a project 3MF round trip", "[3mf][thermal_pattern][bbs_3mf]")
{
    Model source;
    const std::string source_file = std::string(TEST_DATA_DIR) + "/test_3mf/Prusa.stl";
    load_stl(source_file.c_str(), &source);
    source.add_default_instances();
    ModelVolume *source_volume = source.objects.front()->volumes.front();
    source_volume->thermal_pattern_facets.reserve(source_volume->mesh().facets_count());
    source_volume->thermal_pattern_facets.set_triangle_from_string(0, "1");
    source_volume->thermal_pattern_facets.shrink_to_fit();
    source_volume->fuzzy_skin_facets.reserve(source_volume->mesh().facets_count());
    source_volume->fuzzy_skin_facets.set_triangle_from_string(0, "1");
    source_volume->fuzzy_skin_facets.shrink_to_fit();

    const boost::filesystem::path output = boost::filesystem::temp_directory_path() /
                                           boost::filesystem::unique_path("thermal-pattern-project-%%%%-%%%%.3mf");
    const boost::filesystem::path backup = boost::filesystem::temp_directory_path() /
                                           boost::filesystem::unique_path("thermal-pattern-project-backup-%%%%-%%%%");
    source.set_backup_path(backup.string());
    const std::string output_path = output.string();
    DynamicPrintConfig source_config;
    source_config.set_key_value("thermal_pattern_mode",
                                new ConfigOptionEnum<ThermalPatternMode>(ThermalPatternMode::PaintedSurfaces));
    source_config.set_key_value("thermal_pattern_seed", new ConfigOptionInt(4242));
    source_config.set_key_value("thermal_pattern_enabled", new ConfigOptionBools {true});
    source_config.set_key_value("thermal_pattern_temperature_step", new ConfigOptionFloats {11.5});
    source_config.set_key_value("thermal_pattern_max_temperature", new ConfigOptionInts {287});
    StoreParams store_params;
    store_params.path = output_path.c_str();
    store_params.model = &source;
    store_params.config = &source_config;
    const bool stored = store_bbs_3mf(store_params);
    source.remove_backup_path_if_exist();
    REQUIRE(stored);

    Model loaded;
    const boost::filesystem::path load_backup = boost::filesystem::temp_directory_path() /
                                                boost::filesystem::unique_path("thermal-pattern-project-load-%%%%-%%%%");
    loaded.set_backup_path(load_backup.string());
    DynamicPrintConfig loaded_config;
    ConfigSubstitutionContext context {ForwardCompatibilitySubstitutionRule::Disable};
    PlateDataPtrs plate_data;
    std::vector<Preset *> project_presets;
    bool is_bbl_3mf = false;
    Semver file_version;
    const LoadStrategy load_strategy = LoadStrategy::LoadModel | LoadStrategy::LoadConfig |
                                       LoadStrategy::AddDefaultInstances;
    const bool loaded_ok = load_bbs_3mf(output_path.c_str(), &loaded_config, &context, &loaded, &plate_data,
                                        &project_presets, &is_bbl_3mf, &file_version, nullptr, load_strategy);
    loaded.remove_backup_path_if_exist();
    REQUIRE(loaded_ok);
    release_PlateData_list(plate_data);
    for (Preset *preset : project_presets)
        delete preset;
    boost::filesystem::remove(output);

    REQUIRE(loaded.objects.size() == 1);
    REQUIRE(loaded.objects.front()->volumes.size() == 1);
    const FacetsAnnotation &loaded_paint = loaded.objects.front()->volumes.front()->thermal_pattern_facets;
    REQUIRE_FALSE(loaded_paint.empty());
    REQUIRE(loaded_paint.get_triangle_as_string(0) == "1");
    const FacetsAnnotation &loaded_fuzzy_paint = loaded.objects.front()->volumes.front()->fuzzy_skin_facets;
    REQUIRE_FALSE(loaded_fuzzy_paint.empty());
    REQUIRE(loaded_fuzzy_paint.get_triangle_as_string(0) == "1");
    REQUIRE(loaded_config.opt_enum<ThermalPatternMode>("thermal_pattern_mode") == ThermalPatternMode::PaintedSurfaces);
    REQUIRE(loaded_config.opt_int("thermal_pattern_seed") == 4242);
    REQUIRE(loaded_config.opt_bool("thermal_pattern_enabled", 0));
    REQUIRE(loaded_config.opt_float("thermal_pattern_temperature_step", 0) == Catch::Approx(11.5));
    REQUIRE(loaded_config.opt_int("thermal_pattern_max_temperature", 0) == 287);
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
