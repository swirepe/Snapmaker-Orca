#include <catch2/catch_test_macros.hpp>

#include "libslic3r/GCode/Embedded3MF.hpp"

#include <boost/filesystem/operations.hpp>
#include <boost/nowide/fstream.hpp>

#include <sstream>

namespace fs = boost::filesystem;
using namespace Slic3r::GCodeEmbedded3MF;

namespace {

class TempFiles
{
public:
    TempFiles()
        : directory(fs::temp_directory_path() / fs::unique_path("orca_embedded_3mf_%%%%-%%%%-%%%%"))
        , gcode(directory / "print.gcode")
        , project(directory / "project.3mf")
        , extracted(directory / "extracted.3mf")
    {
        fs::create_directories(directory);
    }

    ~TempFiles()
    {
        boost::system::error_code ec;
        fs::remove_all(directory, ec);
    }

    void write(const fs::path &path, const std::string &contents)
    {
        boost::nowide::ofstream output(path.string(), std::ios::binary | std::ios::trunc);
        REQUIRE(output.good());
        output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        REQUIRE(output.good());
    }

    std::string read(const fs::path &path)
    {
        boost::nowide::ifstream input(path.string(), std::ios::binary);
        REQUIRE(input.good());
        return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    }

    fs::path directory;
    fs::path gcode;
    fs::path project;
    fs::path extracted;
};

std::string replace_once(std::string value, const std::string &from, const std::string &to)
{
    const std::size_t position = value.find(from);
    REQUIRE(position != std::string::npos);
    value.replace(position, from.size(), to);
    return value;
}

std::size_t replace_all(std::string &value, const std::string &from, const std::string &to)
{
    std::size_t count    = 0;
    std::size_t position = 0;
    while ((position = value.find(from, position)) != std::string::npos) {
        value.replace(position, from.size(), to);
        position += to.size();
        ++count;
    }
    return count;
}

} // namespace

TEST_CASE("3MF projects round trip through G-code comments", "[Embedded3MF]")
{
    TempFiles files;
    files.write(files.gcode, "G28\nG1 X10 Y20\n");

    std::string project;
    project.reserve(200000);
    for (int i = 0; i < 200000; ++i)
        project.push_back(static_cast<char>((i * 31) & 0xff));
    files.write(files.project, project);

    std::string error;
    REQUIRE(append(files.gcode.string(), files.project.string(), error));
    REQUIRE(error.empty());

    const std::string combined = files.read(files.gcode);
    const std::size_t trailer_position = combined.find("; SLIC3R_EMBEDDED_3MF_BEGIN 1\n");
    REQUIRE(trailer_position != std::string::npos);
    REQUIRE(combined.find("SNAPMAKER_ORCA_EMBEDDED_3MF_") == std::string::npos);
    std::istringstream trailer(combined.substr(trailer_position));
    std::string line;
    bool all_lines_are_comments = true;
    bool data_lines_fit_limit   = true;
    bool reading_data           = false;
    while (std::getline(trailer, line)) {
        all_lines_are_comments = all_lines_are_comments && line.rfind("; ", 0) == 0;
        if (line == "; SLIC3R_EMBEDDED_3MF_DATA")
            reading_data = true;
        else if (line == "; SLIC3R_EMBEDDED_3MF_END")
            reading_data = false;
        else if (reading_data)
            data_lines_fit_limit = data_lines_fit_limit && line.size() <= 78;
    }
    REQUIRE(all_lines_are_comments);
    REQUIRE(data_lines_fit_limit);

    const ExtractResult result = extract(files.gcode.string(), files.extracted.string());
    REQUIRE(result.status == ExtractStatus::Valid);
    REQUIRE(result.error.empty());
    REQUIRE(result.expected_size == project.size());
    REQUIRE(result.actual_size == project.size());
    REQUIRE(result.expected_sha256 == result.actual_sha256);
    REQUIRE(files.read(files.extracted) == project);
}

TEST_CASE("Embedding adds a newline without changing existing G-code", "[Embedded3MF]")
{
    TempFiles files;
    files.write(files.gcode, "G28");
    files.write(files.project, "PK\x03\x04project");

    std::string error;
    REQUIRE(append(files.gcode.string(), files.project.string(), error));
    const std::string combined = files.read(files.gcode);
    REQUIRE(combined.rfind("G28\n; SLIC3R_EMBEDDED_3MF_BEGIN 1\n", 0) == 0);
    REQUIRE(extract(files.gcode.string(), files.extracted.string()).status == ExtractStatus::Valid);
}

TEST_CASE("Legacy Snapmaker Orca marker namespace remains readable", "[Embedded3MF]")
{
    TempFiles files;
    files.write(files.gcode, "G28\n");
    files.write(files.project, "PK\x03\x04legacy project");

    std::string error;
    REQUIRE(append(files.gcode.string(), files.project.string(), error));
    std::string combined = files.read(files.gcode);
    REQUIRE(replace_all(combined, "SLIC3R_EMBEDDED_3MF_", "SNAPMAKER_ORCA_EMBEDDED_3MF_") == 5);
    files.write(files.gcode, combined);

    const ExtractResult result = extract(files.gcode.string(), files.extracted.string());
    REQUIRE(result.status == ExtractStatus::Valid);
    REQUIRE(result.error.empty());
    REQUIRE(files.read(files.extracted) == files.read(files.project));
}

TEST_CASE("Empty project payloads are rejected", "[Embedded3MF]")
{
    TempFiles files;
    files.write(files.gcode, "G28\n");
    files.write(files.project, "");

    const std::string original_gcode = files.read(files.gcode);
    std::string error;
    REQUIRE_FALSE(append(files.gcode.string(), files.project.string(), error));
    REQUIRE(error.find("empty") != std::string::npos);
    REQUIRE(files.read(files.gcode) == original_gcode);
}

