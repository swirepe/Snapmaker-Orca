#include "NonTraversableTravel.hpp"

#include "libslic3r/AABBTreeLines.hpp"
#include "libslic3r/ClipperUtils.hpp"
#include "libslic3r/Exception.hpp"
#include "libslic3r/Model.hpp"
#include "libslic3r/Print.hpp"
#include "libslic3r/TriangleMeshSlicer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <queue>
#include <sstream>

namespace Slic3r {

namespace {

// XY coordinates are emitted with three decimal places. These margins keep
// rounding from moving an otherwise legal route back onto the exact boundary.
constexpr double GCODE_XY_QUANTIZATION = 0.001;

bool contains(const ExPolygons &polygons, const Point &point)
{
    return std::any_of(polygons.begin(), polygons.end(), [&point](const ExPolygon &polygon) { return polygon.contains(point, true); });
}

Points unique_points(Points points)
{
    std::sort(points.begin(), points.end(), [](const Point &lhs, const Point &rhs) {
        return lhs.x() < rhs.x() || (lhs.x() == rhs.x() && lhs.y() < rhs.y());
    });
    points.erase(std::unique(points.begin(), points.end()), points.end());
    return points;
}

} // namespace

bool NonTraversableTravelPlanner::Obstacle::blocks(unsigned int extruder_id) const
{
    return all_extruders || std::find(extruders.begin(), extruders.end(), extruder_id) != extruders.end();
}

void NonTraversableTravelPlanner::clear()
{
    m_obstacles.clear();
    m_slices.clear();
}

void NonTraversableTravelPlanner::initialize(const Print &print)
{
    this->clear();

    for (const PrintObject *print_object : print.objects()) {
        const Transform3d object_transform = print_object->trafo_centered();
        for (const ModelVolume *volume : print_object->model_object()->volumes) {
            if (!volume->is_non_traversable() || volume->mesh().empty())
                continue;

            indexed_triangle_set transformed = volume->mesh().its;
            its_transform(transformed, object_transform * volume->get_matrix(), true);

            for (const PrintInstance &instance : print_object->instances()) {
                Obstacle obstacle;
                obstacle.mesh           = transformed;
                obstacle.name           = volume->name.empty() ? print_object->model_object()->name : volume->name;
                if (obstacle.name.empty())
                    obstacle.name = "unnamed non-traversable space";
                obstacle.all_extruders = volume->blocks_all_extruders();
                obstacle.extruders      = volume->blocked_extruders();

                const Vec3f shift(float(unscale<double>(instance.shift.x())), float(unscale<double>(instance.shift.y())), 0.f);
                for (Vec3f &vertex : obstacle.mesh.vertices)
                    vertex += shift;

                obstacle.bbox = bounding_box(obstacle.mesh);
                obstacle.tree = AABBTreeIndirect::build_aabb_tree_over_indexed_triangle_set(obstacle.mesh.vertices, obstacle.mesh.indices);
                m_obstacles.emplace_back(std::move(obstacle));
            }
        }
    }
}

bool NonTraversableTravelPlanner::has_obstacles_for(unsigned int extruder_id) const
{
    return std::any_of(m_obstacles.begin(), m_obstacles.end(),
                       [extruder_id](const Obstacle &obstacle) { return obstacle.blocks(extruder_id); });
}

const ExPolygons &NonTraversableTravelPlanner::slice(double z, unsigned int extruder_id)
{
    const SliceKey key{scaled<coord_t>(z), extruder_id};
    if (auto found = m_slices.find(key); found != m_slices.end())
        return found->second;

    ExPolygons collected;
    const float planes[] = {float(z - EPSILON), float(z), float(z + EPSILON)};
    for (const Obstacle &obstacle : m_obstacles) {
        if (!obstacle.blocks(extruder_id) || z < obstacle.bbox.min.z() - EPSILON || z > obstacle.bbox.max.z() + EPSILON)
            continue;

        std::vector<float> zs;
        for (float plane : planes)
            if (plane >= obstacle.bbox.min.z() - float(EPSILON) && plane <= obstacle.bbox.max.z() + float(EPSILON))
                zs.emplace_back(plane);

        if (!zs.empty()) {
            std::vector<ExPolygons> sliced = slice_mesh_ex(obstacle.mesh, zs);
            for (ExPolygons &polygons : sliced)
                append(collected, std::move(polygons));
        }
    }

    ExPolygons merged;
    if (!collected.empty())
        merged = union_ex(collected);
    return m_slices.emplace(key, std::move(merged)).first->second;
}

std::optional<Polyline> NonTraversableTravelPlanner::route_around(const Point &start, const Point &end, const ExPolygons &keepouts)
{
    if (keepouts.empty())
        return Polyline{start, end};

    // The forbidden margin accounts for polygon arithmetic and G-code
    // quantization. Route vertices use twice that numerical margin.
    const float      collision_margin = float(scale_(GCODE_XY_QUANTIZATION));
    const ExPolygons forbidden        = offset_ex(keepouts, collision_margin);
    if (forbidden.empty())
        return Polyline{start, end};
    if (contains(forbidden, start) || contains(forbidden, end))
        return std::nullopt;
    if (start == end)
        return Polyline{start, end};

    const Lines boundary_lines = to_lines(forbidden);
    const AABBTreeLines::LinesDistancer<Line> boundary(boundary_lines);
    const auto segment_is_clear = [&boundary](const Point &a, const Point &b) {
        return a == b || boundary.intersections_with_line<false>(Line(a, b)).empty();
    };

    if (segment_is_clear(start, end))
        return Polyline{start, end};

    const ExPolygons routing_boundary = offset_ex(keepouts, 2.f * collision_margin);
    Points           vertices;
    for (const ExPolygon &polygon : routing_boundary) {
        append(vertices, polygon.contour.points);
        for (const Polygon &hole : polygon.holes)
            append(vertices, hole.points);
    }
    vertices = unique_points(std::move(vertices));

    Points nodes;
    nodes.reserve(vertices.size() + 2);
    nodes.emplace_back(start);
    nodes.emplace_back(end);
    for (const Point &point : vertices)
        if (!contains(forbidden, point))
            nodes.emplace_back(point);

    if (nodes.size() == 2)
        return std::nullopt;

    const double infinity = std::numeric_limits<double>::infinity();
    std::vector<double> distances(nodes.size(), infinity);
    std::vector<size_t> predecessors(nodes.size(), size_t(-1));
    std::vector<bool>   visited(nodes.size(), false);
    using QueueEntry = std::pair<double, size_t>;
    std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<QueueEntry>> queue;

    distances[0] = 0.;
    queue.emplace(0., 0);
    while (!queue.empty()) {
        const auto [distance, current] = queue.top();
        queue.pop();
        if (visited[current])
            continue;
        visited[current] = true;
        if (current == 1)
            break;

        for (size_t next = 0; next < nodes.size(); ++next) {
            if (next == current || visited[next] || !segment_is_clear(nodes[current], nodes[next]))
                continue;
            const double candidate = distance + (nodes[next] - nodes[current]).cast<double>().norm();
            if (candidate < distances[next]) {
                distances[next]    = candidate;
                predecessors[next] = current;
                queue.emplace(candidate, next);
            }
        }
    }

    if (!std::isfinite(distances[1]))
        return std::nullopt;

    Points reversed;
    for (size_t node = 1; node != size_t(-1); node = predecessors[node]) {
        reversed.emplace_back(nodes[node]);
        if (node == 0)
            break;
    }
    if (reversed.empty() || reversed.back() != start)
        return std::nullopt;
    std::reverse(reversed.begin(), reversed.end());

    Polyline result(std::move(reversed));
    for (const Line &line : result.lines())
        if (!segment_is_clear(line.a, line.b))
            return std::nullopt;
    return result;
}

Polyline NonTraversableTravelPlanner::route(const Polyline &travel, double z, unsigned int extruder_id)
{
    if (travel.size() < 2 || !this->has_obstacles_for(extruder_id))
        return travel;

    const ExPolygons &keepouts = this->slice(z, extruder_id);
    if (keepouts.empty())
        return travel;

    if (contains(keepouts, travel.first_point()))
        this->throw_route_error("the travel starts inside", travel.first_point(), z, extruder_id);
    if (contains(keepouts, travel.last_point()))
        this->throw_route_error("the travel destination is inside", travel.last_point(), z, extruder_id);

    Polyline result;
    result.points.reserve(travel.points.size());
    result.points.emplace_back(travel.first_point());
    for (size_t index = 1; index < travel.points.size(); ++index) {
        std::optional<Polyline> segment = route_around(result.last_point(), travel.points[index], keepouts);
        if (!segment && index + 1 < travel.points.size())
            continue; // Hard keep-outs take precedence over optional soft-planner waypoints.
        if (!segment)
            this->throw_route_error("no legal XY route exists around", travel.points[index], z, extruder_id);
        result.points.insert(result.points.end(), std::next(segment->points.begin()), segment->points.end());
    }
    return result;
}

void NonTraversableTravelPlanner::validate_vertical(const Point &point, double from_z, double to_z, unsigned int extruder_id) const
{
    if (std::abs(from_z - to_z) < EPSILON)
        return;

    const double low         = std::min(from_z, to_z);
    const double high        = std::max(from_z, to_z);
    const Vec3d  base_origin = Vec3d(unscale<double>(point.x()), unscale<double>(point.y()), low - EPSILON);
    const Vec3d  dir         = Vec3d::UnitZ();
    const double length      = high - low + 2. * EPSILON;
    const double q           = GCODE_XY_QUANTIZATION;
    const std::array<Vec2d, 9> quantization_offsets = {
        Vec2d(0., 0.), Vec2d(q, 0.), Vec2d(-q, 0.), Vec2d(0., q), Vec2d(0., -q),
        Vec2d(q, q), Vec2d(q, -q), Vec2d(-q, q), Vec2d(-q, -q)
    };

    for (const Obstacle &obstacle : m_obstacles) {
        if (!obstacle.blocks(extruder_id) || high < obstacle.bbox.min.z() - EPSILON || low > obstacle.bbox.max.z() + EPSILON ||
            base_origin.x() < obstacle.bbox.min.x() - q || base_origin.x() > obstacle.bbox.max.x() + q ||
            base_origin.y() < obstacle.bbox.min.y() - q || base_origin.y() > obstacle.bbox.max.y() + q)
            continue;

        bool intersects = false;
        for (const Vec2d &offset : quantization_offsets) {
            const Vec3d origin = base_origin + Vec3d(offset.x(), offset.y(), 0.);
            std::vector<igl::Hit> hits;
            AABBTreeIndirect::intersect_ray_all_hits(obstacle.mesh.vertices, obstacle.mesh.indices, obstacle.tree, origin, dir, hits);
            if (std::any_of(hits.begin(), hits.end(), [length](const igl::Hit &hit) { return hit.t >= -EPSILON && hit.t <= length; })) {
                intersects = true;
                break;
            }
        }
        if (intersects) {
            std::ostringstream message;
            message << "A vertical travel move for extruder " << (extruder_id + 1) << " would cross non-traversable space '"
                    << obstacle.name << "' at X" << base_origin.x() << " Y" << base_origin.y()
                    << ". Move or resize the non-traversable space, or change its blocked extruders.";
            throw SlicingError(message.str());
        }
    }
}

std::string NonTraversableTravelPlanner::active_obstacle_names(double z, unsigned int extruder_id) const
{
    std::string names;
    for (const Obstacle &obstacle : m_obstacles) {
        if (!obstacle.blocks(extruder_id) || z < obstacle.bbox.min.z() - EPSILON || z > obstacle.bbox.max.z() + EPSILON)
            continue;
        if (!names.empty())
            names += ", ";
        names += "'" + obstacle.name + "'";
    }
    return names.empty() ? "the configured keep-out volume" : names;
}

void NonTraversableTravelPlanner::throw_route_error(const char *reason, const Point &point, double z, unsigned int extruder_id) const
{
    std::ostringstream message;
    message << "Cannot generate a safe travel move for extruder " << (extruder_id + 1) << ": " << reason << " non-traversable space "
            << this->active_obstacle_names(z, extruder_id) << " at Z" << z << " near X" << unscale<double>(point.x()) << " Y"
            << unscale<double>(point.y()) << ". Move or resize the non-traversable space, or change its blocked extruders.";
    throw SlicingError(message.str());
}

} // namespace Slic3r
