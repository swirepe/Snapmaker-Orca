#ifndef slic3r_PaneCalibration_hpp_
#define slic3r_PaneCalibration_hpp_

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "TriangleMesh.hpp"

namespace Slic3r {

enum class PaneCalibrationTool {
    ClearFilament,
    Ironing,
};

enum class PaneCalibrationDesign {
    Linear,
    Grid,
    Taguchi,
};

enum class PaneCalibrationFactor {
    NozzleTemperature,
    PrintSpeed,
    FlowRatio,
    LayerHeight,
    MaxFanSpeed,
    WallFanSpeed,
    IroningFanSpeed,
    AuxiliaryFanSpeed,
    IroningType,
    IroningFlow,
    IroningAngle,
    LineWidth,
    IroningSpeed,
    IroningSpacing,
};

struct PaneCalibrationFactorSetting
{
    PaneCalibrationFactor factor {PaneCalibrationFactor::NozzleTemperature};
    bool                  enabled {false};
    double                minimum {0.};
    double                maximum {0.};
    unsigned              levels {4};
};

struct PaneCalibrationConfig
{
    PaneCalibrationTool   tool {PaneCalibrationTool::ClearFilament};
    PaneCalibrationDesign design {PaneCalibrationDesign::Taguchi};
    unsigned              taguchi_levels {4};

    std::vector<PaneCalibrationFactorSetting> factors;

    double pane_width {30.};
    double pane_depth {30.};
    double pane_height {1.};
    double pane_gap {5.};
    bool   mouse_ears {false};
    double mouse_ear_diameter {5.};

    bool   labels {false};
    double label_glyph_height {2.};
    double label_relief {2.};
    int    pane_extruder {1};
    int    label_extruder {1};
};

struct PaneCalibrationValue
{
    PaneCalibrationFactor factor {PaneCalibrationFactor::NozzleTemperature};
    double                value {0.};
    unsigned              level {0};
    unsigned              levels {4};
};

using PaneCalibrationRow = std::vector<PaneCalibrationValue>;

struct PaneCalibrationPlan
{
    std::string                    array_name;
    std::vector<PaneCalibrationRow> rows;
};

std::vector<double> pane_calibration_linear_values(double minimum, double maximum, unsigned levels);

// Returns an orthogonal array containing zero-based level indices. The smallest
// supported array with enough columns is selected. Throws std::invalid_argument
// when the level or column count is unsupported.
std::pair<std::string, std::vector<std::vector<unsigned>>>
pane_calibration_taguchi_array(unsigned levels, size_t columns);

PaneCalibrationPlan build_pane_calibration_plan(const PaneCalibrationConfig &config);

std::string pane_calibration_factor_key(PaneCalibrationFactor factor);
std::string pane_calibration_format_value(PaneCalibrationFactor factor, double value, unsigned experiment_levels);
std::vector<std::string> pane_calibration_label_lines(const PaneCalibrationRow &row, unsigned experiment_levels,
                                                      size_t maximum_characters_per_line);

PaneCalibrationConfig default_pane_calibration_config(PaneCalibrationTool tool);

// Meshes are centered on the local XY origin. Label geometry starts at Z=0 so
// callers may keep it as an independently editable volume and translate it to
// the pane's top surface.
TriangleMesh make_pane_calibration_body(const PaneCalibrationConfig &config, double first_layer_height);
TriangleMesh make_pane_calibration_label(const std::vector<std::string> &lines, double glyph_height, double relief,
                                         double pane_width, double pane_depth);

} // namespace Slic3r

#endif // slic3r_PaneCalibration_hpp_
