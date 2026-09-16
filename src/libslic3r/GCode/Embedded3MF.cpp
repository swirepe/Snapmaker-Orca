#include "Embedded3MF.hpp"

#include <array>
#include <charconv>
#include <cctype>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string_view>

#include <boost/filesystem/operations.hpp>
#include <boost/nowide/fstream.hpp>

#include <openssl/evp.h>

namespace Slic3r::GCodeEmbedded3MF {
namespace {

struct MarkerSet
{
    std::string_view begin_prefix;
    std::string_view size_prefix;
    std::string_view hash_prefix;
    std::string_view data_marker;
    std::string_view end_marker;
};

constexpr MarkerSet SLIC3R_MARKERS{
    "; SLIC3R_EMBEDDED_3MF_BEGIN ",
    "; SLIC3R_EMBEDDED_3MF_SIZE ",
    "; SLIC3R_EMBEDDED_3MF_SHA256 ",
    "; SLIC3R_EMBEDDED_3MF_DATA",
    "; SLIC3R_EMBEDDED_3MF_END",
};

// Read files produced by early Snapmaker Orca builds, but only emit the portable marker namespace above.
constexpr MarkerSet LEGACY_SNAPMAKER_ORCA_MARKERS{
    "; SNAPMAKER_ORCA_EMBEDDED_3MF_BEGIN ",
    "; SNAPMAKER_ORCA_EMBEDDED_3MF_SIZE ",
    "; SNAPMAKER_ORCA_EMBEDDED_3MF_SHA256 ",
    "; SNAPMAKER_ORCA_EMBEDDED_3MF_DATA",
    "; SNAPMAKER_ORCA_EMBEDDED_3MF_END",
};

constexpr std::string_view DATA_PREFIX       = "; ";
constexpr unsigned         VERSION           = 1;
constexpr std::size_t      SOURCE_LINE_BYTES = 57; // Encodes to 76 Base64 characters.

const MarkerSet *marker_set_for_begin(std::string_view line)
{
    if (line.compare(0, SLIC3R_MARKERS.begin_prefix.size(), SLIC3R_MARKERS.begin_prefix) == 0)
        return &SLIC3R_MARKERS;
    if (line.compare(0, LEGACY_SNAPMAKER_ORCA_MARKERS.begin_prefix.size(), LEGACY_SNAPMAKER_ORCA_MARKERS.begin_prefix) == 0)
        return &LEGACY_SNAPMAKER_ORCA_MARKERS;
    return nullptr;
}

class Sha256
{
public:
    Sha256()
        : m_context(EVP_MD_CTX_new(), &EVP_MD_CTX_free)
    {
        if (!m_context || EVP_DigestInit_ex(m_context.get(), EVP_sha256(), nullptr) != 1)
            throw std::runtime_error("Unable to initialize SHA-256");
    }

    void update(const void *data, std::size_t size)
    {
        if (size > 0 && EVP_DigestUpdate(m_context.get(), data, size) != 1)
            throw std::runtime_error("Unable to update SHA-256");
    }

    std::string finish()
    {
        std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
        unsigned int                              size = 0;
        if (EVP_DigestFinal_ex(m_context.get(), digest.data(), &size) != 1 || size != 32)
            throw std::runtime_error("Unable to finalize SHA-256");

        std::ostringstream out;
        out << std::hex << std::setfill('0');
        for (unsigned int i = 0; i < size; ++i)
            out << std::setw(2) << static_cast<unsigned>(digest[i]);
        return out.str();
    }

private:
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> m_context;
};

void add_error(std::string &error, const std::string &message)
{
    if (error.find(message) != std::string::npos)
        return;
    if (!error.empty())
        error += "; ";
    error += message;
}

std::string encode_base64(const unsigned char *data, std::size_t size)
{
    static constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string encoded;
    encoded.reserve(((size + 2) / 3) * 4);
    for (std::size_t i = 0; i < size; i += 3) {
        const std::uint32_t value = (static_cast<std::uint32_t>(data[i]) << 16) |
                                    (i + 1 < size ? static_cast<std::uint32_t>(data[i + 1]) << 8 : 0) |
                                    (i + 2 < size ? static_cast<std::uint32_t>(data[i + 2]) : 0);
        encoded.push_back(alphabet[(value >> 18) & 0x3f]);
        encoded.push_back(alphabet[(value >> 12) & 0x3f]);
        encoded.push_back(i + 1 < size ? alphabet[(value >> 6) & 0x3f] : '=');
        encoded.push_back(i + 2 < size ? alphabet[value & 0x3f] : '=');
    }
    return encoded;
}

int decode_base64_char(char value)
{
    if (value >= 'A' && value <= 'Z')
        return value - 'A';
    if (value >= 'a' && value <= 'z')
        return value - 'a' + 26;
    if (value >= '0' && value <= '9')
        return value - '0' + 52;
    if (value == '+')
        return 62;
    if (value == '/')
        return 63;
    return -1;
}

class Base64Decoder
{
public:
    Base64Decoder(boost::nowide::ofstream &output, Sha256 &sha256, std::uint64_t &decoded_size, std::string &error)
        : m_output(output)
        , m_sha256(sha256)
        , m_decoded_size(decoded_size)
        , m_error(error)
    {}

