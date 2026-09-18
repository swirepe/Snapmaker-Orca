#include "PaneCalibration.hpp"

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

std::vector<const PaneCalibrationFactorSetting *> enabled_factors(const PaneCalibrationConfig &config)
{
    std::vector<const PaneCalibrationFactorSetting *> out;
    for (const PaneCalibrationFactorSetting &factor : config.factors)
        if (factor.enabled)
            out.emplace_back(&factor);
    return out;
}

unsigned gf4_multiply(unsigned lhs, unsigned rhs)
{
    // GF(4), represented as a + bx with x^2 = x + 1. Addition is XOR.
    unsigned result = 0;
    unsigned value  = lhs;
    unsigned factor = rhs;
    while (factor != 0) {
        if ((factor & 1u) != 0)
            result ^= value;
        factor >>= 1u;
        value <<= 1u;
        if ((value & 4u) != 0)
            value ^= 0b111u;
    }
    return result & 3u;
}

std::vector<std::vector<unsigned>> binary_array(unsigned dimensions)
{
    const unsigned rows = 1u << dimensions;
    std::vector<std::vector<unsigned>> result(rows, std::vector<unsigned>(rows - 1u, 0u));
    for (unsigned row = 0; row < rows; ++row)
        for (unsigned column_mask = 1; column_mask < rows; ++column_mask) {
            unsigned bits = row & column_mask;
            unsigned parity = 0;
            while (bits != 0) {
                parity ^= bits & 1u;
                bits >>= 1u;
            }
            result[row][column_mask - 1u] = parity;
        }
    return result;
}

std::vector<std::vector<unsigned>> ternary_projective_array(unsigned dimensions)
{
    if (dimensions == 2) {
        const std::array<std::array<unsigned, 2>, 4> columns {{{1, 0}, {0, 1}, {1, 1}, {1, 2}}};
        std::vector<std::vector<unsigned>> result(9, std::vector<unsigned>(columns.size()));
        size_t row = 0;
        for (unsigned x = 0; x < 3; ++x)
            for (unsigned y = 0; y < 3; ++y, ++row)
                for (size_t column = 0; column < columns.size(); ++column)
                    result[row][column] = (x * columns[column][0] + y * columns[column][1]) % 3u;
        return result;
    }

    std::vector<std::array<unsigned, 3>> columns;
    for (unsigned y = 0; y < 3; ++y)
        for (unsigned z = 0; z < 3; ++z)
            columns.push_back({1, y, z});
    for (unsigned z = 0; z < 3; ++z)
        columns.push_back({0, 1, z});
    columns.push_back({0, 0, 1});

    std::vector<std::vector<unsigned>> result(27, std::vector<unsigned>(columns.size()));
    size_t row = 0;
    for (unsigned x = 0; x < 3; ++x)
        for (unsigned y = 0; y < 3; ++y)
            for (unsigned z = 0; z < 3; ++z, ++row)
                for (size_t column = 0; column < columns.size(); ++column)
                    result[row][column] = (x * columns[column][0] + y * columns[column][1] + z * columns[column][2]) % 3u;
    return result;
}

std::vector<std::vector<unsigned>> l18_three_level_array()
{
    // The first (two-level) column of the conventional L18 is intentionally
    // omitted. The remaining seven columns form an OA(18, 7, 3, 2).
    static constexpr unsigned data[18][7] = {
        {0, 0, 0, 0, 0, 0, 0}, {0, 1, 1, 1, 1, 1, 1}, {0, 2, 2, 2, 2, 2, 2},
        {1, 0, 0, 1, 1, 2, 2}, {1, 1, 1, 2, 2, 0, 0}, {1, 2, 2, 0, 0, 1, 1},
        {2, 0, 1, 0, 2, 1, 2}, {2, 1, 2, 1, 0, 2, 0}, {2, 2, 0, 2, 1, 0, 1},
        {0, 0, 2, 2, 1, 1, 0}, {0, 1, 0, 0, 2, 2, 1}, {0, 2, 1, 1, 0, 0, 2},
        {1, 0, 1, 2, 0, 2, 1}, {1, 1, 2, 0, 1, 0, 2}, {1, 2, 0, 1, 2, 1, 0},
        {2, 0, 2, 1, 2, 0, 1}, {2, 1, 0, 2, 0, 1, 2}, {2, 2, 1, 0, 1, 2, 0},
    };
    std::vector<std::vector<unsigned>> result(18, std::vector<unsigned>(7));
    for (size_t row = 0; row < result.size(); ++row)
        std::copy(std::begin(data[row]), std::end(data[row]), result[row].begin());
    return result;
}

