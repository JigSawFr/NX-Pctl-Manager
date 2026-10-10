// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/sync_activity.hpp"

#include <cstring>
#include <fmt/format.h>
#include <string>
#include <vector>

#include "action/pin_lock.hpp"
#include "action/sync_flow.hpp"
#include "sync/sync_net.h"
#include "ui/ui.hpp"
#include "util/paths.hpp"
#include "util/pctl_ops_c.hpp"
#include "util/sync_files.hpp"

extern "C" {
#include "core/platform.h"
}

using namespace brls::literals;

namespace
{
const SyncPolicy POLICIES[] = { SyncPolicy_Ask, SyncPolicy_Auto, SyncPolicy_Off };

std::string policy_text(SyncPolicy p)
{
    return brls::getStr(std::string("playguard/sync/policies/") + sync_policy_name(p));
}

template <size_t N>
void set_text(char (&field)[N], const std::string& value)
{
    std::snprintf(field, N, "%s", value.c_str());
}

std::string or_none(const char* value)
{
    return value[0] ? std::string(value) : "playguard/sync/none"_i18n;
}

// When `then_ms` (the link's clock) was: "14:03" today, else the date and
// time; "" when unknown.
std::string since_text(uint64_t then_ms)
{
    if (!then_ms) return "";
    const uint64_t now = sync_now_ms();
    const uint64_t ago_s = now > then_ms ? (now - then_ms) / 1000 : 0;
    u64 posix = 0;
    if (!time_local_now(&posix, nullptr) || !posix) return "";
    std::string text = ui::time_text(posix - ago_s);   // "2026-10-08 14:03:12"
    const std::string today = ui::today_date() + " ";
    if (text.size() >= today.size() + 5 && text.compare(0, today.size(), today) == 0)
        text = text.substr(today.size(), 5);
    return text;
}

// The settings' problem, worded ("" when ready).
std::string problem_text(const SyncConf& c)
{
    const char* p = sync_conf_problem(&c);
    if (!p) return "";
    if (!c.host[0]) return "playguard/sync/problem_host"_i18n;
    if (!c.username[0] && !c.allow_anonymous) return "playguard/sync/problem_user"_i18n;
    return p;
}
}   // namespace