    void decode_line(std::string_view line)
    {
        if (line.size() > 76)
            add_error(m_error, "Base64 line exceeds 76 characters");

        for (char value : line) {
            if (m_failed)
                return;
            if (m_finished) {
                add_error(m_error, "Base64 data follows padding");
                m_failed = true;
                return;
            }
            if (value != '=' && decode_base64_char(value) < 0) {
                add_error(m_error, "Embedded project contains invalid Base64");
                m_failed = true;
                return;
            }
            m_quartet[m_quartet_size++] = value;
            if (m_quartet_size == m_quartet.size()) {
                decode_quartet();
                m_quartet_size = 0;
            }
        }
    }

    void finish()
    {
        if (m_quartet_size != 0)
            add_error(m_error, "Embedded project has incomplete Base64 padding");
    }

private:
    void decode_quartet()
    {
        if (m_quartet[0] == '=' || m_quartet[1] == '=' || (m_quartet[2] == '=' && m_quartet[3] != '=')) {
            add_error(m_error, "Embedded project has invalid Base64 padding");
            m_failed = true;
            return;
        }

        const int a = decode_base64_char(m_quartet[0]);
        const int b = decode_base64_char(m_quartet[1]);
        const int c = m_quartet[2] == '=' ? 0 : decode_base64_char(m_quartet[2]);
        const int d = m_quartet[3] == '=' ? 0 : decode_base64_char(m_quartet[3]);
        if (a < 0 || b < 0 || c < 0 || d < 0) {
            add_error(m_error, "Embedded project contains invalid Base64");
            m_failed = true;
            return;
        }
        if ((m_quartet[2] == '=' && (b & 0x0f) != 0) || (m_quartet[3] == '=' && (c & 0x03) != 0)) {
            add_error(m_error, "Embedded project has invalid Base64 padding");
            m_failed = true;
            return;
        }

        std::array<unsigned char, 3> decoded{
            static_cast<unsigned char>((a << 2) | (b >> 4)),
            static_cast<unsigned char>((b << 4) | (c >> 2)),
            static_cast<unsigned char>((c << 6) | d),
        };
        std::size_t decoded_count = 3;
        if (m_quartet[2] == '=')
            decoded_count = 1;
        else if (m_quartet[3] == '=')
            decoded_count = 2;

        m_output.write(reinterpret_cast<const char *>(decoded.data()), static_cast<std::streamsize>(decoded_count));
        if (!m_output) {
            add_error(m_error, "Unable to write the extracted project");
            m_failed = true;
            return;
        }
        m_sha256.update(decoded.data(), decoded_count);
        m_decoded_size += decoded_count;
        m_finished = decoded_count != 3;
    }

