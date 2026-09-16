#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace comskip::media {
inline constexpr std::size_t ga94_max_triplets = 31;
inline constexpr std::size_t ga94_max_packet_size = 7 + ga94_max_triplets * 3 + 1;
struct Ga94CaptionPacket {
    std::array<std::uint8_t, ga94_max_packet_size> bytes{};
    std::size_t size{};
};
// A53 side data consists of intact three-byte caption triplets. Wrap every
// triplet in GA94 user-data packets with the five-bit count and marker byte.
// Oversized payloads are split without reordering or discarding 608/XDS/708
// data. Empty input produces no packets; malformed lengths throw.
std::vector<Ga94CaptionPacket> bridge_a53_captions(std::span<const std::uint8_t> payload);
// Decode the framing of persisted GA94/DVD/ReplayTV caption packets into raw
// triplets. Legacy GA94 dumps can omit the final marker. Unrecognized formats
// contain no supported subtitle data; recognized truncated packets throw.
std::vector<std::uint8_t> extract_a53_captions(std::span<const std::uint8_t> packet);
}
