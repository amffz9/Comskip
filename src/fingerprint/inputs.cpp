#include "fingerprint/inputs.h"
#include <algorithm>

namespace comskip::fingerprint {
namespace {
double seconds(const json::Value& value, std::string_view what) {
    if (!value.is_number()) throw json::Invalid(std::string(what) + " is not a number");
    const double number = value.get<double>();
    if (number < 0) throw json::Invalid(std::string(what) + " is negative");
    return number;
}
// A string member that may be absent or null.
const std::string* optional_string(const json::Value& document, const char* name) {
    const auto found = document.find(name);
    return found == document.end() || found->is_null() ? nullptr : &found->get_ref<const std::string&>();
}
double time(double value) { return json::rounded(value, 3); }
}

Evidence evidence_from_json(const json::Value& document) {
    return json::reading("evidence", [&] {
        Evidence evidence;
        if (const auto* id = optional_string(document, "captureId")) evidence.capture_id = *id;
        evidence.revision = document.at("revision").get<long long>();
        evidence.final = document.at("final").get<bool>();
        evidence.logo_known = document.at("logoKnown").get<bool>();
        for (const auto& item : document.at("seams")) {
            Seam seam{seconds(item.at("at"), "seam time"), {}};
            for (const auto& kind : item.at("kinds")) seam.kinds.push_back(kind.get<std::string>());
            evidence.seams.push_back(std::move(seam));
        }
        for (const auto& item : document.at("logo")) {
            const Interval interval{seconds(item.at("from"), "logo start"), seconds(item.at("to"), "logo end")};
            if (interval.to < interval.from) throw json::Invalid("logo interval ends before it starts");
            evidence.logo.push_back(interval);
        }
        for (const auto& item : document.at("blocks")) {
            Block block{seconds(item.at("from"), "block start"), seconds(item.at("to"), "block end"), false};
            const auto& decision = item.at("decision").get_ref<const std::string&>();
            if (decision == "commercial") block.commercial = true;
            else if (decision != "programme") throw json::Invalid("block decision is neither commercial nor programme");
            if (block.to < block.from) throw json::Invalid("block ends before it starts");
            evidence.blocks.push_back(block);
        }
        std::ranges::sort(evidence.seams, {}, &Seam::at);
        std::ranges::sort(evidence.logo, {}, &Interval::from);
        std::ranges::sort(evidence.blocks, {}, &Block::from);
        return evidence;
    });
}

Context context_from_json(const json::Value& document) {
    return json::reading("context", [&] {
        Context context;
        if (const auto* channel = optional_string(document, "channel")) context.channel = *channel;
        if (const auto air = document.find("airStart"); air != document.end()) context.air_start = air->get<double>();
        if (const auto* source = optional_string(document, "source")) {
            if (*source != "capture" && *source != "plex") throw json::Invalid("source is neither capture nor plex");
            context.source = *source;
        }
        if (const auto programmes = document.find("programmes"); programmes != document.end()) {
            for (const auto& item : *programmes) {
                Programme programme{seconds(item.at("from"), "programme start"), seconds(item.at("to"), "programme end"), {}, {}};
                if (const auto* title = optional_string(item, "title")) programme.title = *title;
                if (const auto* key = optional_string(item, "seriesKey")) programme.series_key = *key;
                if (programme.to < programme.from) throw json::Invalid("programme ends before it starts");
                context.programmes.push_back(std::move(programme));
            }
        }
        std::ranges::sort(context.programmes, {}, &Programme::from);
        return context;
    });
}

std::string evidence_to_json(const Evidence& evidence) {
    json::Value document = {{"captureId", evidence.capture_id},
                            {"revision", evidence.revision},
                            {"final", evidence.final},
                            {"logoKnown", evidence.logo_known},
                            {"seams", json::Value::array()},
                            {"logo", json::Value::array()},
                            {"blocks", json::Value::array()}};
    for (const auto& seam : evidence.seams) document["seams"].push_back({{"at", time(seam.at)}, {"kinds", seam.kinds}});
    for (const auto& logo : evidence.logo) document["logo"].push_back({{"from", time(logo.from)}, {"to", time(logo.to)}});
    for (const auto& block : evidence.blocks)
        document["blocks"].push_back(
            {{"from", time(block.from)}, {"to", time(block.to)}, {"decision", block.commercial ? "commercial" : "programme"}});
    return json::write(document);
}

std::string context_to_json(const Context& context) {
    json::Value document = {{"channel", nullptr}, {"airStart", time(context.air_start)}, {"source", context.source},
                            {"programmes", json::Value::array()}};
    if (!context.channel.empty()) document["channel"] = context.channel;
    for (const auto& programme : context.programmes)
        document["programmes"].push_back({{"from", time(programme.from)},
                                          {"to", time(programme.to)},
                                          {"title", programme.title},
                                          {"seriesKey", programme.series_key}});
    return json::write(document);
}

const Programme* programme_at(const Context& context, double time) {
    for (const auto& programme : context.programmes)
        if (programme.from <= time && time < programme.to) return &programme;
    return nullptr;
}
}
