// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/sync_orders.hpp"

#include <borealis.hpp>
#include <fmt/format.h>
#include <vector>

#include "action/history_flow.hpp"
#include "action/pin_lock.hpp"
#include "action/pt_flow.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/profiles.hpp"
#include "util/sync_files.hpp"

using namespace brls::literals;

namespace sync_orders
{

namespace
{
std::string hm(int hour, int minute)
{
    return fmt::format("{:02d}:{:02d}", hour, minute);
}

std::string on_off(const char* on_key, const char* off_key, bool on)
{
    return brls::getStr(on ? on_key : off_key);
}

// A saved profile's limits, by its display name (sync_exec's callback).
bool profile_days(void*, const char* name, uint16_t days[7])
{
    for (const auto& p : profiles::list())
        if (p.name == name) {
            for (int d = 0; d < 7; d++) days[d] = p.days[(size_t)d];
            return true;
        }
    return false;
}

std::vector<int> values(const int* v, int n)
{
    return std::vector<int>(v, v + n);
}

// The change history, as for the same change made on the console.
void record(const Order& o, const SyncOutcome& out, const PtState& before)
{
    if (!out.applied || !out.changed) return;
    const std::string source = out.source ? out.source : "remote";
    const std::string detail = o.intent.kind == SyncIntent_Profile ? std::string(o.intent.profile) : std::string();
    switch (out.change) {
        case SyncChange_Limits:
            history_flow::record_values("limits", values(out.before, 7), values(out.after, 7), source, detail);
            break;
        case SyncChange_Level:
            if (out.before[0] >= 0) history_flow::record_values("level", { out.before[0] }, { out.after[0] }, "remote");
            break;
        case SyncChange_Custom:
            history_flow::record_values("custom", values(out.before, 3), values(out.after, 3), "remote");
            break;
        case SyncChange_Vr:
            if (out.before[0] >= 0) history_flow::record_values("vr", { out.before[0] }, { out.after[0] }, "remote");
            break;
        case SyncChange_Alarm:
            if (out.before[0] >= 0) history_flow::record_values("alarm", { out.before[0] }, { out.after[0] }, "remote");
            break;
        case SyncChange_Bedtime: {
            PtState after;
            pctl_play_timer_query(&after);
            if (!before.valid || !after.valid) break;
            const std::string from = pt_flow::bedtime_text(before), to = pt_flow::bedtime_text(after);
            if (from != to) history_flow::record_event("bedtime", "remote", from + " → " + to);
            break;
        }
        case SyncChange_Unlock: history_flow::record_event("unlock", "remote"); break;
        case SyncChange_Relock: history_flow::record_event("relock", "remote"); break;
        default: break;
    }
    if (o.intent.kind == SyncIntent_ConsoleLock && out.console_lock_after >= 0)
        history_flow::record_values("console_lock", { out.console_lock_after ? 0 : 1 }, { out.console_lock_after },
                                    "remote");
}

SyncOutcome execute(const Order& o, bool remote_timer_writes)
{
    auto& cfg = config::get();
    SyncRecords rec = sync_files::records_from(cfg);
    const std::string today = ui::today_date();
    SyncExecCtx ctx{};
    ctx.remote_timer_writes = remote_timer_writes;
    ctx.weekday = ui::today_weekday();
    ctx.today = today.c_str();
    ctx.rec = &rec;
    ctx.profile_days = profile_days;
    ctx.ctx = nullptr;

    PtState before{};
    const SyncIntentKind k = o.intent.kind;
    if (k == SyncIntent_Bedtime || k == SyncIntent_BedtimeEnd || k == SyncIntent_BedtimeEnabled)
        pctl_play_timer_query(&before);

    SyncOutcome out;
    sync_exec(&o.intent, &ctx, &out);
    if (sync_files::records_into(rec, cfg)) ui::save_config();
    record(o, out, before);
    brls::Logger::info("sync: {}={} -> rc 0x{:08X} {}", o.entity, o.payload, (unsigned)out.rc,
                       out.applied ? "applied" : sync_reason_name(out.reason));
    return out;
}

// Lock now and a report change nothing a child would want: never asked.
bool asks(SyncIntentKind k)
{
    return k != SyncIntent_LockNow;
}
}   // namespace

std::string describe(const SyncIntent& in)
{
    switch (in.kind) {
        case SyncIntent_LimitDay:
            return brls::getStr("playguard/sync/orders/limit_day", ui::day_name_in_text(in.day), ui::fmt_minutes(in.minutes));
        case SyncIntent_LimitUniform:
            return brls::getStr("playguard/sync/orders/limit_uniform", ui::fmt_minutes(in.minutes));
        case SyncIntent_LimitsWeek: {
            if (in.mask == 0x7F) return brls::getStr("playguard/sync/orders/limits_week", ui::days_summary(in.days));
            std::string list;
            for (int d = 0; d < 7; d++)
                if (in.mask & (1u << d))
                    list += (list.empty() ? "" : ", ") +
                            brls::getStr("playguard/common/line", ui::day_name(d), ui::fmt_minutes(in.days[d]));
            return brls::getStr("playguard/sync/orders/limits_days", list);
        }
        case SyncIntent_LimitToday:
            return brls::getStr("playguard/sync/orders/limit_today", ui::fmt_minutes(in.minutes));
        case SyncIntent_RemoveLimit: return "playguard/sync/orders/remove_limit"_i18n;
        case SyncIntent_ConsoleLock:
            return on_off("playguard/sync/orders/console_lock_on", "playguard/sync/orders/console_lock_off", in.on);
        case SyncIntent_BonusTime: return brls::getStr("playguard/sync/orders/bonus", ui::fmt_played(in.minutes));
        case SyncIntent_StopToday: return "playguard/sync/orders/stop_today"_i18n;
        case SyncIntent_Unlock: return "playguard/sync/orders/unlock"_i18n;
        case SyncIntent_LockNow: return "playguard/sync/orders/lock_now"_i18n;
        case SyncIntent_Alarm: return on_off("playguard/sync/orders/alarm_on", "playguard/sync/orders/alarm_off", in.on);
        case SyncIntent_BedtimeEnabled:
            return on_off("playguard/sync/orders/bedtime_on", "playguard/sync/orders/bedtime_off", in.on);
        case SyncIntent_Bedtime: return brls::getStr("playguard/sync/orders/bedtime", hm(in.hour, in.minute));
        case SyncIntent_BedtimeEnd: return brls::getStr("playguard/sync/orders/bedtime_end", hm(in.hour, in.minute));
        case SyncIntent_Profile: return brls::getStr("playguard/sync/orders/profile", std::string(in.profile));
        case SyncIntent_Level: return brls::getStr("playguard/sync/orders/level", ui::level_name(in.level));
        case SyncIntent_Vr: return on_off("playguard/sync/orders/vr_on", "playguard/sync/orders/vr_off", in.on);
        case SyncIntent_Sns: return on_off("playguard/sync/orders/sns_on", "playguard/sync/orders/sns_off", in.on);
        case SyncIntent_Comm: return on_off("playguard/sync/orders/comm_on", "playguard/sync/orders/comm_off", in.on);
        case SyncIntent_ExportReport: return "playguard/sync/orders/export_report"_i18n;
        default: return "?";
    }
}

void run(const Order& o, SyncPolicy policy, bool remote_timer_writes, std::function<void(const SyncOutcome&)> done)
{
    if (policy == SyncPolicy_Auto || !asks(o.intent.kind)) {
        // The broker authenticated the sender: Security's "ask for the PIN
        // before a change" is for someone at the console.
        pin_lock::remote_bypass(true);
        const SyncOutcome out = execute(o, remote_timer_writes);
        pin_lock::remote_bypass(false);
        done(out);
        return;
    }
    const std::string body = brls::getStr("playguard/sync/ask_body", describe(o.intent));
    ui::confirm(body, "playguard/sync/ask_confirm"_i18n,
                [o, remote_timer_writes, done]() { done(execute(o, remote_timer_writes)); },
                [done]() {
                    SyncOutcome out;
                    sync_outcome_init(&out);
                    out.reason = SyncReason_NotConfirmed;
                    done(out);
                });
}

void tell(const Order& o, const SyncOutcome& out)
{
    const std::string what = describe(o.intent);
    if (out.relock_failed) {
        ui::error(brls::getStr("playguard/sync/done", what) + "\n\n" + "playguard/toast/relock_err"_i18n);
        return;
    }
    if (out.applied) {
        ui::notify(brls::getStr("playguard/sync/done", what));
        return;
    }
    if (out.reason == SyncReason_NotConfirmed && !out.rc) return;   // declined here: nothing to say
    std::string why = out.reason == SyncReason_NotConfirmed
                          ? pin_lock::refusal_text()   // the PIN screen's answer
                          : brls::getStr(std::string("playguard/sync/reasons/") + sync_reason_name(out.reason));
    if (out.reason == SyncReason_PctlError && out.rc) why += " — " + ui::rc_text(out.rc);
    ui::notify(brls::getStr("playguard/sync/refused", what, why));
}

}   // namespace sync_orders
