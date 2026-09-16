#ifndef slic3r_GCode_Embedded3MF_hpp_
#define slic3r_GCode_Embedded3MF_hpp_

#include <cstdint>
#include <string>

namespace Slic3r::GCodeEmbedded3MF {

enum class ExtractStatus {
    NotFound,
    Valid,
    Corrupt,
    IoError,
};

struct ExtractResult {
    ExtractStatus status             = ExtractStatus::NotFound;
    std::string   error;
    std::uint64_t expected_size      = 0;
    std::uint64_t actual_size        = 0;
    std::string   expected_sha256;
    std::string   actual_sha256;
    bool          has_project_data   = false;
};

// Appends a versioned, comment-only Base64 trailer containing project_path to
// gcode_path. On failure, gcode_path is restored to its original size.
bool append(const std::string &gcode_path, const std::string &project_path, std::string &error);

// Extracts the first embedded project in gcode_path to output_path. Corrupt
// payloads keep any successfully decoded bytes so callers may offer to open
// them anyway. output_path is removed when no project bytes could be decoded.
ExtractResult extract(const std::string &gcode_path, const std::string &output_path);

} // namespace Slic3r::GCodeEmbedded3MF

#endif // slic3r_GCode_Embedded3MF_hpp_
