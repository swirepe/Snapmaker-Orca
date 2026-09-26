#include "FuzzySkinCalibration.hpp"

#include "PrintConfig.hpp"
#include "Model.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace Slic3r {

namespace {

void translate(indexed_triangle_set& mesh, float x, float y, float z)
{
    for (Vec3f& vertex : mesh.vertices)
        vertex += Vec3f(x, y, z);
}

const std::array<unsigned char, 7>& glyph(char value)
{
    using Rows = std::array<unsigned char, 7>;
    static const Rows                           blank{0, 0, 0, 0, 0, 0, 0};
    static const std::unordered_map<char, Rows> glyphs{
        {'0', {14, 17, 19, 21, 25, 17, 14}}, {'1', {4, 12, 4, 4, 4, 4, 14}},      {'2', {14, 17, 1, 2, 4, 8, 31}},
        {'3', {30, 1, 1, 14, 1, 1, 30}},     {'4', {2, 6, 10, 18, 31, 2, 2}},     {'5', {31, 16, 16, 30, 1, 1, 30}},
        {'6', {14, 16, 16, 30, 17, 17, 14}}, {'7', {31, 1, 2, 4, 8, 8, 8}},       {'8', {14, 17, 17, 14, 17, 17, 14}},
        {'9', {14, 17, 17, 15, 1, 1, 14}},   {'D', {30, 17, 17, 17, 17, 17, 30}}, {'F', {31, 16, 16, 30, 16, 16, 16}},
        {'I', {14, 4, 4, 4, 4, 4, 14}},      {'T', {31, 4, 4, 4, 4, 4, 4}},       {'=', {0, 0, 31, 0, 31, 0, 0}},
        {'.', {0, 0, 0, 0, 0, 12, 12}},      {'-', {0, 0, 0, 31, 0, 0, 0}},       {' ', {0, 0, 0, 0, 0, 0, 0}},
    };
    const auto found = glyphs.find(value);
    return found == glyphs.end() ? blank : found->second;
}

} // namespace

std::string fuzzy_skin_calibration_value_label(double value)
{
    if (!std::isfinite(value))
        throw std::invalid_argument("Calibration label values must be finite");

    std::ostringstream stream;
    stream << std::fixed << std::setprecision(3) << value;
    std::string text = stream.str();
    while (!text.empty() && text.back() == '0')
        text.pop_back();
    if (!text.empty() && text.back() == '.')
        text.pop_back();
    return text;
}

std::vector<double> fuzzy_skin_calibration_values(const FuzzySkinCalibrationRange& range)
{
    if (!std::isfinite(range.minimum) || !std::isfinite(range.maximum) || !std::isfinite(range.step) || range.minimum <= 0.0 ||
        range.maximum < range.minimum || range.step <= 0.0)
        throw std::invalid_argument("Calibration ranges must be positive and ordered");

    const double span       = range.maximum - range.minimum;
    const double step_count = std::round(span / range.step);
    if (!std::isfinite(step_count) || step_count < 0.0 || step_count > 7.0 ||
        std::abs(range.minimum + step_count * range.step - range.maximum) > 1e-6)
        throw std::invalid_argument("Calibration ranges must contain between one and eight evenly spaced values");
    const size_t count = static_cast<size_t>(step_count) + 1;

    std::vector<double> values;
    values.reserve(count);
    for (size_t index = 0; index < count; ++index)
        values.emplace_back(index + 1 == count ? range.maximum : range.minimum + double(index) * range.step);
    return values;
}

bool is_fuzzy_skin_calibration_cube(FuzzySkinCalibrationMode mode)
{
    return mode == FuzzySkinCalibrationMode::CubeSingle || mode == FuzzySkinCalibrationMode::CubeSeries ||
           mode == FuzzySkinCalibrationMode::CubeOrthogonal;
}

