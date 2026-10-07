// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "util/profiles.hpp"

#include <borealis/extern/nlohmann/json.hpp>
#include <cstdio>

#include "util/paths.hpp"

namespace profiles
{

namespace
{
std::vector<Profile> g_cache;
bool g_cache_valid = false;

// Next code point of `s` at `i` (advanced past it); false at the end. An
// invalid sequence gives 0xFFFD and skips one byte.
bool next_cp(const std::string& s, size_t& i, uint32_t& cp)
{
    if (i >= s.size()) return false;
    const unsigned char c = (unsigned char)s[i];
    int len = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
    if (len == 0 || i + len > s.size()) {
        i++;
        cp = 0xFFFD;
        return true;
    }
    cp = len == 1 ? c : c & (0x7F >> len);
    for (int k = 1; k < len; k++) {
        const unsigned char cc = (unsigned char)s[i + k];
        if ((cc & 0xC0) != 0x80) {
            i++;
            cp = 0xFFFD;
            return true;
        }
        cp = (cp << 6) | (cc & 0x3F);
    }
    // Overlong forms and surrogates are not characters.
    if ((len == 2 && cp < 0x80) || (len == 3 && cp < 0x800) || (len == 4 && (cp < 0x10000 || cp > 0x10FFFF)) ||
        (cp >= 0xD800 && cp <= 0xDFFF))
        cp = 0xFFFD;
    i += len;
    return true;
}

void append_utf8(std::string& out, uint32_t cp)
{
    if (cp < 0x80) out += (char)cp;
    else if (cp < 0x800) {
        out += (char)(0xC0 | (cp >> 6));
        out += (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += (char)(0xE0 | (cp >> 12));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
    } else {
        out += (char)(0xF0 | (cp >> 18));
        out += (char)(0x80 | ((cp >> 12) & 0x3F));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
    }
}

// U+00C0..U+00FF without their accent ("" when there is no plain letter).
const char* LATIN1[64] = {
    "A", "A", "A", "A", "A", "A", "AE", "C", "E", "E", "E", "E", "I", "I", "I", "I",
    "D", "N", "O", "O", "O", "O", "O", "",  "O", "U", "U", "U", "U", "Y", "TH", "ss",
    "a", "a", "a", "a", "a", "a", "ae", "c", "e", "e", "e", "e", "i", "i", "i", "i",
    "d", "n", "o", "o", "o", "o", "o", "",  "o", "u", "u", "u", "u", "y", "th", "y",
};

std::string lower_ascii(std::string s)
{
    for (auto& c : s)
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    return s;
}

std::string path_of(const std::string& stem)
{
    return paths::profiles_dir() + "/" + stem + ".json";
}
}   // namespace

std::string sanitize_name(const std::string& name)
{
    std::string out;
    size_t i = 0, chars = 0;
    uint32_t cp = 0;
    while (chars < MAX_NAME && next_cp(name, i, cp)) {
        if (cp < 0x20 || (cp >= 0x7F && cp < 0xA0) || cp == '/' || cp == '\\' || cp == 0xFFFD) continue;
        if (cp == ' ' && (out.empty() || out.back() == ' ')) continue;   // no leading or doubled spaces
        append_utf8(out, cp);
        chars++;
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

std::string file_stem(const std::string& name)
{
    std::string out;
    size_t i = 0;
    uint32_t cp = 0;
    const std::string clean = sanitize_name(name);
    while (next_cp(clean, i, cp)) {
        if ((cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') || (cp >= '0' && cp <= '9') || cp == ' ' ||
            cp == '-' || cp == '_')
            out += (char)cp;
        else if (cp >= 0xC0 && cp <= 0xFF) out += LATIN1[cp - 0xC0];
        else if (cp == 0x152) out += "OE";
        else if (cp == 0x153) out += "oe";
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    while (!out.empty() && out.front() == ' ') out.erase(out.begin());
    return out;
}

std::vector<Profile> list()
{
    std::vector<Profile> out;
    for (const auto& file : paths::list_files(paths::profiles_dir(), ".json")) {
        std::string text;
        if (!paths::read_file(paths::profiles_dir() + "/" + file, text)) continue;
        try {
            auto j = nlohmann::json::parse(text);
            if (!j.is_object()) continue;
            Profile p;
            p.file = file.substr(0, file.size() - 5);
            auto name = j.find("name");
            if (name != j.end() && name->is_string()) p.name = sanitize_name(name->get<std::string>());
            if (p.name.empty()) p.name = sanitize_name(p.file);
            auto days = j.find("days");
            if (days == j.end() || !days->is_array() || days->size() != 7) continue;
            bool ok = true;
            for (int i = 0; i < 7 && ok; i++) {
                const auto& d = (*days)[i];
                if (d.is_null()) {
                    p.days[i] = 0xFFFF;
                    continue;
                }
                // Whole minutes only: 90.5 is a damaged file, not 90.
                ok = d.is_number_integer() && d.get<long long>() >= 0 && d.get<long long>() <= 1440;
                if (ok) p.days[i] = (uint16_t)d.get<long long>();
            }
            if (ok && !p.name.empty()) out.push_back(p);
        } catch (...) {
            // ignore unreadable profile files
        }
    }
    return out;
}

bool save(const Profile& p, std::string* error)
{
    const std::string name = sanitize_name(p.name);
    std::string stem = file_stem(name);
    if (name.empty() || stem.empty()) {
        if (error) *error = "Invalid name";
        return false;
    }
    // The same file under another case ("School week" saved as "school
    // week"): FAT sees one file, so write to the one that exists.
    const bool same_file = !p.file.empty() && lower_ascii(p.file) == lower_ascii(stem);
    if (same_file) stem = p.file;

    nlohmann::json j;
    j["name"] = name;
    j["days"] = nlohmann::json::array();
    for (auto d : p.days) {
        if (d == 0xFFFF) j["days"].push_back(nullptr);
        else j["days"].push_back(d);
    }
    g_cache_valid = false;
    if (!paths::atomic_write(path_of(stem), j.dump(2, ' ', false, nlohmann::json::error_handler_t::replace) + "\n", error))
        return false;
    if (!p.file.empty() && !same_file) std::remove(path_of(p.file).c_str());   // renamed
    return true;
}

bool remove(const std::string& file)
{
    g_cache_valid = false;
    return !file.empty() && file.find('/') == std::string::npos && std::remove(path_of(file).c_str()) == 0;
}

bool find_same_file(const std::string& name, const std::string& except_file, Profile* out)
{
    const std::string stem = lower_ascii(file_stem(name));
    if (stem.empty()) return false;
    for (const auto& p : list()) {
        if (lower_ascii(p.file) != stem || (!except_file.empty() && lower_ascii(p.file) == lower_ascii(except_file)))
            continue;
        if (out) *out = p;
        return true;
    }
    return false;
}

static const std::vector<Profile>& cached()
{
    if (!g_cache_valid) {
        g_cache = list();
        g_cache_valid = true;
    }
    return g_cache;
}

size_t count()
{
    return cached().size();
}

std::string match(const uint16_t days[7])
{
    for (const auto& p : cached()) {
        bool same = true;
        for (int i = 0; i < 7 && same; i++) same = p.days[i] == days[i];
        if (same) return p.name;
    }
    return "";
}

}   // namespace profiles
