// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/outside_change_logic.hpp"

#include <borealis/extern/nlohmann/json.hpp>
#include <cstring>

namespace outside_change_logic
{

namespace
{
int64_t abs64(int64_t v) { return v < 0 ? -v : v; }

bool digits(const std::string& s, size_t from, size_t n)
{
    for (size_t i = from; i < from + n; i++)
        if (i >= s.size() || s[i] < '0' || s[i] > '9') return false;
    return true;
}

bool is_date(const std::string& s)
{
    return s.size() == 10 && digits(s, 0, 4) && s[4] == '-' && digits(s, 5, 2) && s[7] == '-' && digits(s, 8, 2);
}

bool is_hm(const std::string& s)
{
    return s.size() == 5 && digits(s, 0, 2) && s[2] == ':' && digits(s, 3, 2);
}

bool day_ok(int64_t v) { return (v >= 0 && v <= 1440) || v == 0xFFFF; }
}   // namespace

Findings check(Record& rec, const Observation& ob)
{
    Findings f;

    // A notice lasts for the day it was seen.
    if (rec.notice && rec.notice_date != ob.date) {
        rec.notice = 0;
        rec.reset_at.clear();
        f.save = true;
    }

    // Today's play time: down on the same day is a reset (the console's own
    // one comes with a new day).
    if (rec.date != ob.date) {
        rec.date = ob.date;
        rec.spent_s = rec.saved_spent_s = -1;
        f.save = true;
    }
    if (ob.spent_known && ob.spent_s >= 0) {
        if (rec.spent_s >= 0 && ob.spent_s + SPENT_SLACK_S < rec.spent_s) {
            f.reset = true;
            f.spent_before_s = rec.spent_s;
        }
        rec.spent_s = ob.spent_s;
        if (f.reset || rec.saved_spent_s < 0 || abs64(rec.spent_s - rec.saved_spent_s) >= SPENT_SAVE_S) f.save = true;
    }

    // The console clock against the steady clock (the same source only: a
    // new one restarts from its own zero).
    if (ob.clock_known) {
        const bool comparable = rec.offset_known && rec.steady_id == ob.steady_id;
        const int64_t moved = comparable ? ob.offset_s - rec.offset_s : 0;
        if (comparable && abs64(moved) > CLOCK_SLACK_S) {
            f.clock = true;
            f.clock_moved_s = moved;
        }
        if (!comparable || abs64(moved) >= OFFSET_SAVE_S) {
            rec.offset_known = true;
            rec.offset_s = ob.offset_s;
            rec.steady_id = ob.steady_id;
            f.save = true;
        }
    }

    // The limits against the ones PlayGuard last knew.
    if (ob.limits_known) {
        const bool same = rec.limits_known && std::memcmp(rec.limits, ob.limits, sizeof(rec.limits)) == 0;
        if (rec.limits_known && !same) {
            f.limits = true;
            std::memcpy(f.limits_before, rec.limits, sizeof(rec.limits));
            std::memcpy(f.limits_after, ob.limits, sizeof(ob.limits));
        }
        if (!same) {
            rec.limits_known = true;
            std::memcpy(rec.limits, ob.limits, sizeof(rec.limits));
            f.save = true;
        }
    }

    const unsigned seen = (f.reset ? NOTICE_RESET : 0u) | (f.clock ? NOTICE_CLOCK : 0u) | (f.limits ? NOTICE_LIMITS : 0u);
    if (seen) {
        rec.notice |= seen;
        rec.notice_date = ob.date;
        if (f.reset) rec.reset_at = ob.hm;
        f.save = true;
    }
    if (f.save) rec.saved_spent_s = rec.spent_s;
    return f;
}

void own_change(Record& rec)
{
    rec.spent_s = rec.saved_spent_s = -1;
    rec.offset_known = false;
    rec.limits_known = false;
}

void dismiss(Record& rec)
{
    rec.notice = 0;
    rec.reset_at.clear();
}

Record parse(const std::string& text)
{
    Record rec;
    const nlohmann::json j = nlohmann::json::parse(text, nullptr, false);
    if (!j.is_object()) return rec;
    auto str = [&j](const char* key) -> std::string {
        const auto it = j.find(key);
        return it != j.end() && it->is_string() ? it->get<std::string>() : std::string();
    };
    auto num = [&j](const char* key, int64_t* out) {
        const auto it = j.find(key);
        if (it == j.end() || !it->is_number_integer()) return false;
        *out = it->get<int64_t>();
        return true;
    };

    const std::string date = str("date");
    int64_t v = 0;
    if (is_date(date)) {
        rec.date = date;
        if (num("spent_s", &v) && v >= 0 && v <= 86400) rec.spent_s = rec.saved_spent_s = v;
    }
    if (num("offset_s", &v)) {
        rec.offset_known = true;
        rec.offset_s = v;
        rec.steady_id = str("steady_id");
    }
    const auto lim = j.find("limits");
    if (lim != j.end() && lim->is_array() && lim->size() == 7) {
        bool ok = true;
        for (size_t i = 0; i < 7 && ok; i++) {
            ok = (*lim)[i].is_number_integer() && day_ok((*lim)[i].get<int64_t>());
            if (ok) rec.limits[i] = (uint16_t)(*lim)[i].get<int64_t>();
        }
        rec.limits_known = ok;
        if (!ok) std::memset(rec.limits, 0, sizeof(rec.limits));
    }
    const std::string notice_date = str("notice_date");
    if (num("notice", &v) && v > 0 && v <= (NOTICE_RESET | NOTICE_CLOCK | NOTICE_LIMITS) && is_date(notice_date)) {
        rec.notice = (unsigned)v;
        rec.notice_date = notice_date;
        const std::string at = str("reset_at");
        if ((rec.notice & NOTICE_RESET) && is_hm(at)) rec.reset_at = at;
    }
    return rec;
}

std::string serialize(const Record& rec)
{
    nlohmann::json j = { { "schema", 1 } };
    if (!rec.date.empty()) j["date"] = rec.date;
    if (!rec.date.empty() && rec.spent_s >= 0) j["spent_s"] = rec.spent_s;
    if (rec.offset_known) {
        j["offset_s"] = rec.offset_s;
        j["steady_id"] = rec.steady_id;
    }
    if (rec.limits_known) j["limits"] = std::vector<int>(rec.limits, rec.limits + 7);
    if (rec.notice) {
        j["notice"] = rec.notice;
        j["notice_date"] = rec.notice_date;
        if (!rec.reset_at.empty()) j["reset_at"] = rec.reset_at;
    }
    return j.dump(2) + "\n";
}

}   // namespace outside_change_logic
