#include "../localization/diagnostic.h"
#include "a53_caption_bridge.h"
#include <algorithm>
#include <stdexcept>

namespace comskip::media {
std::vector<Ga94CaptionPacket> bridge_a53_captions(std::span<const std::uint8_t> payload) {
    if (payload.size() % 3 != 0) throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::malformed_a53_caption_triplets);
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
std::vector<std::uint8_t> extract_a53_captions(std::span<const std::uint8_t> packet) {
    std::vector<std::uint8_t> result;
    if (packet.size() < 4) return result;
    if (std::equal(packet.begin(), packet.begin() + 4, "GA94")) {
        if (packet.size() < 7 || packet[4] != 3) throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::malformed_ga94_caption_header);
        const auto length = (packet[5] & 0x1f) * 3u;
        if (length > packet.size() - 7) throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::truncated_ga94_captions);
        if (packet[5] & 0x40) result.assign(packet.begin() + 7, packet.begin() + 7 + length);
    } else if (packet[0] == 'C' && packet[1] == 'C' && packet[2] == 1 && packet[3] == 0xf8) {
        if (packet.size() < 5) throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::truncated_dvd_caption_header);
        const auto count = (packet[4] & 0x1e) / 2;
        if (packet.size() - 5 < static_cast<std::size_t>(count) * 6)
            throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::truncated_dvd_caption_pairs);
        const int first_field = (packet[4] & 0x80) ? 0 : 1;
        auto payload = packet.subspan(5);
        while (payload.size() >= 6 && (payload[0] == 0xfe || payload[0] == 0xff)) {
            for (int field = 0; field < 2; ++field) {
                const auto offset = field * 3;
                result.push_back(payload[offset] == 0xff && field == first_field ? 0xfc : 0xfd);
                result.push_back(payload[offset + 1]); result.push_back(payload[offset + 2]);
            }
            payload = payload.subspan(6);
        }
        if (!payload.empty() && (payload[0] == 0xfe || payload[0] == 0xff) && payload.size() != 1)
            throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::truncated_extra_dvd_captions);
    } else if ((packet[0] == 0xbb && packet[1] == 2) || (packet[2] == 0x99 && packet[3] == 2)) {
        if (packet.size() < 8) throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::truncated_replaytv_captions);
        result = {0xfc, packet[6], packet[7], 0xfd, packet[2], packet[3]};
    }
    return result;
}
}
