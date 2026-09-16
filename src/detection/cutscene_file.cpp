#include "cutscene_file.h"

#include <array>
#include <istream>
#include <ostream>

namespace comskip::detection {
namespace {
constexpr std::size_t header_size = 4;
}

std::expected<CutsceneRecord, CutsceneFileError> decode_cutscene(std::span<const std::uint8_t> bytes) {
    if (bytes.size() < header_size) return std::unexpected(CutsceneFileError::missing_header);
    if (bytes.size() == header_size) return std::unexpected(CutsceneFileError::missing_pixels);
    if (bytes.size() - header_size > maximum_cutscene_pixels)
        return std::unexpected(CutsceneFileError::too_many_pixels);
    const auto bits = static_cast<std::uint32_t>(bytes[0]) |
        (static_cast<std::uint32_t>(bytes[1]) << 8) |
        (static_cast<std::uint32_t>(bytes[2]) << 16) |
        (static_cast<std::uint32_t>(bytes[3]) << 24);
    CutsceneRecord record{static_cast<std::int32_t>(bits)};
    record.pixels.assign(bytes.begin() + header_size, bytes.end());
    return record;
}

std::expected<std::vector<std::uint8_t>, CutsceneFileError> encode_cutscene(const CutsceneRecord& record) {
    if (record.pixels.empty()) return std::unexpected(CutsceneFileError::missing_pixels);
    if (record.pixels.size() > maximum_cutscene_pixels)
        return std::unexpected(CutsceneFileError::too_many_pixels);
    const auto bits = static_cast<std::uint32_t>(record.brightness);
    std::vector<std::uint8_t> bytes;
    bytes.reserve(header_size + record.pixels.size());
    for (unsigned shift : {0u, 8u, 16u, 24u}) bytes.push_back(static_cast<std::uint8_t>(bits >> shift));
    bytes.insert(bytes.end(), record.pixels.begin(), record.pixels.end());
    return bytes;
}

std::expected<CutsceneRecord, CutsceneFileError> read_cutscene(std::istream& input) {
    std::array<std::uint8_t, header_size + maximum_cutscene_pixels + 1> bytes{};
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    const auto count = static_cast<std::size_t>(input.gcount());
    if (input.bad()) return std::unexpected(CutsceneFileError::read_failed);
    return decode_cutscene(std::span{bytes}.first(count));
}

std::expected<void, CutsceneFileError> write_cutscene(std::ostream& output, const CutsceneRecord& record) {
    auto bytes = encode_cutscene(record);
    if (!bytes) return std::unexpected(bytes.error());
    output.write(reinterpret_cast<const char*>(bytes->data()), static_cast<std::streamsize>(bytes->size()));
    if (!output) return std::unexpected(CutsceneFileError::write_failed);
    return {};
}
}