FuzzySkinCalibrationPlan build_fuzzy_skin_calibration_plan(const FuzzySkinCalibrationConfig& config)
{
    if (!std::isfinite(config.coupon_width) || !std::isfinite(config.coupon_depth) || !std::isfinite(config.coupon_height) ||
        !std::isfinite(config.gap) || config.coupon_width < 10.0 || config.coupon_depth < 10.0 || config.coupon_height <= 0.0 ||
        config.gap < 0.0)
        throw std::invalid_argument("Calibration coupons must be at least 10 x 10 mm with a positive height");
    if (config.mode == FuzzySkinCalibrationMode::SupportedUnderside && config.layout == FuzzySkinCalibrationLayout::ConnectedPanel)
        throw std::invalid_argument("Supported-underside calibration requires breakaway coupons");
    if (config.thickness.minimum < 0.001 || config.thickness.maximum > 1.0)
        throw std::invalid_argument("Fuzzy skin calibration thickness must be between 0.001 and 1 mm");
    if (config.distance.minimum < 0.01 || config.distance.maximum > 5.0)
        throw std::invalid_argument("Fuzzy skin calibration point distance must be between 0.01 and 5 mm");

    std::vector<double> thicknesses;
    std::vector<double> distances;
    if (config.mode == FuzzySkinCalibrationMode::CubeSingle) {
        thicknesses = fuzzy_skin_calibration_values({config.thickness.minimum, config.thickness.minimum, 1.0});
        distances   = fuzzy_skin_calibration_values({config.distance.minimum, config.distance.minimum, 1.0});
    } else if (config.mode == FuzzySkinCalibrationMode::CubeOrthogonal) {
        if (config.thickness.minimum >= config.thickness.maximum || config.distance.minimum >= config.distance.maximum)
            throw std::invalid_argument("The L9 cube array requires distinct minimum and maximum values for both factors");
        thicknesses = fuzzy_skin_calibration_values(
            {config.thickness.minimum, config.thickness.maximum, 0.5 * (config.thickness.maximum - config.thickness.minimum)});
        distances = fuzzy_skin_calibration_values(
            {config.distance.minimum, config.distance.maximum, 0.5 * (config.distance.maximum - config.distance.minimum)});
    } else {
        thicknesses = fuzzy_skin_calibration_values(config.thickness);
        distances   = fuzzy_skin_calibration_values(config.distance);
    }
    FuzzySkinCalibrationPlan  plan;
    plan.shared_object = config.mode == FuzzySkinCalibrationMode::SupportedUnderside;

    if (config.mode == FuzzySkinCalibrationMode::IroningComparison) {
        plan.rows    = 2 * distances.size();
        plan.columns = thicknesses.size();
        for (size_t distance = 0; distance < distances.size(); ++distance)
            for (size_t ironing_index = 0; ironing_index < 2; ++ironing_index)
                for (size_t column = 0; column < plan.columns; ++column) {
                    const bool   ironing = ironing_index == 1;
                    const size_t row     = 2 * distance + ironing_index;
                    plan.cells.push_back({thicknesses[column], distances[distance], ironing, row, column,
                                          "T=" + fuzzy_skin_calibration_value_label(thicknesses[column]) + " D=" +
                                              fuzzy_skin_calibration_value_label(distances[distance]) +
                                              " I=" + (ironing ? "1" : "0")});
                }
    } else {
        plan.rows    = distances.size();
        plan.columns = thicknesses.size();
        for (size_t row = 0; row < plan.rows; ++row)
            for (size_t column = 0; column < plan.columns; ++column)
                plan.cells.push_back({thicknesses[column], distances[row], false, row, column,
                                      "T=" + fuzzy_skin_calibration_value_label(thicknesses[column]) + " D=" +
                                          fuzzy_skin_calibration_value_label(distances[row])});
    }

    const size_t maximum_samples = config.mode == FuzzySkinCalibrationMode::IroningComparison ? 128 : 64;
    if (plan.cells.size() > maximum_samples)
        throw std::invalid_argument("Calibration plan is limited to 64 parameter combinations");
    return plan;
}

FuzzySkinCalibrationBedPatch fuzzy_skin_calibration_bed_patch(const FuzzySkinCalibrationConfig& config)
{
    if (!std::isfinite(config.coupon_width) || !std::isfinite(config.coupon_depth) || config.coupon_width <= 0.0 ||
        config.coupon_depth <= 0.0)
        throw std::invalid_argument("Calibration coupon dimensions must be positive");

    FuzzySkinCalibrationBedPatch patch;
    if (config.mode == FuzzySkinCalibrationMode::SupportedUnderside) {
        const double leg_width = std::min(3.0, 0.2 * config.coupon_width);
        patch.center_x         = -0.5 * config.coupon_width + 0.5 * leg_width;
        patch.radius           = std::min(0.4 * leg_width, 0.2 * config.coupon_depth);
    } else {
        patch.radius = 0.25 * std::min(config.coupon_width, config.coupon_depth);
    }
    return patch;
}