std::vector<std::vector<unsigned>> four_level_array()
{
    // OA(16, 5, 4, 2), constructed from the affine plane over GF(4).
    std::vector<std::vector<unsigned>> result(16, std::vector<unsigned>(5));
    size_t row = 0;
    for (unsigned x = 0; x < 4; ++x)
        for (unsigned y = 0; y < 4; ++y, ++row) {
            result[row][0] = x;
            result[row][1] = y;
            result[row][2] = x ^ y;
            result[row][3] = x ^ gf4_multiply(2, y);
            result[row][4] = x ^ gf4_multiply(3, y);
        }
    return result;
}

PaneCalibrationRow make_row(const std::vector<const PaneCalibrationFactorSetting *> &factors,
                            const std::vector<unsigned> &levels, unsigned common_levels)
{
    PaneCalibrationRow row;
    row.reserve(factors.size());
    for (size_t column = 0; column < factors.size(); ++column) {
        const PaneCalibrationFactorSetting &factor = *factors[column];
        const unsigned factor_levels = common_levels == 0 ? factor.levels : common_levels;
        const unsigned level         = levels[column];
        if (factor.factor == PaneCalibrationFactor::IroningType || factor.factor == PaneCalibrationFactor::IroningAngle) {
            // Categorical factors are keyed by the experiment level. Their
            // numeric min/max fields are intentionally ignored.
            row.push_back({factor.factor, double(level), level, factor_levels});
        } else {
            const std::vector<double> values = pane_calibration_linear_values(factor.minimum, factor.maximum, factor_levels);
            row.push_back({factor.factor, values[level], level, factor_levels});
        }
    }
    return row;
}

std::string trim_number(double value, unsigned precision)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    std::string text = stream.str();
    const size_t decimal = text.find('.');
    if (decimal != std::string::npos) {
        while (text.size() > decimal + 1 && text.back() == '0')
            text.pop_back();
        if (text.back() == '.')
            text.pop_back();
    }
    return text;
}

void translate(indexed_triangle_set &mesh, float x, float y, float z)
{
    for (Vec3f &vertex : mesh.vertices)
        vertex += Vec3f(x, y, z);
}

const std::array<unsigned char, 7> &glyph(char value)
{
    using Rows = std::array<unsigned char, 7>;
    static const Rows blank {0, 0, 0, 0, 0, 0, 0};
    static const std::unordered_map<char, Rows> glyphs {
        {'0', {14, 17, 19, 21, 25, 17, 14}}, {'1', {4, 12, 4, 4, 4, 4, 14}},
        {'2', {14, 17, 1, 2, 4, 8, 31}},     {'3', {30, 1, 1, 14, 1, 1, 30}},
        {'4', {2, 6, 10, 18, 31, 2, 2}},     {'5', {31, 16, 16, 30, 1, 1, 30}},
        {'6', {14, 16, 16, 30, 17, 17, 14}}, {'7', {31, 1, 2, 4, 8, 8, 8}},
        {'8', {14, 17, 17, 14, 17, 17, 14}}, {'9', {14, 17, 17, 15, 1, 1, 14}},
        {'A', {14, 17, 17, 31, 17, 17, 17}}, {'B', {30, 17, 17, 30, 17, 17, 30}},
        {'C', {14, 17, 16, 16, 16, 17, 14}}, {'D', {30, 17, 17, 17, 17, 17, 30}},
        {'E', {31, 16, 16, 30, 16, 16, 31}}, {'F', {31, 16, 16, 30, 16, 16, 16}},
        {'G', {14, 17, 16, 23, 17, 17, 15}}, {'H', {17, 17, 17, 31, 17, 17, 17}},
        {'I', {14, 4, 4, 4, 4, 4, 14}},      {'J', {7, 2, 2, 2, 18, 18, 12}},
        {'K', {17, 18, 20, 24, 20, 18, 17}}, {'L', {16, 16, 16, 16, 16, 16, 31}},
        {'M', {17, 27, 21, 21, 17, 17, 17}}, {'N', {17, 25, 21, 19, 17, 17, 17}},
        {'O', {14, 17, 17, 17, 17, 17, 14}}, {'P', {30, 17, 17, 30, 16, 16, 16}},
        {'Q', {14, 17, 17, 17, 21, 18, 13}}, {'R', {30, 17, 17, 30, 20, 18, 17}},
        {'S', {15, 16, 16, 14, 1, 1, 30}},   {'T', {31, 4, 4, 4, 4, 4, 4}},
        {'U', {17, 17, 17, 17, 17, 17, 14}}, {'V', {17, 17, 17, 17, 17, 10, 4}},
        {'W', {17, 17, 17, 21, 21, 21, 10}}, {'X', {17, 17, 10, 4, 10, 17, 17}},
        {'Y', {17, 17, 10, 4, 4, 4, 4}},     {'Z', {31, 1, 2, 4, 8, 16, 31}},
        {'=', {0, 0, 31, 0, 31, 0, 0}},      {'-', {0, 0, 0, 31, 0, 0, 0}},
        {'.', {0, 0, 0, 0, 0, 12, 12}},      {'%', {17, 2, 4, 8, 16, 17, 0}},
        {'/', {1, 2, 4, 4, 8, 16, 0}},       {' ', {0, 0, 0, 0, 0, 0, 0}},
    };
    const auto found = glyphs.find(value);
    return found == glyphs.end() ? blank : found->second;
}

} // namespace

