#ifndef slic3r_FuzzySkinCalibration_hpp_
#define slic3r_FuzzySkinCalibration_hpp_

#include <cstddef>
#include <string>
#include <vector>

#include "TriangleMesh.hpp"

namespace Slic3r {

enum class FuzzySkinCalibrationMode { TextureMatrix, IroningComparison, SupportedUnderside };

enum class FuzzySkinCalibrationLayout { ConnectedPanel, BreakawayCoupons };

struct FuzzySkinCalibrationRange
{
    double minimum{0.1};
    double maximum{0.4};
    double step{0.1};
};

struct FuzzySkinCalibrationConfig
{
    FuzzySkinCalibrationMode   mode{FuzzySkinCalibrationMode::TextureMatrix};
    FuzzySkinCalibrationLayout layout{FuzzySkinCalibrationLayout::ConnectedPanel};
    FuzzySkinCalibrationRange  thickness;
    FuzzySkinCalibrationRange  distance{0.2, 0.8, 0.2};
    double                     coupon_width{20.0};
    double                     coupon_depth{20.0};
    double                     coupon_height{2.0};
    double                     gap{2.0};
    bool                       labels{true};
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
    std::vector<FuzzySkinCalibrationCell> cells;
};

std::vector<double>      fuzzy_skin_calibration_values(const FuzzySkinCalibrationRange& range);
FuzzySkinCalibrationPlan build_fuzzy_skin_calibration_plan(const FuzzySkinCalibrationConfig& config);

TriangleMesh make_fuzzy_skin_calibration_coupon(double width, double depth, double height);
TriangleMesh make_fuzzy_skin_calibration_bridge(double width, double depth, double roof_height, double roof_thickness);
TriangleMesh make_fuzzy_skin_calibration_label(const std::string& text, double glyph_height, double relief);

} // namespace Slic3r

#endif // slic3r_FuzzySkinCalibration_hpp_
