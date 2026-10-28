#include "fingerprint/inputs.h"
#include <algorithm>

namespace comskip::fingerprint {
namespace {
double seconds(const json::Value& value, std::string_view what) {
    const double number = value.number(what);
    if (number < 0) throw json::Invalid(std::string(what) + " is negative");
    return number;
}
const std::string* optional_string(const json::Value& document, std::string_view name) {
    const auto* value = document.find(name);
    return value && !value->is_null() ? &value->string(name) : nullptr;
}
}

Evidence evidence_from_json(const json::Value& document) {
    Evidence evidence;
    if (const auto* id = optional_string(document, "captureId")) evidence.capture_id = *id;
    evidence.revision = static_cast<long long>(document.at("revision").number("revision"));
    evidence.final = document.at("final").boolean("final");
    evidence.logo_known = document.at("logoKnown").boolean("logoKnown");
    for (const auto& item : document.at("seams").array("seams")) {
        Seam seam{seconds(item.at("at"), "seam time"), {}};
        for (const auto& kind : item.at("kinds").array("seam kinds")) seam.kinds.push_back(kind.string("seam kind"));
        evidence.seams.push_back(std::move(seam));
    }
    for (const auto& item : document.at("logo").array("logo")) {
        const Interval interval{seconds(item.at("from"), "logo start"), seconds(item.at("to"), "logo end")};
        if (interval.to < interval.from) throw json::Invalid("logo interval ends before it starts");
        evidence.logo.push_back(interval);
    }
    for (const auto& item : document.at("blocks").array("blocks")) {
        Block block{seconds(item.at("from"), "block start"), seconds(item.at("to"), "block end"), false};
        const auto& decision = item.at("decision").string("block decision");
        if (decision == "commercial") block.commercial = true;
        else if (decision != "programme") throw json::Invalid("block decision is neither commercial nor programme");
        if (block.to < block.from) throw json::Invalid("block ends before it starts");
        evidence.blocks.push_back(block);
    }
    std::ranges::sort(evidence.seams, {}, &Seam::at);
    std::ranges::sort(evidence.logo, {}, &Interval::from);
    std::ranges::sort(evidence.blocks, {}, &Block::from);
    return evidence;
}

Context context_from_json(const json::Value& document) {
    Context context;
    if (const auto* channel = optional_string(document, "channel")) context.channel = *channel;
    if (const auto* air = document.find("airStart")) context.air_start = air->number("airStart");
    if (const auto* source = optional_string(document, "source")) {
        if (*source != "capture" && *source != "plex") throw json::Invalid("source is neither capture nor plex");
        context.source = *source;
    }
    if (const auto* programmes = document.find("programmes")) {
        for (const auto& item : programmes->array("programmes")) {
            Programme programme{seconds(item.at("from"), "programme start"), seconds(item.at("to"), "programme end"), {}, {}};
            if (const auto* title = optional_string(item, "title")) programme.title = *title;
            if (const auto* key = optional_string(item, "seriesKey")) programme.series_key = *key;
            if (programme.to < programme.from) throw json::Invalid("programme ends before it starts");
            context.programmes.push_back(std::move(programme));
        }
    }
    std::ranges::sort(context.programmes, {}, &Programme::from);
    return context;
}

std::string evidence_to_json(const Evidence& evidence) {
    std::string text = "{\"captureId\":";
    json::append_string(text, evidence.capture_id);
    text += ",\"revision\":";
    json::append_integer(text, evidence.revision);
    text += evidence.final ? ",\"final\":true" : ",\"final\":false";
    text += evidence.logo_known ? ",\"logoKnown\":true,\"seams\":[" : ",\"logoKnown\":false,\"seams\":[";
    for (std::size_t index = 0; index < evidence.seams.size(); ++index) {
        text += index ? ",{\"at\":" : "{\"at\":";
        json::append_number(text, evidence.seams[index].at, 3);
        text += ",\"kinds\":[";
        for (std::size_t kind = 0; kind < evidence.seams[index].kinds.size(); ++kind) {
            if (kind) text += ',';
            json::append_string(text, evidence.seams[index].kinds[kind]);
        }
        text += "]}";
    }
    text += "],\"logo\":[";
    for (std::size_t index = 0; index < evidence.logo.size(); ++index) {
        text += index ? ",{\"from\":" : "{\"from\":";
        json::append_number(text, evidence.logo[index].from, 3);
        text += ",\"to\":";
        json::append_number(text, evidence.logo[index].to, 3);
        text += '}';
    }
    text += "],\"blocks\":[";
    for (std::size_t index = 0; index < evidence.blocks.size(); ++index) {
        const auto& block = evidence.blocks[index];
        text += index ? ",{\"from\":" : "{\"from\":";
        json::append_number(text, block.from, 3);
        text += ",\"to\":";
        json::append_number(text, block.to, 3);
        text += block.commercial ? ",\"decision\":\"commercial\"}" : ",\"decision\":\"programme\"}";
    }
    text += "]}";
    return text;
}

std::string context_to_json(const Context& context) {
    std::string text = "{\"channel\":";
    if (context.channel.empty()) text += "null";
    else json::append_string(text, context.channel);
    text += ",\"airStart\":";
    json::append_number(text, context.air_start, 3);
    text += ",\"source\":";
    json::append_string(text, context.source);
    text += ",\"programmes\":[";
    for (std::size_t index = 0; index < context.programmes.size(); ++index) {
        const auto& programme = context.programmes[index];
        text += index ? ",{\"from\":" : "{\"from\":";
        json::append_number(text, programme.from, 3);
        text += ",\"to\":";
        json::append_number(text, programme.to, 3);
        text += ",\"title\":";
        json::append_string(text, programme.title);
        text += ",\"seriesKey\":";
        json::append_string(text, programme.series_key);
        text += '}';
    }
    text += "]}";
    return text;
}

const Programme* programme_at(const Context& context, double time) {
    for (const auto& programme : context.programmes)
        if (programme.from <= time && time < programme.to) return &programme;
    return nullptr;
}
}
