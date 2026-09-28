#ifndef slic3r_FuzzySkinCalibration_hpp_
#define slic3r_FuzzySkinCalibration_hpp_

#include <cstddef>
#include <string>
#include <vector>

#include "TriangleMesh.hpp"

namespace Slic3r {

class DynamicPrintConfig;
class ModelObject;

enum class FuzzySkinCalibrationMode { TextureMatrix, IroningComparison, SupportedUnderside, CubeSingle, CubeSeries, CubeOrthogonal };

enum class FuzzySkinCalibrationLayout { ConnectedPanel, BreakawayCoupons };

struct FuzzySkinCalibrationRange
{
    double minimum{0.05};
    double maximum{0.5};
    double step{0.15};
};

struct FuzzySkinCalibrationConfig
{
    FuzzySkinCalibrationMode   mode{FuzzySkinCalibrationMode::TextureMatrix};
    FuzzySkinCalibrationLayout layout{FuzzySkinCalibrationLayout::ConnectedPanel};
    FuzzySkinCalibrationRange  thickness;
    FuzzySkinCalibrationRange  distance{0.2, 2.0, 0.6};
    double                     coupon_width{25.0};
    double                     coupon_depth{25.0};
    double                     coupon_height{2.0};
    double                     gap{2.0};
    double                     label_min_stroke{0.5};
    bool                       labels{true};
    bool                       label_pedestal{false};
};

struct FuzzySkinCalibrationCell
{
    double      thickness{0.0};
    double      distance{0.0};
    bool        fuzzy_ironing{false};
    size_t      row{0};
    size_t      column{0};
    std::string label;
};

struct FuzzySkinCalibrationPlan
{
    size_t                                rows{0};
    size_t                                columns{0};
    bool                                  shared_object{false};
    std::vector<FuzzySkinCalibrationCell> cells;
};

struct FuzzySkinCalibrationGrid
{
    double width{0.0};
    double depth{0.0};
    double pitch_x{0.0};
    double pitch_y{0.0};
    double edge_clearance{0.0};
    size_t rows{0};
    size_t columns{0};

    Vec2d cell_center(const FuzzySkinCalibrationCell& cell) const;
};

FuzzySkinCalibrationGrid build_fuzzy_skin_calibration_grid(const FuzzySkinCalibrationConfig& config, const FuzzySkinCalibrationPlan& plan);

struct FuzzySkinCalibrationBedPatch
{
    double center_x{0.0};
    double center_y{0.0};
    double radius{0.0};
};

std::vector<double>          fuzzy_skin_calibration_values(const FuzzySkinCalibrationRange& range);
std::string                  fuzzy_skin_calibration_value_label(double value);
FuzzySkinCalibrationPlan     build_fuzzy_skin_calibration_plan(const FuzzySkinCalibrationConfig& config);
FuzzySkinCalibrationBedPatch fuzzy_skin_calibration_bed_patch(const FuzzySkinCalibrationConfig& config);
double fuzzy_skin_calibration_bed_thickness(const FuzzySkinCalibrationConfig& config, double thickness, double first_layer_height);
void   apply_fuzzy_skin_calibration_print_config(DynamicPrintConfig& config);

double fuzzy_skin_calibration_cube_overhang(double thickness);

bool is_fuzzy_skin_calibration_cube(FuzzySkinCalibrationMode mode);
void populate_fuzzy_skin_calibration_cube(ModelObject&                      object,
                                          const FuzzySkinCalibrationConfig& config,
                                          const FuzzySkinCalibrationCell&   cell);

TriangleMesh make_fuzzy_skin_calibration_coupon(double width, double depth, double height);
TriangleMesh make_fuzzy_skin_calibration_bridge(double width, double depth, double roof_height, double roof_thickness);
TriangleMesh make_fuzzy_skin_calibration_bed_patch(double radius, double height);
TriangleMesh make_fuzzy_skin_calibration_label(const std::string& text, double glyph_height, double relief, double pedestal_height = 0.0);

// Wrap at spaces/newlines without shrinking below the minimum printable stroke.
// Throws if the supplied box cannot contain the text and its optional pedestal.
TriangleMesh make_fuzzy_skin_calibration_fitted_label(
    const std::string& text, double max_width, double max_height, double relief, double pedestal_height = 0.0, double minimum_stroke = 0.5);

} // namespace Slic3r

#endif // slic3r_FuzzySkinCalibration_hpp_
