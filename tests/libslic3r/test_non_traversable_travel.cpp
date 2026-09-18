#include <catch2/catch_test_macros.hpp>

#include "libslic3r/ClipperUtils.hpp"
#include "libslic3r/Exception.hpp"
#include "libslic3r/Format/3mf.hpp"
#include "libslic3r/GCode/NonTraversableTravel.hpp"
#include "libslic3r/GCodeWriter.hpp"
#include "libslic3r/Model.hpp"
#include "libslic3r/Print.hpp"
#include "libslic3r/TriangleMesh.hpp"

#include <boost/filesystem/operations.hpp>

#include <algorithm>

using namespace Slic3r;

namespace {

Point mm(double x, double y) { return Point::new_scale(x, y); }

ExPolygons square_keepout(double radius)
{
    return union_ex(Polygons{Polygon{{mm(-radius, -radius), mm(radius, -radius), mm(radius, radius), mm(-radius, radius)}}});
}

} // namespace

TEST_CASE("Non-traversable volume stores live-all and explicit extruder rules", "[NonTraversableTravel][Model]")
{
    Model        model;
    ModelObject *object = model.add_object();
    ModelVolume *volume = object->add_volume(make_cube(10., 10., 10.), ModelVolumeType::NON_TRAVERSABLE_SPACE);
    ModelVolume *part   = object->add_volume(make_cube(5., 5., 5.));

    CHECK(volume->is_non_traversable());
    CHECK_FALSE(part->blocks_all_extruders());
    CHECK_FALSE(part->blocks_extruder(0));
    CHECK(volume->blocks_all_extruders());
    CHECK(volume->blocks_extruder(0));
    CHECK(volume->blocks_extruder(15));

    volume->set_blocked_extruders({0, 2, 2});
    CHECK_FALSE(volume->blocks_all_extruders());
    CHECK(volume->blocked_extruders() == std::vector<unsigned int>{0, 2});
    CHECK(volume->blocks_extruder(0));
    CHECK_FALSE(volume->blocks_extruder(1));
    CHECK(volume->blocks_extruder(2));

    volume->set_blocked_extruders({});
    CHECK(volume->blocks_all_extruders());
}

TEST_CASE("Non-traversable volume type has a stable project string", "[NonTraversableTravel][Model]")
{
    CHECK(ModelVolume::type_to_string(ModelVolumeType::NON_TRAVERSABLE_SPACE) == "non_traversable_space");
    CHECK(ModelVolume::type_from_string("non_traversable_space") == ModelVolumeType::NON_TRAVERSABLE_SPACE);
}

TEST_CASE("Non-traversable project data survives a 3MF round trip", "[NonTraversableTravel][3mf]")
{
    Model        source;
    ModelObject *object = source.add_object();
    object->name = "keepout-test";
    object->add_volume(make_cube(20., 20., 10.));
    ModelVolume *keepout = object->add_volume(make_cube(5., 5., 10.), ModelVolumeType::NON_TRAVERSABLE_SPACE);
    keepout->name = "protected bore";
    keepout->set_blocked_extruders({0, 2});
    object->add_instance();

    const boost::filesystem::path path = boost::filesystem::temp_directory_path() /
                                         boost::filesystem::unique_path("non-traversable-%%%%-%%%%.3mf");
    REQUIRE(store_3mf(path.string().c_str(), &source, nullptr, false));

    Model              restored;
    DynamicPrintConfig config;
    ConfigSubstitutionContext substitutions{ForwardCompatibilitySubstitutionRule::Disable};
    REQUIRE(load_3mf(path.string().c_str(), config, substitutions, &restored, false));
    boost::filesystem::remove(path);

    REQUIRE(restored.objects.size() == 1);
    const auto found = std::find_if(restored.objects.front()->volumes.begin(), restored.objects.front()->volumes.end(),
                                    [](const ModelVolume *volume) { return volume->is_non_traversable(); });
    REQUIRE(found != restored.objects.front()->volumes.end());
    CHECK((*found)->name == "protected bore");
    CHECK_FALSE((*found)->blocks_all_extruders());
    CHECK((*found)->blocked_extruders() == std::vector<unsigned int>{0, 2});
}

TEST_CASE("Non-traversable travel remains direct when clear and detours when blocked", "[NonTraversableTravel][Routing]")
{
    const ExPolygons keepout = square_keepout(1.);

    const auto clear = NonTraversableTravelPlanner::route_around(mm(-3., 2.), mm(3., 2.), keepout);
    REQUIRE(clear);
    CHECK(clear->points.size() == 2);

    const auto detour = NonTraversableTravelPlanner::route_around(mm(-3., 0.), mm(3., 0.), keepout);
    REQUIRE(detour);
    CHECK(detour->points.size() > 2);
    CHECK(detour->first_point() == mm(-3., 0.));
    CHECK(detour->last_point() == mm(3., 0.));
    CHECK(detour->length() > (mm(3., 0.) - mm(-3., 0.)).cast<double>().norm());

    Polyline quantized = *detour;
    for (Point &point : quantized.points) {
        const Vec2d as_mm = unscaled(point).cast<double>();
        point = Point::new_scale(GCodeFormatter::quantize_xyzf(as_mm.x()), GCodeFormatter::quantize_xyzf(as_mm.y()));
    }
    for (const Line &line : quantized.lines()) {
        const auto segment = NonTraversableTravelPlanner::route_around(line.a, line.b, keepout);
        REQUIRE(segment);
        CHECK(segment->points.size() == 2);
    }
}

