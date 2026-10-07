// ntp_servers — the built-in list of public NTP servers, grouped by region.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>
#include <vector>

namespace ntp
{

struct Region
{
    const char* id;          // also the i18n key suffix: nx_pctl/clock/region/<id>
    std::vector<const char*> hosts;
};

const std::vector<Region>& regions();

// Pool for the console's region setting (Japan, Americas, Europe, Australia,
// Hong Kong/Taiwan/Korea, China), "pool.ntp.org" when unknown.
std::string default_server_for_console();

// Two independent public servers used to cross-check a measurement.
std::vector<std::string> cross_check_servers(const std::string& primary);

}   // namespace ntp