std::vector<double> pane_calibration_linear_values(double minimum, double maximum, unsigned levels)
{
    if (levels < 2)
        throw std::invalid_argument("A calibration factor requires at least two levels");
    if (maximum < minimum)
        throw std::invalid_argument("A calibration factor maximum must not be lower than its minimum");

    std::vector<double> values(levels, minimum);
    const double step = (maximum - minimum) / double(levels - 1);
    for (unsigned index = 0; index < levels; ++index)
        values[index] = index + 1 == levels ? maximum : minimum + step * double(index);
    return values;
}

std::pair<std::string, std::vector<std::vector<unsigned>>>
pane_calibration_taguchi_array(unsigned levels, size_t columns)
{
    if (columns < 2)
        throw std::invalid_argument("A Taguchi calibration requires at least two factors");

    if (levels == 2) {
        if (columns <= 3)
            return {"L4", binary_array(2)};
        if (columns <= 7)
            return {"L8", binary_array(3)};
        if (columns <= 15)
            return {"L16", binary_array(4)};
    } else if (levels == 3) {
        if (columns <= 4)
            return {"L9", ternary_projective_array(2)};
        if (columns <= 7)
            return {"L18", l18_three_level_array()};
        if (columns <= 13)
            return {"L27", ternary_projective_array(3)};
    } else if (levels == 4 && columns <= 5) {
        return {"L16", four_level_array()};
    }

    throw std::invalid_argument("No supported Taguchi array has enough columns for this level count");
}

PaneCalibrationPlan build_pane_calibration_plan(const PaneCalibrationConfig &config)
{
    const std::vector<const PaneCalibrationFactorSetting *> factors = enabled_factors(config);
    PaneCalibrationPlan plan;

    if (config.design == PaneCalibrationDesign::Linear) {
        if (factors.size() != 1)
            throw std::invalid_argument("A linear calibration requires exactly one enabled factor");
        if (factors.front()->levels < 2 || factors.front()->levels > 10)
            throw std::invalid_argument("A linear calibration factor must use between two and ten levels");
        plan.array_name = "Linear";
        for (unsigned level = 0; level < factors.front()->levels; ++level)
            plan.rows.emplace_back(make_row(factors, {level}, 0));
    } else if (config.design == PaneCalibrationDesign::Grid) {
        if (factors.size() != 2)
            throw std::invalid_argument("A grid calibration requires exactly two enabled factors");
        if (factors[0]->levels < 2 || factors[0]->levels > 10 || factors[1]->levels < 2 || factors[1]->levels > 10)
            throw std::invalid_argument("Grid calibration factors must use between two and ten levels");
        plan.array_name = "Grid";
        for (unsigned first = 0; first < factors[0]->levels; ++first)
            for (unsigned second = 0; second < factors[1]->levels; ++second)
                plan.rows.emplace_back(make_row(factors, {first, second}, 0));
    } else {
        if (config.taguchi_levels < 2 || config.taguchi_levels > 4)
            throw std::invalid_argument("Taguchi calibration supports two, three, or four levels");
        auto [name, array] = pane_calibration_taguchi_array(config.taguchi_levels, factors.size());
        plan.array_name = std::move(name);
        plan.rows.reserve(array.size());
        for (const std::vector<unsigned> &array_row : array) {
            std::vector<unsigned> row_levels(array_row.begin(), array_row.begin() + factors.size());
            plan.rows.emplace_back(make_row(factors, row_levels, config.taguchi_levels));
        }
    }

    return plan;
}