double fuzzy_skin_calibration_bed_thickness(const FuzzySkinCalibrationConfig& config, double thickness, double first_layer_height)
{
    if (!std::isfinite(thickness) || !std::isfinite(first_layer_height) || config.thickness.maximum <= 0.0 || thickness < 0.0 ||
        first_layer_height <= 0.0)
        throw std::invalid_argument("Calibration bed texture dimensions must be positive");
    return 0.25 * first_layer_height * std::clamp(thickness / config.thickness.maximum, 0.0, 1.0);
}

void apply_fuzzy_skin_calibration_print_config(DynamicPrintConfig& config)
{
    config.set_key_value("print_sequence", new ConfigOptionEnum<PrintSequence>(PrintSequence::ByLayer));
}

void populate_fuzzy_skin_calibration_cube(ModelObject&                      object,
                                          const FuzzySkinCalibrationConfig& config,
                                          const FuzzySkinCalibrationCell&   cell)
{
    const float                      half   = float(0.5 * config.coupon_width);
    const float                      height = float(config.coupon_width);
    const std::array<Vec2f, 4>       corners{{{-half, -half}, {half, -half}, {half, half}, {-half, half}}};
    const std::array<const char*, 4> names{{"Plain", "Fuzzy", "Ironed", "Fuzzy and ironed"}};
    // Four triangular prisms tile the cube without overlaps. Each owns one entire
    // outer wall and the triangular quarter of the top touching that wall.
    for (size_t side = 0; side < 4; ++side) {
        const Vec2f&         a = corners[side];
        const Vec2f&         b = corners[(side + 1) % 4];
        indexed_triangle_set mesh;
        mesh.vertices        = {{0.f, 0.f, 0.f},    {a.x(), a.y(), 0.f},    {b.x(), b.y(), 0.f},
                                {0.f, 0.f, height}, {a.x(), a.y(), height}, {b.x(), b.y(), height}};
        mesh.indices         = {{0, 2, 1}, {3, 4, 5}, {0, 1, 4}, {0, 4, 3}, {1, 2, 5}, {1, 5, 4}, {2, 0, 3}, {2, 3, 5}};
        ModelVolume* volume  = object.add_volume(TriangleMesh(std::move(mesh)), ModelVolumeType::MODEL_PART, false);
        const bool   fuzzy   = side == 1 || side == 3;
        const bool   ironing = side >= 2;
        volume->name         = std::string(names[side]) + " " + cell.label;
        ModelConfig& target  = volume->config;
        target.set_key_value("fuzzy_skin", new ConfigOptionEnum<FuzzySkinType>(fuzzy ? FuzzySkinType::External : FuzzySkinType::None));
        target.set_key_value("fuzzy_skin_thickness", new ConfigOptionFloat(cell.thickness));
        target.set_key_value("fuzzy_skin_point_distance", new ConfigOptionFloat(cell.distance));
        target.set_key_value("fuzzy_skin_top_surface", new ConfigOptionBool(fuzzy));
        target.set_key_value("fuzzy_skin_lower_surface", new ConfigOptionBool(false));
        target.set_key_value("fuzzy_skin_bed_surface", new ConfigOptionBool(false));
        target.set_key_value("fuzzy_skin_first_layer", new ConfigOptionBool(false));
        target.set_key_value("fuzzy_skin_connect_walls", new ConfigOptionBool(true));
        target.set_key_value("fuzzy_skin_compensate_extrusion", new ConfigOptionBool(true));
        target.set_key_value("fuzzy_skin_ironing", new ConfigOptionBool(fuzzy && ironing));
        // Reuse the transparent calibration's all-solid-layer ironing process.
        // The nozzle irons the horizontal layer up to its edge on every layer.
        target.set_key_value("ironing_type", new ConfigOptionEnum<IroningType>(ironing ? IroningType::AllSolid : IroningType::NoIroning));
        target.set_key_value("ironing_inset", new ConfigOptionFloat(0.01));

        if (config.labels) {
            const std::array<std::string, 3> lines{{"F=" + std::to_string(fuzzy) + " I=" + std::to_string(ironing),
                                                    "T=" + fuzzy_skin_calibration_value_label(cell.thickness),
                                                    "D=" + fuzzy_skin_calibration_value_label(cell.distance)}};
            for (size_t line = 0; line < lines.size(); ++line) {
                TriangleMesh label = make_fuzzy_skin_calibration_label(lines[line], std::min(1.6, double(height) / 10.0), 0.35);
                label.rotate_x(float(0.5 * PI));
                label.translate(0.f, -half + 0.05f, float(height * 0.5 + (1.0 - double(line)) * 2.5));
                label.rotate_z(float(side * 0.5 * PI));
                ModelVolume* label_volume = object.add_volume(std::move(label), ModelVolumeType::MODEL_PART, false);
                label_volume->name        = "Wall label " + lines[line];
                label_volume->config.set_key_value("fuzzy_skin", new ConfigOptionEnum<FuzzySkinType>(FuzzySkinType::None));
                label_volume->config.set_key_value("fuzzy_skin_top_surface", new ConfigOptionBool(false));
                label_volume->config.set_key_value("fuzzy_skin_lower_surface", new ConfigOptionBool(false));
                label_volume->config.set_key_value("fuzzy_skin_bed_surface", new ConfigOptionBool(false));
                label_volume->config.set_key_value("fuzzy_skin_ironing", new ConfigOptionBool(false));
                label_volume->config.set_key_value("ironing_type", new ConfigOptionEnum<IroningType>(IroningType::NoIroning));
            }
        }
    }
}