TEST_CASE("Non-traversable travel rejects forbidden endpoints and enclosed routes", "[NonTraversableTravel][Routing]")
{
    const ExPolygons keepout = square_keepout(1.);
    CHECK_FALSE(NonTraversableTravelPlanner::route_around(mm(0., 0.), mm(3., 0.), keepout));
    CHECK_FALSE(NonTraversableTravelPlanner::route_around(mm(0., 0.), mm(0., 0.), keepout));

    ExPolygon ring(Polygon{{mm(-2., -2.), mm(2., -2.), mm(2., 2.), mm(-2., 2.)}});
    ring.holes.emplace_back(Polygon{{mm(-1., -1.), mm(-1., 1.), mm(1., 1.), mm(1., -1.)}});
    CHECK_FALSE(NonTraversableTravelPlanner::route_around(mm(0., 0.), mm(3., 0.), ExPolygons{std::move(ring)}));
}

TEST_CASE("Non-traversable travel preserves unknown-position XY-before-Z ordering", "[NonTraversableTravel][GCodeWriter]")
{
    GCodeWriter writer;
    writer.apply_print_config(static_cast<const PrintConfig &>(FullPrintConfig::defaults()));
    writer.set_position(Vec3d(0., 0., 1.));
    writer.set_current_position_clear(false);

    CHECK(writer.planned_travel_z(2., false, true) == 1.);
    CHECK(writer.planned_destination_z(2., false, true) == 2.);

    writer.set_current_position_clear(true);
    CHECK(writer.planned_travel_z(2., false, true) == 2.);
}

TEST_CASE("Non-traversable travel uses transformed instances, height, and active extruder", "[NonTraversableTravel][Routing][Print]")
{
    Model        model;
    ModelObject *object = model.add_object();
    object->name = "transformed keepout";
    object->add_volume(make_cube(20., 20., 5.));
    ModelVolume *keepout = object->add_volume(make_cube(2., 2., 8.), ModelVolumeType::NON_TRAVERSABLE_SPACE);
    keepout->set_offset(Vec3d(3., -2., 0.));
    keepout->set_blocked_extruders({0});

    ModelInstance *first = object->add_instance();
    first->set_offset(Vec3d(40., 40., 0.));
    ModelInstance *second = object->add_instance();
    second->set_offset(Vec3d(70., 40., 0.));
    object->ensure_on_bed();

    Print print;
    print.set_status_silent();
    DynamicPrintConfig config = DynamicPrintConfig::full_print_config();
    print.apply(model, config);
    REQUIRE(print.objects().size() == 1);
    REQUIRE(print.objects().front()->instances().size() == 2);

    NonTraversableTravelPlanner planner;
    planner.initialize(print);
    CHECK(planner.has_obstacles_for(0));
    CHECK_FALSE(planner.has_obstacles_for(1));

    const PrintObject *print_object = print.objects().front();
    const Vec3d local_center = print_object->trafo_centered() * keepout->get_matrix() * keepout->mesh().bounding_box().center();
    for (const PrintInstance &instance : print_object->instances()) {
        const Vec2d center = local_center.head<2>() + unscaled(instance.shift).cast<double>();
        const Point start  = Point::new_scale(center.x() - 4., center.y());
        const Point end    = Point::new_scale(center.x() + 4., center.y());

        const Polyline blocked = planner.route(Polyline{start, end}, local_center.z(), 0);
        CHECK(blocked.points.size() > 2);

        const Point    center_point = Point::new_scale(center.x(), center.y());
        const Polyline soft_waypoint_inside = planner.route(Polyline{start, center_point, end}, local_center.z(), 0);
        CHECK(soft_waypoint_inside.points.size() > 2);

        const Polyline other_extruder = planner.route(Polyline{start, end}, local_center.z(), 1);
        CHECK(other_extruder.points.size() == 2);

        const Polyline above = planner.route(Polyline{start, end}, local_center.z() + 10., 0);
        CHECK(above.points.size() == 2);

        CHECK_THROWS_AS(planner.validate_vertical(center_point, local_center.z() - 10., local_center.z() + 10., 0), SlicingError);
        const Point near_quantized_edge = Point::new_scale(center.x() + 1.0005, center.y());
        CHECK_THROWS_AS(planner.validate_vertical(near_quantized_edge, local_center.z() - 10., local_center.z() + 10., 0),
                        SlicingError);
        CHECK_NOTHROW(planner.validate_vertical(center_point, local_center.z() - 10., local_center.z() + 10., 1));
    }
}