    boost::nowide::ofstream &m_output;
    Sha256                  &m_sha256;
    std::uint64_t           &m_decoded_size;
    std::string             &m_error;
    std::array<char, 4>      m_quartet{};
    std::size_t              m_quartet_size = 0;
    bool                     m_finished     = false;
    bool                     m_failed       = false;
};

bool parse_size(std::string_view value, std::uint64_t &size)
{
    if (value.empty())
        return false;
    const char *begin = value.data();
    const char *end   = begin + value.size();
    auto [ptr, ec]    = std::from_chars(begin, end, size);
    return ec == std::errc() && ptr == end;
}

bool parse_hash(std::string_view value, std::string &hash)
{
    if (value.size() != 64)
        return false;
    hash.clear();
    hash.reserve(value.size());
    for (char c : value) {
        if (!std::isxdigit(static_cast<unsigned char>(c)))
            return false;
        hash.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return true;
}

void remove_file(const std::string &path)
{
    boost::system::error_code ec;
    boost::filesystem::remove(path, ec);
}

} // namespace

bool append(const std::string &gcode_path, const std::string &project_path, std::string &error)
{
    error.clear();
    boost::system::error_code ec;
    if (!boost::filesystem::is_regular_file(gcode_path, ec) || ec) {
        error = "G-code file does not exist or is not a regular file";
        return false;
    }
    if (!boost::filesystem::is_regular_file(project_path, ec) || ec) {
        error = "3MF project does not exist or is not a regular file";
        return false;
    }

    const std::uint64_t original_size = boost::filesystem::file_size(gcode_path, ec);
    if (ec) {
        error = "Unable to determine the G-code file size: " + ec.message();
        return false;
    }

    try {
        Sha256                  sha256;
        std::uint64_t           project_size = 0;
        std::array<char, 65536> buffer{};
        boost::nowide::ifstream project(project_path, std::ios::binary);
        if (!project) {
            error = "Unable to open the 3MF project";
            return false;
        }
        while (project) {
            project.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            const std::streamsize count = project.gcount();
            if (count > 0) {
                if (project_size > std::numeric_limits<std::uint64_t>::max() - static_cast<std::uint64_t>(count))
                    throw std::runtime_error("3MF project is too large");
                project_size += static_cast<std::uint64_t>(count);
                sha256.update(buffer.data(), static_cast<std::size_t>(count));
            }
        }
        if (!project.eof())
            throw std::runtime_error("Unable to read the 3MF project");
        if (project_size == 0)
            throw std::runtime_error("The 3MF project is empty");
        const std::string hash = sha256.finish();

        boost::nowide::fstream gcode(gcode_path, std::ios::binary | std::ios::in | std::ios::out);
        if (!gcode)
            throw std::runtime_error("Unable to open the G-code file for embedding");

        bool needs_newline = false;
        if (original_size > 0) {
            gcode.seekg(-1, std::ios::end);
            char last = '\0';
            gcode.get(last);
            if (!gcode)
                throw std::runtime_error("Unable to inspect the end of the G-code file");
            needs_newline = last != '\n';
        }
        gcode.clear();
        gcode.seekp(0, std::ios::end);
        if (needs_newline)
            gcode << '\n';
        gcode << SLIC3R_MARKERS.begin_prefix << VERSION << '\n'
              << SLIC3R_MARKERS.size_prefix << project_size << '\n'
              << SLIC3R_MARKERS.hash_prefix << hash << '\n'
              << SLIC3R_MARKERS.data_marker << '\n';

        project.clear();
        project.seekg(0, std::ios::beg);
        std::array<unsigned char, SOURCE_LINE_BYTES> source{};
        while (project) {
            project.read(reinterpret_cast<char *>(source.data()), static_cast<std::streamsize>(source.size()));
            const std::streamsize count = project.gcount();
            if (count > 0)
                gcode << DATA_PREFIX << encode_base64(source.data(), static_cast<std::size_t>(count)) << '\n';
        }
        if (!project.eof())
            throw std::runtime_error("Unable to reread the 3MF project");

        gcode << SLIC3R_MARKERS.end_marker << '\n';
        gcode.flush();
        if (!gcode)
            throw std::runtime_error("Unable to append the embedded project to the G-code file");
        return true;
    } catch (const std::exception &exception) {
        error = exception.what();
        boost::filesystem::resize_file(gcode_path, original_size, ec);
        if (ec)
            error += "; unable to restore the original G-code size: " + ec.message();
        return false;
    }
}

ExtractResult extract(const std::string &gcode_path, const std::string &output_path)
{
    ExtractResult result;
    remove_file(output_path);

    boost::nowide::ifstream input(gcode_path, std::ios::binary);
    if (!input) {
        result.status = ExtractStatus::IoError;
        result.error  = "Unable to open the G-code file while looking for an embedded project";
        return result;
    }

    enum class State { Seeking, Size, Hash, DataMarker, Data, Done };
    State            state       = State::Seeking;
    const MarkerSet *markers     = nullptr;
    bool             found_begin = false;
    bool             saw_size    = false;
    bool             saw_hash    = false;
    bool             saw_data    = false;
    bool             saw_end     = false;

    try {
        Sha256                   sha256;
        boost::nowide::ofstream output;
        std::unique_ptr<Base64Decoder> decoder;
        std::string line;
        while (std::getline(input, line)) {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            if (const MarkerSet *begin_markers = marker_set_for_begin(line)) {
                if (found_begin) {
                    add_error(result.error, "Multiple embedded project markers were found");
                    continue;
                }
                found_begin = true;
                markers     = begin_markers;
                state       = State::Size;
                std::uint64_t version = 0;
                if (!parse_size(std::string_view(line).substr(markers->begin_prefix.size()), version))
                    add_error(result.error, "Embedded project has an invalid format version");
                else if (version != VERSION)
                    add_error(result.error, "Embedded project uses an unsupported format version");
                continue;
            }

            if (!found_begin || state == State::Done)
                continue;

            if (line.compare(0, markers->size_prefix.size(), markers->size_prefix) == 0) {
                if (state != State::Size || saw_size)
                    add_error(result.error, "Embedded project size metadata is out of order");
                saw_size = parse_size(std::string_view(line).substr(markers->size_prefix.size()), result.expected_size);
                if (!saw_size)
                    add_error(result.error, "Embedded project has an invalid size");
                state = State::Hash;
                continue;
            }

            if (line.compare(0, markers->hash_prefix.size(), markers->hash_prefix) == 0) {
                if (state != State::Hash || saw_hash)
                    add_error(result.error, "Embedded project checksum metadata is out of order");
                saw_hash = parse_hash(std::string_view(line).substr(markers->hash_prefix.size()), result.expected_sha256);
                if (!saw_hash)
                    add_error(result.error, "Embedded project has an invalid SHA-256 checksum");
                state = State::DataMarker;
                continue;
            }

            if (line == markers->data_marker) {
                if (state != State::DataMarker || saw_data)
                    add_error(result.error, "Embedded project data marker is out of order");
                saw_data = true;
                state    = State::Data;
                output.open(output_path, std::ios::binary | std::ios::trunc);
                if (!output) {
                    result.status = ExtractStatus::IoError;
                    result.error  = "Unable to create a temporary file for the embedded project";
                    return result;
                }
                decoder = std::make_unique<Base64Decoder>(output, sha256, result.actual_size, result.error);
                continue;
            }

            if (line == markers->end_marker) {
                if (state != State::Data || saw_end)
                    add_error(result.error, "Embedded project end marker is out of order");
                saw_end = true;
                state   = State::Done;
                if (decoder)
                    decoder->finish();
                continue;
            }

            if (state == State::Data) {
                if (line.compare(0, DATA_PREFIX.size(), DATA_PREFIX) != 0) {
                    add_error(result.error, "Embedded project data is not a G-code comment");
                } else if (decoder) {
                    decoder->decode_line(std::string_view(line).substr(DATA_PREFIX.size()));
                }
            } else {
                add_error(result.error, "Embedded project metadata is incomplete or out of order");
            }
        }

        if (!input.eof()) {
            result.status = ExtractStatus::IoError;
            result.error  = "Unable to read the G-code file while extracting its embedded project";
            remove_file(output_path);
            return result;
        }

        if (!found_begin) {
            result.status = ExtractStatus::NotFound;
            return result;
        }
        if (!saw_size)
            add_error(result.error, "Embedded project is missing its size metadata");
        if (!saw_hash)
            add_error(result.error, "Embedded project is missing its checksum metadata");
        if (!saw_data)
            add_error(result.error, "Embedded project is missing its data marker");
        if (!saw_end)
            add_error(result.error, "Embedded project is truncated");
        if (decoder && !saw_end)
            decoder->finish();

        if (output.is_open()) {
            output.flush();
            if (!output)
                add_error(result.error, "Unable to finish writing the extracted project");
            output.close();
        }

        result.actual_sha256    = sha256.finish();
        result.has_project_data = result.actual_size > 0;
        if (!result.has_project_data)
            add_error(result.error, "Embedded project contains no project data");
        if (saw_size && result.expected_size != result.actual_size)
            add_error(result.error, "Embedded project size does not match its metadata");
        if (saw_hash && result.expected_sha256 != result.actual_sha256)
            add_error(result.error, "Embedded project SHA-256 checksum does not match");

        if (!result.has_project_data)
            remove_file(output_path);

        result.status = result.error.empty() ? ExtractStatus::Valid : ExtractStatus::Corrupt;
        return result;
    } catch (const std::exception &exception) {
        result.status = ExtractStatus::IoError;
        result.error  = exception.what();
        remove_file(output_path);
        return result;
    }
}

} // namespace Slic3r::GCodeEmbedded3MF
