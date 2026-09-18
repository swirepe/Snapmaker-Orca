#ifndef slic3r_NonTraversableTravel_hpp_
#define slic3r_NonTraversableTravel_hpp_

#include "libslic3r/ExPolygon.hpp"
#include "libslic3r/Polyline.hpp"
#include "libslic3r/TriangleMesh.hpp"
#include "libslic3r/AABBTreeIndirect.hpp"

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Slic3r {

class Print;

// Routes slicer-planned travel moves around user supplied three-dimensional
// keep-out volumes. Coordinates accepted and returned by this class are in the
// global print coordinate system, not GCode's per-instance local coordinates.
class NonTraversableTravelPlanner
{
public:
    void initialize(const Print &print);
    void clear();

    bool has_obstacles_for(unsigned int extruder_id) const;

    Polyline route(const Polyline &travel, double z, unsigned int extruder_id);
    void     validate_vertical(const Point &point, double from_z, double to_z, unsigned int extruder_id) const;

    // Public for deterministic unit testing of the geometry kernel. A null
    // result means that an endpoint is forbidden or no path exists.
    static std::optional<Polyline> route_around(const Point &start, const Point &end, const ExPolygons &keepouts);

private:
    struct Obstacle {
        indexed_triangle_set          mesh;
        AABBTreeIndirect::Tree3f       tree;
        BoundingBoxf3                  bbox;
        std::string                    name;
        bool                           all_extruders { true };
        std::vector<unsigned int>      extruders;

        bool blocks(unsigned int extruder_id) const;
    };

    using SliceKey = std::pair<coord_t, unsigned int>;

    const ExPolygons &slice(double z, unsigned int extruder_id);
    std::string       active_obstacle_names(double z, unsigned int extruder_id) const;
    [[noreturn]] void throw_route_error(const char *reason, const Point &point, double z, unsigned int extruder_id) const;

    std::vector<Obstacle>          m_obstacles;
    std::map<SliceKey, ExPolygons> m_slices;
};

} // namespace Slic3r

#endif // slic3r_NonTraversableTravel_hpp_
