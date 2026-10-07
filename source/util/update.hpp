// update — is there a newer PlayGuard, and does it support this firmware?
//
// Every release publishes compat.json next to playguard.nro (written by
// tools/gen_compat.py from CMakeLists.txt and core/sysinfo.h):
//   {"schema": 1, "version": "1.1.0", "fw_tested_max": "24.0.0", "fw_min_play_timer": "21.0.0"}
// Parsing and the decision are plain C++ (host-tested); check() fetches the
// file of the latest release over HTTPS (update_check.cpp).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <string>

namespace update
{

struct Latest
{
    std::string version;            // "1.1.0"
    uint32_t    fw_tested_max = 0;  // MAKEHOSVERSION layout: major << 16 | minor << 8 | micro
};

enum class Verdict
{
    Unknown,           // could not tell (offline, release without compat.json, bad file)
    UpToDate,          // this is already the newest release
    UpdateSupports,    // a newer release supports this console's firmware
    UpdateNoSupport,   // a newer release exists, but not for this firmware yet
};

struct Result
{
    Verdict     verdict = Verdict::Unknown;
    Latest      latest;
    std::string error;   // why the check failed (Unknown only), in English
};

// "1.2.3" (optionally "v1.2.3" or "1.2.3-dev") -> major, minor, micro.
bool     parse_version(const std::string& text, int out[3]);
// <0, 0 or >0 like strcmp. A suffixed version ("1.0.0-dev") sorts before the
// plain release with the same numbers; unparsable versions sort first.
int      compare_versions(const std::string& a, const std::string& b);
// "24.0.0" -> MAKEHOSVERSION(24, 0, 0); 0 when malformed.
uint32_t parse_firmware(const std::string& text);
// Reads compat.json; false when a required field is missing or malformed.
bool     parse_compat(const std::string& json, Latest* out);
Verdict  decide(uint32_t console_fw, const std::string& app_version, const Latest& latest);

// <repository>/releases/latest/download/compat.json
std::string compat_url();
// Fetches compat.json of the latest release and decides. Blocking (network):
// run it with brls::async. On the desktop build PLAYGUARD_SIM_LATEST
// ("1.1.0:24.0.0" or "offline") replaces the network.
Result      check(uint32_t console_fw);

}   // namespace update