std::string pane_calibration_factor_key(PaneCalibrationFactor factor)
{
    switch (factor) {
    case PaneCalibrationFactor::NozzleTemperature: return "T";
    case PaneCalibrationFactor::PrintSpeed: return "S";
    case PaneCalibrationFactor::FlowRatio: return "FR";
    case PaneCalibrationFactor::LayerHeight: return "LH";
    case PaneCalibrationFactor::MaxFanSpeed: return "F";
    case PaneCalibrationFactor::WallFanSpeed: return "WF";
    case PaneCalibrationFactor::IroningFanSpeed: return "IF";
    case PaneCalibrationFactor::AuxiliaryFanSpeed: return "AF";
    case PaneCalibrationFactor::IroningType: return "IT";
    case PaneCalibrationFactor::IroningFlow: return "IQ";
    case PaneCalibrationFactor::IroningAngle: return "IA";
    case PaneCalibrationFactor::LineWidth: return "LW";
    case PaneCalibrationFactor::IroningSpeed: return "IS";
    case PaneCalibrationFactor::IroningSpacing: return "IP";
    }
    return "?";
}

std::string pane_calibration_format_value(PaneCalibrationFactor factor, double value, unsigned experiment_levels)
{
    if (factor == PaneCalibrationFactor::IroningType) {
        const unsigned level = unsigned(std::lround(value));
        if (experiment_levels == 2)
            return level == 0 ? "OFF" : "ALL";
        if (experiment_levels == 3) {
            static constexpr const char *values[] = {"OFF", "TOP", "ALL"};
            return values[std::min(level, 2u)];
        }
        static constexpr const char *values[] = {"OFF", "TOP", "ALT", "ALL"};
        return values[std::min(level, 3u)];
    }
    if (factor == PaneCalibrationFactor::IroningAngle) {
        const unsigned level = unsigned(std::lround(value));
        if (experiment_levels == 2)
            return level == 0 ? "0" : "90";
        if (experiment_levels == 3) {
            static constexpr const char *values[] = {"0", "45", "90"};
            return values[std::min(level, 2u)];
        }
        static constexpr const char *values[] = {"0", "45", "90", "135"};
        return values[std::min(level, 3u)];
    }

    switch (factor) {
    case PaneCalibrationFactor::NozzleTemperature:
    case PaneCalibrationFactor::MaxFanSpeed:
    case PaneCalibrationFactor::WallFanSpeed:
    case PaneCalibrationFactor::IroningFanSpeed:
    case PaneCalibrationFactor::AuxiliaryFanSpeed:
    case PaneCalibrationFactor::IroningFlow: return trim_number(value, 0);
    case PaneCalibrationFactor::PrintSpeed:
    case PaneCalibrationFactor::IroningSpeed: return trim_number(value, 1);
    case PaneCalibrationFactor::FlowRatio: return trim_number(value, 2);
    case PaneCalibrationFactor::LayerHeight:
    case PaneCalibrationFactor::LineWidth:
    case PaneCalibrationFactor::IroningSpacing: return trim_number(value, 3);
    case PaneCalibrationFactor::IroningType:
    case PaneCalibrationFactor::IroningAngle: break;
    }
    return trim_number(value, 2);
}

std::vector<std::string> pane_calibration_label_lines(const PaneCalibrationRow &row, unsigned experiment_levels,
                                                      size_t maximum_characters_per_line)
{
    if (maximum_characters_per_line < 3)
        throw std::invalid_argument("Calibration labels require at least three characters per line");

    std::vector<std::string> lines;
    for (const PaneCalibrationValue &entry : row) {
        const unsigned levels = entry.levels >= 2 ? entry.levels : experiment_levels;
        const std::string token = pane_calibration_factor_key(entry.factor) + "=" +
                                  pane_calibration_format_value(entry.factor, entry.value, levels);
        if (token.size() > maximum_characters_per_line)
            throw std::invalid_argument("A calibration label token does not fit the pane");
        if (lines.empty() || lines.back().size() + 1 + token.size() > maximum_characters_per_line)
            lines.emplace_back(token);
        else
            lines.back() += " " + token;
    }
    return lines;
}

