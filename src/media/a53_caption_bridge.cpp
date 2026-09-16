#include "a53_caption_bridge.h"
#include <algorithm>
#include <stdexcept>

namespace comskip::media {
std::vector<Ga94CaptionPacket> bridge_a53_captions(std::span<const std::uint8_t> payload) {
    if (payload.size() % 3 != 0) throw std::invalid_argument("Malformed A53 caption triplets");
    const auto triplets = payload.size() / 3;
    std::vector<Ga94CaptionPacket> packets;
    packets.reserve(triplets / ga94_max_triplets + (triplets % ga94_max_triplets != 0));
    while (!payload.empty()) {
        const auto count = std::min(payload.size() / 3, ga94_max_triplets);
        auto& packet = packets.emplace_back();
        std::copy_n("GA94", 4, packet.bytes.begin());
        packet.bytes[4] = 3;
        packet.bytes[5] = static_cast<std::uint8_t>(0x40 | count);
        packet.bytes[6] = 0;
        std::copy_n(payload.begin(), count * 3, packet.bytes.begin() + 7);
        packet.bytes[7 + count * 3] = 0xff;
        packet.size = 8 + count * 3;
        payload = payload.subspan(count * 3);
    }
    return packets;
}
}