void SyncActivity::onContentAvailable()
{
    intro->setSingleLine(false);
    note->setSingleLine(false);
    this->conf = sync_files::load();

    enabled->init("playguard/sync/enabled"_i18n, conf.enabled, [this](bool on) {
        SyncConf next = this->conf;
        next.enabled = on;
        if (on) {
            uint8_t random[4] = { 0, 0, 0, 0 };
            if (!platform_random(random, sizeof(random))) {
                ui::error("playguard/sync/no_random"_i18n);
                enabled->setOn(false, false);
                return;
            }
            sync_files::ensure_id(next, random);
        }
        if (!this->commit(next)) enabled->setOn(this->conf.enabled, false);
    });

    host->registerClickAction([this](brls::View*) {
        ui::prompt_text("playguard/sync/host_prompt"_i18n, conf.host, SYNC_HOST_MAX - 1, [this](std::string text) {
            SyncConf next = this->conf;
            set_text(next.host, text);
            this->commit(next);
        });
        return true;
    });

    port->registerClickAction([this](brls::View*) {
        ui::prompt_text("playguard/sync/port_prompt"_i18n, std::to_string(conf.port), 5, [this](std::string text) {
            uint32_t value = 0;
            if (!sync_parse_uint(text.c_str(), 1, 65535, &value)) {
                ui::error("playguard/sync/port_invalid"_i18n);
                return;
            }
            SyncConf next = this->conf;
            // 8883 is MQTT over TLS by convention.
            if (value == 8883 && next.port != 8883) next.tls = true;
            next.port = (uint16_t)value;
            this->commit(next);
        });
        return true;
    });

    tls->init("playguard/sync/tls"_i18n, conf.tls, [this](bool on) {
        SyncConf next = this->conf;
        next.tls = on;
        if (!this->commit(next)) tls->setOn(this->conf.tls, false);
    });

    user->registerClickAction([this](brls::View*) {
        const std::vector<std::string> options = { "playguard/sync/user_enter"_i18n, "playguard/sync/user_anonymous"_i18n };
        ui::pick("playguard/sync/user"_i18n, options, 0, [this](int index) {
            if (index == 1) {
                SyncConf next = this->conf;
                next.username[0] = '\0';
                next.password[0] = '\0';
                next.allow_anonymous = true;
                this->commit(next);
                return;
            }
            ui::prompt_text("playguard/sync/user_prompt"_i18n, conf.username, SYNC_USER_MAX - 1, [this](std::string text) {
                SyncConf next = this->conf;
                set_text(next.username, text);
                this->commit(next);
            });
        });
        return true;
    });

    password->registerClickAction([this](brls::View*) {
        const std::vector<std::string> options = { "playguard/sync/password_enter"_i18n, "playguard/sync/password_none"_i18n };
        ui::pick("playguard/sync/password"_i18n, options, 0, [this](int index) {
            if (index == 1) {
                SyncConf next = this->conf;
                next.password[0] = '\0';
                this->commit(next);
                return;
            }
            ui::prompt_text("playguard/sync/password_prompt"_i18n, "", SYNC_PASS_MAX - 1, [this](std::string text) {
                SyncConf next = this->conf;
                set_text(next.password, text);
                this->commit(next);
            });
        });
        return true;
    });

    name->registerClickAction([this](brls::View*) {
        ui::prompt_text("playguard/sync/name_prompt"_i18n, conf.console_name, SYNC_NAME_MAX - 1, [this](std::string text) {
            SyncConf next = this->conf;
            set_text(next.console_name, text);
            this->commit(next);
        });
        return true;
    });

    policy->registerClickAction([this](brls::View*) {
        std::vector<std::string> labels;
        int selected = 0;
        for (size_t i = 0; i < 3; i++) {
            labels.push_back(policy_text(POLICIES[i]));
            if (POLICIES[i] == conf.policy) selected = (int)i;
        }
        ui::pick("playguard/sync/policy"_i18n, labels, selected, [this](int index) {
            SyncConf next = this->conf;
            next.policy = POLICIES[index];
            this->commit(next);
        });
        return true;
    });

    timer->init("playguard/sync/timer_writes"_i18n, conf.remote_timer_writes, [this](bool on) {
        if (!on) {
            SyncConf next = this->conf;
            next.remote_timer_writes = false;
            if (!this->commit(next)) timer->setOn(this->conf.remote_timer_writes, false);
            return;
        }
        // Back off until the parent confirms: the switch shows what is saved.
        timer->setOn(false, false);
        ui::confirm("playguard/sync/timer_writes_body"_i18n, "playguard/sync/timer_writes_confirm"_i18n, [this]() {
            SyncConf next = this->conf;
            next.remote_timer_writes = true;
            this->commit(next);
        }, nullptr, true);
    });

    discovery->init("playguard/sync/discovery"_i18n, conf.ha_discovery, [this](bool on) {
        SyncConf next = this->conf;
        next.ha_discovery = on;
        if (!this->commit(next)) discovery->setOn(this->conf.ha_discovery, false);
    });

    report->init("playguard/sync/report"_i18n, conf.publish_report, [this](bool on) {
        SyncConf next = this->conf;
        next.publish_report = on;
        if (!this->commit(next)) report->setOn(this->conf.publish_report, false);
    });

    status->registerClickAction([this](brls::View*) {
        const sync_flow::Status st = sync_flow::status();
        std::string text = this->status_line;
        const SyncStatus& l = st.link;
        if (st.agent) {
            text += "\n\n" + brls::getStr("playguard/sync/status_agent", st.agent_version);
            if (st.agent_read_only) text += "\n" + "playguard/sync/status_agent_read_only"_i18n;
        }
        if (st.running) {
            text += "\n\n" + brls::getStr("playguard/sync/status_counts", (int)l.publishes, (int)l.orders,
                                          (int)l.rejected, (int)st.pending);
            if (l.last_result[0]) text += "\n" + brls::getStr("playguard/sync/status_last", l.last_result);
            if (l.error[0]) text += "\n" + brls::getStr("playguard/sync/status_error", l.error);
        }
        ui::info(text);
        return true;
    });

    sync_now->registerClickAction([this](brls::View*) {
        if (!sync_flow::status().running) {
            ui::notify("playguard/sync/not_running"_i18n);
            return true;
        }
        sync_flow::sync_now();
        ui::notify("playguard/sync/sync_now_done"_i18n);
        return true;
    });

    log->registerClickAction([](brls::View*) {
        const auto lines = sync_flow::log_lines();
        std::string text;
        // The newest at the top; a dialog only holds so much.
        for (size_t i = lines.size(); i > 0 && lines.size() - i < 20; i--) text += lines[i - 1] + "\n";
        ui::info(text.empty() ? "playguard/sync/log_empty"_i18n : text);
        return true;
    });

    id->registerClickAction([this](brls::View*) {
        if (!sync_conf_id_valid(conf.console_id)) return true;
        ui::info(brls::getStr("playguard/sync/id_body", conf.console_id,
                              fmt::format("{}/{}", conf.topic_prefix, conf.console_id)));
        return true;
    });

    this->refresh();
    this->status_timer.setCallback([this]() { this->refresh_status(); });
    this->status_timer.start(1000);
}