TEST_CASE("Ordinary G-code has no embedded project", "[Embedded3MF]")
{
    TempFiles files;
    files.write(files.gcode, "G28\n; ordinary comment\n");
    files.write(files.extracted, "stale");

    const ExtractResult result = extract(files.gcode.string(), files.extracted.string());
    REQUIRE(result.status == ExtractStatus::NotFound);
    REQUIRE_FALSE(fs::exists(files.extracted));
}

TEST_CASE("Embedded project checksum corruption retains recoverable bytes", "[Embedded3MF]")
{
    TempFiles files;
    files.write(files.gcode, "G28\n");
    files.write(files.project, "PK\x03\x04recoverable project");
    std::string error;
    REQUIRE(append(files.gcode.string(), files.project.string(), error));

    std::string combined = files.read(files.gcode);
    const std::string hash_prefix = "; SLIC3R_EMBEDDED_3MF_SHA256 ";
    const std::size_t hash_position = combined.find(hash_prefix);
    REQUIRE(hash_position != std::string::npos);
    const std::size_t digit_position = hash_position + hash_prefix.size();
    REQUIRE(digit_position < combined.size());
    combined[digit_position] = combined[digit_position] == '0' ? '1' : '0';
    files.write(files.gcode, combined);

    const ExtractResult result = extract(files.gcode.string(), files.extracted.string());
    REQUIRE(result.status == ExtractStatus::Corrupt);
    REQUIRE(result.error.find("SHA-256 checksum does not match") != std::string::npos);
    REQUIRE(result.has_project_data);
    REQUIRE(files.read(files.extracted) == files.read(files.project));
}

TEST_CASE("Embedded project size mismatches are reported", "[Embedded3MF]")
{
    TempFiles files;
    files.write(files.gcode, "G28\n");
    files.write(files.project, "project bytes");
    std::string error;
    REQUIRE(append(files.gcode.string(), files.project.string(), error));
    files.write(files.gcode, replace_once(files.read(files.gcode),
        "; SLIC3R_EMBEDDED_3MF_SIZE 13", "; SLIC3R_EMBEDDED_3MF_SIZE 14"));

    const ExtractResult result = extract(files.gcode.string(), files.extracted.string());
    REQUIRE(result.status == ExtractStatus::Corrupt);
    REQUIRE(result.error.find("size does not match") != std::string::npos);
    REQUIRE(result.has_project_data);
}

TEST_CASE("Truncated and malformed Base64 payloads are reported", "[Embedded3MF]")
{
    TempFiles files;

    SECTION("truncated trailer") {
        files.write(files.gcode,
            "G28\n"
            "; SLIC3R_EMBEDDED_3MF_BEGIN 1\n"
            "; SLIC3R_EMBEDDED_3MF_SIZE 4\n"
            "; SLIC3R_EMBEDDED_3MF_SHA256 0000000000000000000000000000000000000000000000000000000000000000\n"
            "; SLIC3R_EMBEDDED_3MF_DATA\n"
            "; UEsD\n");
        const ExtractResult result = extract(files.gcode.string(), files.extracted.string());
        REQUIRE(result.status == ExtractStatus::Corrupt);
        REQUIRE(result.error.find("truncated") != std::string::npos);
        REQUIRE(result.has_project_data);
    }

    SECTION("invalid Base64") {
        files.write(files.gcode,
            "G28\n"
            "; SLIC3R_EMBEDDED_3MF_BEGIN 1\n"
            "; SLIC3R_EMBEDDED_3MF_SIZE 4\n"
            "; SLIC3R_EMBEDDED_3MF_SHA256 0000000000000000000000000000000000000000000000000000000000000000\n"
            "; SLIC3R_EMBEDDED_3MF_DATA\n"
            "; UEs!\n"
            "; SLIC3R_EMBEDDED_3MF_END\n");
        const ExtractResult result = extract(files.gcode.string(), files.extracted.string());
        REQUIRE(result.status == ExtractStatus::Corrupt);
        REQUIRE(result.error.find("invalid Base64") != std::string::npos);
        REQUIRE_FALSE(result.has_project_data);
        REQUIRE_FALSE(fs::exists(files.extracted));
    }
}

TEST_CASE("Unsupported versions and duplicate payloads are reported", "[Embedded3MF]")
{
    TempFiles files;
    files.write(files.gcode, "G28\n");
    files.write(files.project, "project bytes");
    std::string error;
    REQUIRE(append(files.gcode.string(), files.project.string(), error));

    SECTION("unsupported version") {
        files.write(files.gcode, replace_once(files.read(files.gcode),
            "; SLIC3R_EMBEDDED_3MF_BEGIN 1", "; SLIC3R_EMBEDDED_3MF_BEGIN 2"));
        const ExtractResult result = extract(files.gcode.string(), files.extracted.string());
        REQUIRE(result.status == ExtractStatus::Corrupt);
        REQUIRE(result.error.find("unsupported format version") != std::string::npos);
        REQUIRE(result.has_project_data);
    }

    SECTION("duplicate marker") {
        std::string combined = files.read(files.gcode);
        combined += "; SNAPMAKER_ORCA_EMBEDDED_3MF_BEGIN 1\n";
        files.write(files.gcode, combined);
        const ExtractResult result = extract(files.gcode.string(), files.extracted.string());
        REQUIRE(result.status == ExtractStatus::Corrupt);
        REQUIRE(result.error.find("Multiple embedded project markers") != std::string::npos);
    }
}
