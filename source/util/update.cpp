// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/update.hpp"

#include <borealis/extern/nlohmann/json.hpp>

namespace update
{

static bool parse_triplet(const std::string& text, int out[3], bool* suffixed)
{
    size_t i = (!text.empty() && (text[0] == 'v' || text[0] == 'V')) ? 1 : 0;
    for (int part = 0; part < 3; part++) {
        if (part > 0) {
            if (i >= text.size() || text[i] != '.') return false;
            i++;
        }
        if (i >= text.size() || text[i] < '0' || text[i] > '9') return false;
        long v = 0;
        while (i < text.size() && text[i] >= '0' && text[i] <= '9') {
            v = v * 10 + (text[i] - '0');
            if (v > 0xFFFF) return false;
            i++;
        }
        out[part] = (int)v;
    }
    // Anything after the third number must be a suffix ("-dev", "+abc").
    if (i < text.size() && text[i] != '-' && text[i] != '+') return false;
    if (suffixed) *suffixed = i < text.size();
    return true;
}

bool parse_version(const std::string& text, int out[3])
{
    return parse_triplet(text, out, nullptr);
}

int compare_versions(const std::string& a, const std::string& b)
{
    int va[3], vb[3];
    bool sa = false, sb = false;
    const bool oka = parse_triplet(a, va, &sa);
    const bool okb = parse_triplet(b, vb, &sb);
    if (!oka || !okb) return (int)oka - (int)okb;
    for (int i = 0; i < 3; i++)
        if (va[i] != vb[i]) return va[i] < vb[i] ? -1 : 1;
    return (int)sb - (int)sa;   // "1.0.0-dev" < "1.0.0"
}

uint32_t parse_firmware(const std::string& text)
{
    int v[3];
    bool suffixed = false;
    if (!parse_triplet(text, v, &suffixed) || suffixed) return 0;
    if (v[0] > 255 || v[1] > 255 || v[2] > 255) return 0;
    return ((uint32_t)v[0] << 16) | ((uint32_t)v[1] << 8) | (uint32_t)v[2];
}

bool parse_compat(const std::string& json, Latest* out)
{
    try {
        const auto j = nlohmann::json::parse(json);
        if (!j.is_object() || !j.contains("version") || !j.contains("fw_tested_max")) return false;
        if (!j["version"].is_string() || !j["fw_tested_max"].is_string()) return false;
        Latest l;
        l.version       = j["version"].get<std::string>();
        l.fw_tested_max = parse_firmware(j["fw_tested_max"].get<std::string>());
        int v[3];
        if (!parse_version(l.version, v) || !l.fw_tested_max) return false;
        *out = l;
        return true;
    } catch (...) {
        return false;
    }
}

Verdict decide(uint32_t console_fw, const std::string& app_version, const Latest& latest)
{
    if (compare_versions(latest.version, app_version) <= 0) return Verdict::UpToDate;
    return console_fw && console_fw <= latest.fw_tested_max ? Verdict::UpdateSupports : Verdict::UpdateNoSupport;
}

}   // namespace update
