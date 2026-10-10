// The desktop build's agent_client: an agent simulated in the process, with
// the agent's own command dispatch (sysmodule/agent/source/agent_core.c).
// PLAYGUARD_SIM_AGENT=running makes it present; PLAYGUARD_SIM_AGENT_ORDER=
// "limit_uniform=90" hands PlayGuard that order once it said Hello. What
// PlayGuard pushes and answers is logged ("sim agent: …") for the smoke test.
// No broker: the simulated agent says it is online.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <borealis.hpp>
#include <cstdlib>
#include <cstring>

#include "util/agent_client.hpp"

extern "C" {
#include "agent_core.h"
#include "sync_net.h"
}

namespace agent_client
{

namespace
{
AgentShared* s_agent = nullptr;
bool         s_open = false;
bool         s_order_sent = false;

bool present()
{
    const char* v = std::getenv("PLAYGUARD_SIM_AGENT");
    return v && !std::strcmp(v, "running");
}

AgentShared& agent()
{
    if (!s_agent) {
        s_agent = new AgentShared;
        agent_shared_init(s_agent, "9.9.9-sim");
        std::snprintf(s_agent->console_id, sizeof(s_agent->console_id), "5a0c3e11");
        s_agent->status.link.state = SyncLink_Online;
        s_agent->status.link.online_since_ms = sync_now_ms();
        s_agent->status.configured = 1;
        agent_log(s_agent, "simulated agent started");
    }
    return *s_agent;
}

// What the agent's main loop does with what PlayGuard pushed: publishes it
// (here: logs it) and takes it.
void publish_pushed()
{
    AgentShared& a = agent();
    a.state_info.fresh = a.activity_info.fresh = a.names_info.fresh = a.week_info.fresh = false;
    for (int i = 0; i < AGENT_FINALS; i++)
        if (a.finals[i].fresh) {
            a.finals[i].fresh = false;
            brls::Logger::info("sim agent: publish activity/{} ({} bytes)", a.finals[i].date, a.finals[i].len);
        }
}

Result call(uint32_t cmd, AgentCall& c)
{
    if (!s_open && cmd != AgentCmd_Hello) return 1;
    const Result rc = agent_dispatch(&agent(), cmd, &c);
    publish_pushed();
    return rc;
}

// The order of PLAYGUARD_SIM_AGENT_ORDER, once.
void hand_order()
{
    const char* spec = std::getenv("PLAYGUARD_SIM_AGENT_ORDER");
    if (s_order_sent || !spec || !std::strchr(spec, '=')) return;
    s_order_sent = true;
    AgentOrder o;
    std::memset(&o, 0, sizeof(o));
    o.id = 1;
    const char* eq = std::strchr(spec, '=');
    std::snprintf(o.entity, sizeof(o.entity), "%.*s", (int)(eq - spec), spec);
    std::snprintf(o.payload, sizeof(o.payload), "%s", eq + 1);
    if (sync_apply_parse(o.entity, o.payload, std::strlen(o.payload), &o.intent) != SyncReason_None) return;
    agent_forward(&agent(), &o);
}
}   // namespace

bool available()
{
    return present();
}

bool open(AgentHelloReply* reply, Result* rc_out)
{
    std::memset(reply, 0, sizeof(*reply));
    if (!present()) return false;
    AgentHello hello;
    std::memset(&hello, 0, sizeof(hello));
    hello.protocol = AGENT_PROTOCOL;
    std::snprintf(hello.version, sizeof(hello.version), "sim");
    AgentCall c;
    std::memset(&c, 0, sizeof(c));
    c.in = &hello;
    c.in_len = sizeof(hello);
    c.out = reply;
    c.out_cap = sizeof(*reply);
    const Result rc = call(AgentCmd_Hello, c);
    if (rc_out) *rc_out = rc;
    s_open = rc == 0 && reply->accepted;
    brls::Logger::info("sim agent: Hello -> {}", s_open ? "accepted" : "refused");
    if (s_open) hand_order();
    return rc == 0;
}

void close()
{
    if (!s_open) return;
    s_open = false;
    agent_session_closed(&agent());
    brls::Logger::info("sim agent: session closed");
}

bool connected()
{
    return s_open;
}

Result set_foreground(bool on)
{
    const uint8_t v = on ? 1 : 0;
    AgentCall c;
    std::memset(&c, 0, sizeof(c));
    c.in = &v;
    c.in_len = 1;
    return call(AgentCmd_SetForeground, c);
}

static Result send_buffer(uint32_t cmd, const std::string& data)
{
    AgentCall c;
    std::memset(&c, 0, sizeof(c));
    c.buf = data.data();
    c.buf_len = data.size();
    const Result rc = call(cmd, c);
    brls::Logger::info("sim agent: push {} ({} bytes) -> {}", cmd, data.size(), rc);
    return rc;
}

Result push(AgentCmd cmd, const std::string& doc)
{
    return send_buffer(cmd, doc);
}

Result push_final(const std::string& date, const std::string& doc)
{
    return send_buffer(AgentCmd_PushFinal, date + "\n" + doc);
}

Result pop_order(AgentOrder* out)
{
    AgentCall c;
    std::memset(&c, 0, sizeof(c));
    c.buf_out = out;
    c.buf_out_cap = sizeof(*out);
    return call(AgentCmd_PopOrder, c);
}

Result order_result(const AgentResult& r)
{
    AgentCall c;
    std::memset(&c, 0, sizeof(c));
    c.in = &r;
    c.in_len = sizeof(r);
    const Result rc = call(AgentCmd_OrderResult, c);
    brls::Logger::info("sim agent: order {} -> {}", r.id, r.applied ? "applied" : sync_reason_name((SyncReason)r.reason));
    return rc;
}

static Result plain(uint32_t cmd)
{
    AgentCall c;
    std::memset(&c, 0, sizeof(c));
    return call(cmd, c);
}

Result sync_now()
{
    return plain(AgentCmd_SyncNow);
}

Result reload()
{
    brls::Logger::info("sim agent: reload");
    return plain(AgentCmd_ReloadConfig);
}

Result prepare_shutdown()
{
    return plain(AgentCmd_PrepareShutdown);
}

Result status(AgentStatus* out)
{
    AgentCall c;
    std::memset(&c, 0, sizeof(c));
    c.buf_out = out;
    c.buf_out_cap = sizeof(*out);
    return call(AgentCmd_GetStatus, c);
}

Result records(AgentRecords* out)
{
    AgentCall c;
    std::memset(&c, 0, sizeof(c));
    c.out = out;
    c.out_cap = sizeof(*out);
    return call(AgentCmd_GetRecords, c);
}

Result log(std::string* out)
{
    std::string text(AGENT_LOG_LINES * AGENT_LOG_LINE + 1, '\0');
    AgentCall c;
    std::memset(&c, 0, sizeof(c));
    c.buf_out = &text[0];
    c.buf_out_cap = text.size();
    const Result rc = call(AgentCmd_GetLog, c);
    if (rc == 0) out->assign(text.c_str());
    return rc;
}

}   // namespace agent_client