PaneCalibrationConfig default_pane_calibration_config(PaneCalibrationTool tool)
{
    PaneCalibrationConfig config;
    config.tool        = tool;
    config.design      = PaneCalibrationDesign::Taguchi;
    config.taguchi_levels = 4;
    config.pane_height = tool == PaneCalibrationTool::ClearFilament ? 1. : 2.;

    config.factors = {
        {PaneCalibrationFactor::NozzleTemperature, tool == PaneCalibrationTool::ClearFilament, 260., 275., 4},
        {PaneCalibrationFactor::PrintSpeed,        tool == PaneCalibrationTool::ClearFilament, 25., 50., 4},
        {PaneCalibrationFactor::FlowRatio,         tool == PaneCalibrationTool::ClearFilament, 0.96, 1.08, 4},
        {PaneCalibrationFactor::LayerHeight,       tool == PaneCalibrationTool::ClearFilament, 0.1, 0.4, 4},
        {PaneCalibrationFactor::MaxFanSpeed,       false, 0., 60., 4},
        {PaneCalibrationFactor::WallFanSpeed,      false, 20., 80., 4},
        {PaneCalibrationFactor::IroningFanSpeed,   false, 0., 60., 4},
        {PaneCalibrationFactor::AuxiliaryFanSpeed, false, 0., 80., 4},
        {PaneCalibrationFactor::IroningType,       tool == PaneCalibrationTool::Ironing, 0., 3., 4},
        {PaneCalibrationFactor::IroningFlow,       tool == PaneCalibrationTool::Ironing, 5., 20., 4},
        {PaneCalibrationFactor::IroningAngle,      false, 0., 3., 4},
        {PaneCalibrationFactor::LineWidth,         false, 0.4, 0.6, 4},
        {PaneCalibrationFactor::IroningSpeed,      tool == PaneCalibrationTool::Ironing, 10., 30., 4},
        {PaneCalibrationFactor::IroningSpacing,    tool == PaneCalibrationTool::Ironing, 0.05, 0.2, 4},
    };
    return config;
}

TriangleMesh make_pane_calibration_body(const PaneCalibrationConfig &config, double first_layer_height)
{
    if (config.pane_width <= 0. || config.pane_depth <= 0. || config.pane_height <= 0.)
        throw std::invalid_argument("Pane dimensions must be greater than zero");

    indexed_triangle_set body = its_make_cube(config.pane_width, config.pane_depth, config.pane_height);
    translate(body, float(-0.5 * config.pane_width), float(-0.5 * config.pane_depth), 0.f);

    if (config.mouse_ears) {
        const double diameter = config.mouse_ear_diameter;
        if (diameter <= 0. || first_layer_height <= 0.)
            throw std::invalid_argument("Mouse-ear dimensions must be greater than zero");
        const float radius = float(0.5 * diameter);
        const float x = float(0.5 * config.pane_width - 0.45 * radius);
        const float y = float(0.5 * config.pane_depth - 0.45 * radius);
        for (float x_sign : {-1.f, 1.f})
            for (float y_sign : {-1.f, 1.f}) {
                indexed_triangle_set ear = its_make_cylinder(radius, std::min(first_layer_height, config.pane_height), PI / 18.);
                translate(ear, x_sign * x, y_sign * y, 0.f);
                its_merge(body, ear);
            }
    }

    return TriangleMesh(std::move(body));
}

TriangleMesh make_pane_calibration_label(const std::vector<std::string> &lines, double glyph_height, double relief,
                                         double pane_width, double pane_depth)
{
    if (lines.empty())
        return {};
    if (glyph_height <= 0. || relief <= 0.)
        throw std::invalid_argument("Label dimensions must be greater than zero");

    const double cell       = glyph_height / 7.;
    const double pixel      = 0.9 * cell;
    const double advance_x  = 6. * cell;
    const double advance_y  = 8. * cell;
    const size_t max_length = std::max_element(lines.begin(), lines.end(),
        [](const std::string &lhs, const std::string &rhs) { return lhs.size() < rhs.size(); })->size();
    const double width  = max_length == 0 ? 0. : (double(max_length) * 6. - 1.) * cell;
    const double depth  = (double(lines.size()) * 8. - 1.) * cell;
    if (width > pane_width - 2. * cell || depth > pane_depth - 2. * cell)
        throw std::invalid_argument("The calibration label does not fit the pane");

    indexed_triangle_set result;
    const double top = 0.5 * depth;
    for (size_t line_index = 0; line_index < lines.size(); ++line_index) {
        const std::string &line = lines[line_index];
        const double line_width = line.empty() ? 0. : (double(line.size()) * 6. - 1.) * cell;
        const double left = -0.5 * line_width;
        for (size_t character_index = 0; character_index < line.size(); ++character_index) {
            const auto &rows = glyph(char(std::toupper(static_cast<unsigned char>(line[character_index]))));
            for (unsigned row = 0; row < 7; ++row)
                for (unsigned column = 0; column < 5; ++column)
                    if ((rows[row] & (1u << (4u - column))) != 0) {
                        indexed_triangle_set voxel = its_make_cube(pixel, pixel, relief);
                        const float x = float(left + (double(character_index) * 6. + column) * cell);
                        const float y = float(top - (double(line_index) * 8. + row + 1.) * cell);
                        translate(voxel, x, y, 0.f);
                        its_merge(result, voxel);
                    }
        }
    }
    return TriangleMesh(std::move(result));
}

} // namespace Slic3r