TriangleMesh make_fuzzy_skin_calibration_coupon(double width, double depth, double height)
{
    if (width <= 0.0 || depth <= 0.0 || height <= 0.0)
        throw std::invalid_argument("Calibration coupon dimensions must be positive");
    indexed_triangle_set mesh = its_make_cube(width, depth, height);
    translate(mesh, float(-0.5 * width), float(-0.5 * depth), 0.f);
    return TriangleMesh(std::move(mesh));
}

TriangleMesh make_fuzzy_skin_calibration_bridge(double width, double depth, double roof_height, double roof_thickness)
{
    if (width <= 0.0 || depth <= 0.0 || roof_height <= 0.0 || roof_thickness <= 0.0 || width < 6.0)
        throw std::invalid_argument("Supported-underside coupon dimensions are invalid");

    const double         leg_width = std::min(3.0, 0.2 * width);
    indexed_triangle_set mesh;
    for (double x : {-0.5 * width + 0.5 * leg_width, 0.5 * width - 0.5 * leg_width}) {
        indexed_triangle_set leg = its_make_cube(leg_width, depth, roof_height);
        translate(leg, float(x - 0.5 * leg_width), float(-0.5 * depth), 0.f);
        its_merge(mesh, leg);
    }
    indexed_triangle_set roof = its_make_cube(width, depth, roof_thickness);
    translate(roof, float(-0.5 * width), float(-0.5 * depth), float(roof_height));
    its_merge(mesh, roof);
    return TriangleMesh(std::move(mesh));
}

TriangleMesh make_fuzzy_skin_calibration_bed_patch(double radius, double height)
{
    if (radius <= 0.0 || height <= 0.0)
        throw std::invalid_argument("Calibration bed patch dimensions must be positive");
    return TriangleMesh(its_make_cylinder(radius, height, PI / 36.0));
}

TriangleMesh make_fuzzy_skin_calibration_label(const std::string& text, double glyph_height, double relief, double pedestal_height)
{
    if (text.empty())
        return {};
    if (glyph_height <= 0.0 || relief <= 0.0 || !std::isfinite(pedestal_height) || pedestal_height < 0.0)
        throw std::invalid_argument("Calibration label dimensions must be positive");

    const double         cell       = glyph_height / 7.0;
    const double         pixel      = 0.88 * cell;
    const double         advance    = 6.0 * cell;
    const double         text_width = text.empty() ? 0.0 : (6.0 * double(text.size()) - 1.0) * cell;
    indexed_triangle_set mesh;
    for (size_t character = 0; character < text.size(); ++character) {
        const auto& rows = glyph(static_cast<char>(std::toupper(static_cast<unsigned char>(text[character]))));
        for (size_t row = 0; row < rows.size(); ++row)
            for (size_t column = 0; column < 5; ++column)
                if ((rows[row] & (1u << (4u - unsigned(column)))) != 0) {
                    indexed_triangle_set dot = its_make_cube(pixel, pixel, relief);
                    const double         x   = -0.5 * text_width + double(character) * advance + double(column) * cell;
                    const double         y   = 0.5 * glyph_height - double(row + 1) * cell;
                    translate(dot, float(x), float(y), float(pedestal_height));
                    its_merge(mesh, dot);
                }
    }
    if (pedestal_height > 0.0) {
        // A continuous smooth pad keeps the glyphs above the texture peaks.
        indexed_triangle_set pedestal = its_make_cube(text_width + 0.8, glyph_height + 0.8, pedestal_height + 0.02);
        translate(pedestal, float(-0.5 * text_width - 0.4), float(-0.5 * glyph_height - 0.4), 0.f);
        its_merge(mesh, pedestal);
    }
    return TriangleMesh(std::move(mesh));
}

} // namespace Slic3r