bool SyncActivity::commit(SyncConf next)
{
    // The link can change the console: the same PIN as a console change.
    if (!pin_lock::allow_change()) {
        ui::notify(pin_lock::refusal_text());
        this->refresh();
        return false;
    }
    std::string error;
    if (!sync_files::save(next, &error)) {
        ui::error("playguard/sync/save_err"_i18n + " — " + error);
        this->refresh();
        return false;
    }
    this->conf = next;
    sync_files::export_nro_state();
    sync_flow::reload();
    this->refresh();
    return true;
}

void SyncActivity::refresh()
{
    intro->setText(brls::getStr("playguard/sync/intro", sync_files::conf_file()));
    enabled->setOn(conf.enabled, false);
    host->setDetailText(or_none(conf.host));
    port->setDetailText(std::to_string(conf.port));
    tls->setOn(conf.tls, false);
    user->setDetailText(conf.username[0] ? std::string(conf.username)
                                         : conf.allow_anonymous ? "playguard/sync/anonymous"_i18n
                                                                : "playguard/sync/none"_i18n);
    password->setDetailText(conf.password[0] ? "playguard/sync/password_set"_i18n : "playguard/sync/none"_i18n);
    name->setDetailText(conf.console_name[0] ? std::string(conf.console_name) : "playguard/sync/name_default"_i18n);
    policy->setDetailText(policy_text(conf.policy));
    timer->setOn(conf.remote_timer_writes, false);
    discovery->setOn(conf.ha_discovery, false);
    report->setOn(conf.publish_report, false);
    id->setDetailText(sync_conf_id_valid(conf.console_id) ? std::string(conf.console_id) : "playguard/sync/none"_i18n);
    note->setText("playguard/sync/note"_i18n);
    this->refresh_status();
}

void SyncActivity::refresh_status()
{
    const sync_flow::Status st = sync_flow::status();
    std::string text;
    NVGcolor color = ui::color_neutral();
    if (!conf.enabled) {
        text = "playguard/sync/state_off"_i18n;
    } else if (!st.ready) {
        text = brls::getStr("playguard/sync/state_incomplete", problem_text(conf));
        color = ui::color_warn();
    } else if (st.agent_refused) {
        text = brls::getStr("playguard/sync/state_agent_refused", st.agent_version);
        color = ui::color_warn();
    } else if (!st.running) {
        text = "playguard/sync/state_stopped"_i18n;
        color = ui::color_warn();
    } else {
        switch (st.link.state) {
            case SyncLink_Online: {
                const std::string since = since_text(st.link.online_since_ms);
                text = since.empty() ? "playguard/sync/state_online"_i18n
                                     : brls::getStr("playguard/sync/state_online_since", since);
                color = ui::color_ok();
                break;
            }
            case SyncLink_Connecting: text = "playguard/sync/state_connecting"_i18n; break;
            case SyncLink_Waiting:
                text = st.link.error[0] ? brls::getStr("playguard/sync/state_retry", st.link.error)
                                        : "playguard/sync/state_waiting"_i18n;
                color = ui::color_warn();
                break;
            default: text = "playguard/sync/state_off"_i18n; break;
        }
        if (st.agent) text = brls::getStr("playguard/sync/state_agent", text);
    }
    if (text == this->status_line) return;
    this->status_line = text;
    status->setDetailText(text);
    status->setDetailTextColor(color);
}
