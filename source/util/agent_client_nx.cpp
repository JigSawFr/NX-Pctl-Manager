// agent_client on the console: a session on pg:agent (agent_client.hpp).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#ifdef __SWITCH__
#include "util/agent_client.hpp"

#include <cstdio>
#include <cstring>
#include <switch.h>

#include "app.hpp"

namespace agent_client
{

namespace
{
Service s_srv;
bool    s_open = false;

// Atmosphère's sm answers whether a service is registered (command 65100,
// as libstratosphere's smAtmosphereHasService): asking sm for one that is
// not would wait until it is.
bool has_service(const char* name)
{
    const SmServiceName n = smEncodeName(name);
    u8 has = 0;
    const Result rc = hosversionAtLeast(12, 0, 0) ? tipcDispatchInOut(smGetServiceSessionTipc(), 65100, n, has)
                                                  : serviceDispatchInOut(smGetServiceSession(), 65100, n, has);
    return R_SUCCEEDED(rc) && has;
}

// A call that failed because the agent went away ends the session.
Result checked(Result rc)
{
    if (R_FAILED(rc) && R_MODULE(rc) == Module_Kernel) close();
    return rc;
}
}   // namespace

bool available()
{
    return s_open || has_service(AGENT_SERVICE_NAME);
}

bool open(AgentHelloReply* reply, Result* rc_out)
{
    std::memset(reply, 0, sizeof(*reply));
    if (!s_open) {
        if (!has_service(AGENT_SERVICE_NAME)) return false;
        const Result rc = smGetService(&s_srv, AGENT_SERVICE_NAME);
        if (rc_out) *rc_out = rc;
        if (R_FAILED(rc)) return false;
        s_open = true;
    }
    AgentHello hello;
    std::memset(&hello, 0, sizeof(hello));
    hello.protocol = AGENT_PROTOCOL;
    std::snprintf(hello.version, sizeof(hello.version), "%s", app::version().c_str());
    const Result rc = checked(serviceDispatchInOut(&s_srv, AgentCmd_Hello, hello, *reply));
    if (rc_out) *rc_out = rc;
    if (R_FAILED(rc)) {
        close();
        return false;
    }
    if (!reply->accepted) close();   // another protocol: nothing more to say
    return true;
}

void close()
{
    if (!s_open) return;
    serviceClose(&s_srv);
    s_open = false;
}

bool connected()
{
    return s_open;
}

Result set_foreground(bool on)
{
    if (!s_open) return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
    const u8 v = on ? 1 : 0;
    return checked(serviceDispatchIn(&s_srv, AgentCmd_SetForeground, v));
}

static Result send_buffer(u32 cmd, const void* data, size_t size)
{
    if (!s_open) return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
    return checked(serviceDispatch(&s_srv, cmd,
                                   .buffer_attrs = { SfBufferAttr_In | SfBufferAttr_HipcMapAlias },
                                   .buffers = { { data, size } }));
}

static Result receive_buffer(u32 cmd, void* data, size_t size)
{
    if (!s_open) return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
    return checked(serviceDispatch(&s_srv, cmd,
                                   .buffer_attrs = { SfBufferAttr_Out | SfBufferAttr_HipcMapAlias },
                                   .buffers = { { data, size } }));
}

Result push(AgentCmd cmd, const std::string& doc)
{
    return send_buffer(cmd, doc.data(), doc.size());
}

Result push_final(const std::string& date, const std::string& doc)
{
    const std::string data = date + "\n" + doc;
    return send_buffer(AgentCmd_PushFinal, data.data(), data.size());
}

Result pop_order(AgentOrder* out)
{
    std::memset(out, 0, sizeof(*out));
    return receive_buffer(AgentCmd_PopOrder, out, sizeof(*out));
}

Result order_result(const AgentResult& r)
{
    if (!s_open) return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
    return checked(serviceDispatchIn(&s_srv, AgentCmd_OrderResult, r));
}

static Result plain(u32 cmd)
{
    if (!s_open) return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
    return checked(serviceDispatch(&s_srv, cmd));
}

Result sync_now()
{
    return plain(AgentCmd_SyncNow);
}

Result reload()
{
    return plain(AgentCmd_ReloadConfig);
}

Result prepare_shutdown()
{
    return plain(AgentCmd_PrepareShutdown);
}

Result status(AgentStatus* out)
{
    std::memset(out, 0, sizeof(*out));
    return receive_buffer(AgentCmd_GetStatus, out, sizeof(*out));
}

Result records(AgentRecords* out)
{
    std::memset(out, 0, sizeof(*out));
    if (!s_open) return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
    return checked(serviceDispatchOut(&s_srv, AgentCmd_GetRecords, *out));
}

Result log(std::string* out)
{
    std::string text(AGENT_LOG_LINES * AGENT_LOG_LINE + 1, '\0');
    const Result rc = receive_buffer(AgentCmd_GetLog, &text[0], text.size());
    if (R_SUCCEEDED(rc)) out->assign(text.c_str());
    return rc;
}

}   // namespace agent_client
#endif   // __SWITCH__
