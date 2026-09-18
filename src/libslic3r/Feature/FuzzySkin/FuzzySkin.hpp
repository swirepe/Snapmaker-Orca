#ifndef libslic3r_FuzzySkin_hpp_
#define libslic3r_FuzzySkin_hpp_

#include "libslic3r/Arachne/utils/ExtrusionJunction.hpp"
#include "libslic3r/Arachne/utils/ExtrusionLine.hpp"
#include "libslic3r/PerimeterGenerator.hpp"

namespace Slic3r::Feature::FuzzySkin {

enum class FuzzySurfaceType { Top, Lower };

struct FuzzySurfaceConfig
{
    double    point_distance{0.3};
    double    displacement{0.2};
    bool      connect_boundaries{true};
    bool      compensate_extrusion{true};
    double    bridge_compensation_multiplier{3.0};
    NoiseType noise_type{NoiseType::Classic};
    double    noise_scale{1.0};
    int       noise_octaves{4};
    double    noise_persistence{0.5};
};

struct FuzzySurfacePoint
{
    Point  point;
    double z_offset{0.0};
    double extrusion_multiplier{1.0};
};

double fuzzy_surface_noise(const Vec2d& position, coordf_t slice_z, const FuzzySurfaceConfig& config);

double fuzzy_surface_displacement(double thickness, double support_top_z_distance, double minimum_support_distance, FuzzySurfaceType type);

std::vector<FuzzySurfacePoint> fuzzy_surface_points(const Polyline&           polyline,
                                                    coordf_t                  slice_z,
                                                    FuzzySurfaceType          type,
                                                    const FuzzySurfaceConfig& config);

void fuzzy_polyline(Points& poly, bool closed, coordf_t slice_z, const FuzzySkinConfig& cfg);

void fuzzy_extrusion_line(Arachne::ExtrusionJunctions& ext_lines, coordf_t slice_z, const FuzzySkinConfig& cfg);

void group_region_by_fuzzify(PerimeterGenerator& g);

bool should_fuzzify(const FuzzySkinConfig& config, int layer_id, size_t loop_idx, bool is_contour);

Polygon apply_fuzzy_skin(const Polygon& polygon, const PerimeterGenerator& perimeter_generator, size_t loop_idx, bool is_contour);
void    apply_fuzzy_skin(Arachne::ExtrusionLine* extrusion, const PerimeterGenerator& perimeter_generator, bool is_contour);

} // namespace Slic3r::Feature::FuzzySkin

#endif // libslic3r_FuzzySkin_hpp_
