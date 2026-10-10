// Host tests for sysmodule/agent/source/agent_core.c: the pg:agent commands
// on the shared state (Hello and the protocol check, nothing before a
// session, foreground, the pushed documents and their size limits, the
// finished days, orders handed out once and answered once, Sync now,
// reload, shutdown, status, records, the log ring), the session closing with
// orders still out, and what nro_state.txt allows the agent to do.
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "agent_core.h"

static AgentShared S;

static uint32_t call(uint32_t cmd, const void *in, size_t in_len, const void *buf, size_t buf_len, void *out,
                     size_t out_cap, size_t *out_len)
{
    AgentCall c;
    memset(&c, 0, sizeof(c));
    c.in = in;
    c.in_len = in_len;
    c.buf = buf;
    c.buf_len = buf_len;
    c.out = out;
    c.out_cap = out_cap;
    const uint32_t rc = agent_dispatch(&S, cmd, &c);
    if (out_len) *out_len = c.out_len;
    return rc;
}

// A command whose reply is a mapped output buffer (the order, the status).
static uint32_t call_buf(uint32_t cmd, void *buf_out, size_t cap, size_t *len)
{
    AgentCall c;
    memset(&c, 0, sizeof(c));
    c.buf_out = buf_out;
    c.buf_out_cap = cap;
    const uint32_t rc = agent_dispatch(&S, cmd, &c);
    if (len) *len = c.buf_out_len;
    return rc;
}

static uint32_t hello(uint32_t protocol, AgentHelloReply *r)
{
    AgentHello h;
    memset(&h, 0, sizeof(h));
    h.protocol = protocol;
    snprintf(h.version, sizeof(h.version), "1.2.0");
    size_t n = 0;
    return call(AgentCmd_Hello, &h, sizeof(h), NULL, 0, r, sizeof(*r), &n);
}

static void test_session(void)
{
    agent_shared_init(&S, "1.2.0-agent");
    snprintf(S.console_id, sizeof(S.console_id), "a1b2c3d4");
    S.status.link.state = SyncLink_Online;

    // Nothing but Hello, the status and the log without a session.
    const char doc[] = "{\"schema\":1}";
    assert(call(AgentCmd_PushState, NULL, 0, doc, sizeof(doc), NULL, 0, NULL) == AGENT_RC_NO_SESSION);
    AgentStatus st;
    size_t n = 0;
    assert(call_buf(AgentCmd_GetStatus, &st, sizeof(st), &n) == 0 && n == sizeof(st));
    assert(!st.app_session && !strcmp(st.version, "1.2.0-agent"));
    assert(call_buf(AgentCmd_GetStatus, &st, 8, &n) == AGENT_RC_BAD_INPUT);

    // Another protocol: the reply says which, no session.
    AgentHelloReply r;
    assert(hello(AGENT_PROTOCOL + 1, &r) == 0);
    assert(!r.accepted && r.protocol == AGENT_PROTOCOL && !strcmp(r.version, "1.2.0-agent") && !S.app_session);

    assert(hello(AGENT_PROTOCOL, &r) == 0 && r.accepted);
    assert(!strcmp(r.console_id, "a1b2c3d4") && r.online && S.app_session && S.app_foreground && S.session_changed);
    assert(!strcmp(S.app_version, "1.2.0"));
    S.session_changed = false;
    assert(hello(AGENT_PROTOCOL, &r) == 0 && !S.session_changed);   // already open

    const uint8_t bg = 0;
    assert(call(AgentCmd_SetForeground, &bg, 1, NULL, 0, NULL, 0, NULL) == 0);
    assert(!S.app_foreground && S.foreground_changed);
    assert(call(AgentCmd_SetForeground, NULL, 0, NULL, 0, NULL, 0, NULL) == AGENT_RC_BAD_INPUT);
    assert(call(99, NULL, 0, NULL, 0, NULL, 0, NULL) == AGENT_RC_UNKNOWN_CMD);
}

static void test_documents(void)
{
    // A pushed document, its NUL dropped; taken once.
    const char doc[] = "{\"schema\":1,\"source\":\"app\"}";
    assert(call(AgentCmd_PushState, NULL, 0, doc, sizeof(doc), NULL, 0, NULL) == 0);
    char out[AGENT_DOC_STATE];
    assert(agent_take_doc(S.state, &S.state_info, out, sizeof(out)) == strlen(doc) && !strcmp(out, doc));
    assert(agent_take_doc(S.state, &S.state_info, out, sizeof(out)) == 0);
    assert(call(AgentCmd_PushState, NULL, 0, NULL, 0, NULL, 0, NULL) == AGENT_RC_BAD_INPUT);

    static char big[AGENT_DOC_LARGE + 10];
    memset(big, 'x', sizeof(big));
    assert(call(AgentCmd_PushActivity, NULL, 0, big, AGENT_DOC_STATE, NULL, 0, NULL) == AGENT_RC_TOO_LARGE);
    assert(call(AgentCmd_PushNames, NULL, 0, big, AGENT_DOC_LARGE - 1, NULL, 0, NULL) == 0);
    assert(S.names_info.len == AGENT_DOC_LARGE - 1);
    assert(call(AgentCmd_PushWeek, NULL, 0, big, sizeof(big), NULL, 0, NULL) == AGENT_RC_TOO_LARGE);

    // Finished days: the same day replaces itself, seven at most waiting.
    const char d1[] = "2026-10-08\n{\"final\":true}";
    assert(call(AgentCmd_PushFinal, NULL, 0, d1, strlen(d1), NULL, 0, NULL) == 0);
    assert(call(AgentCmd_PushFinal, NULL, 0, d1, strlen(d1), NULL, 0, NULL) == 0);
    int fresh = 0;
    for (int i = 0; i < AGENT_FINALS; i++) fresh += S.finals[i].fresh;
    assert(fresh == 1 && !strcmp(S.finals[0].date, "2026-10-08") && !strcmp(S.finals[0].doc, "{\"final\":true}"));
    char day[64];
    for (int k = 1; k < AGENT_FINALS; k++) {
        snprintf(day, sizeof(day), "2026-10-0%d\n{}", k);
        assert(call(AgentCmd_PushFinal, NULL, 0, day, strlen(day), NULL, 0, NULL) == 0);
    }
    assert(call(AgentCmd_PushFinal, NULL, 0, "2026-09-30\n{}", 13, NULL, 0, NULL) == AGENT_RC_FULL);
    assert(call(AgentCmd_PushFinal, NULL, 0, "2026/10/08\n{}", 13, NULL, 0, NULL) == AGENT_RC_BAD_INPUT);
    assert(call(AgentCmd_PushFinal, NULL, 0, "2026-10-08", 10, NULL, 0, NULL) == AGENT_RC_BAD_INPUT);
    for (int i = 0; i < AGENT_FINALS; i++) S.finals[i].fresh = false;
}

static void test_orders(void)
{
    AgentOrder o;
    memset(&o, 0, sizeof(o));
    o.id = 7;
    snprintf(o.entity, sizeof(o.entity), "limit_mon");
    snprintf(o.payload, sizeof(o.payload), "90");
    o.intent.kind = SyncIntent_LimitsWeek;
    assert(agent_forward(&S, &o));
    o.id = 8;
    snprintf(o.entity, sizeof(o.entity), "lock_now");
    assert(agent_forward(&S, &o));

    // Each order is handed out once, oldest slot first.
    AgentOrder got;
    size_t n = 0;
    assert(call_buf(AgentCmd_PopOrder, &got, sizeof(got), &n) == 0 && n == sizeof(got));
    assert(got.id == 7 && !strcmp(got.entity, "limit_mon") && got.intent.kind == SyncIntent_LimitsWeek);
    assert(call_buf(AgentCmd_PopOrder, &got, sizeof(got), &n) == 0 && got.id == 8);
    assert(call_buf(AgentCmd_PopOrder, &got, sizeof(got), &n) == 0 && got.id == 0);

    // Answered once; an unknown id is refused.
    AgentResult r = { 7, 0, 1, 1, 0, 0 };
    assert(call(AgentCmd_OrderResult, &r, sizeof(r), NULL, 0, NULL, 0, NULL) == 0);
    assert(call(AgentCmd_OrderResult, &r, sizeof(r), NULL, 0, NULL, 0, NULL) == AGENT_RC_BAD_INPUT);
    assert(call(AgentCmd_OrderResult, &r, 3, NULL, 0, NULL, 0, NULL) == AGENT_RC_BAD_INPUT);
    AgentResult taken[4];
    assert(agent_take_results(&S, taken, 4) == 1 && taken[0].id == 7 && taken[0].applied);
    assert(agent_take_results(&S, taken, 4) == 0);

    // PlayGuard goes away with order 8 still out: it is kept to be answered
    // "waiting", and the agent is on its own again.
    uint32_t ids[AGENT_ORDERS_MAX];
    assert(agent_session_closed(&S) == 1);
    assert(!S.app_session && S.session_changed);
    assert(agent_take_closed(&S, ids, AGENT_ORDERS_MAX) == 1 && ids[0] == 8);
    assert(agent_take_closed(&S, ids, AGENT_ORDERS_MAX) == 0);
    assert(call_buf(AgentCmd_PopOrder, &got, sizeof(got), &n) == AGENT_RC_NO_SESSION);

    // A full hand-over queue refuses more.
    for (uint32_t i = 1; i <= AGENT_ORDERS_MAX; i++) {
        o.id = i;
        assert(agent_forward(&S, &o));
    }
    o.id = 99;
    assert(!agent_forward(&S, &o));
    assert(agent_session_closed(&S) == AGENT_ORDERS_MAX);
    assert(agent_take_closed(&S, ids, 4) == 4 && ids[0] == 1);   // as many as asked, oldest first
    assert(agent_take_closed(&S, ids, AGENT_ORDERS_MAX) == AGENT_ORDERS_MAX - 4 && ids[0] == 5);
}

static void test_requests_status_log(void)
{
    AgentHelloReply r;
    assert(hello(AGENT_PROTOCOL, &r) == 0);
    assert(call(AgentCmd_SyncNow, NULL, 0, NULL, 0, NULL, 0, NULL) == 0 && S.want_sync_now);
    assert(call(AgentCmd_ReloadConfig, NULL, 0, NULL, 0, NULL, 0, NULL) == 0 && S.want_reload);
    assert(call(AgentCmd_PrepareShutdown, NULL, 0, NULL, 0, NULL, 0, NULL) == 0 && S.want_shutdown);

    S.records.valid = 1;
    S.records.records.console_lock = true;
    AgentRecords rec;
    size_t n = 0;
    assert(call(AgentCmd_GetRecords, NULL, 0, NULL, 0, &rec, sizeof(rec), &n) == 0 && n == sizeof(rec));
    assert(rec.valid && rec.records.console_lock);
    assert(call(AgentCmd_GetRecords, NULL, 0, NULL, 0, &rec, 4, &n) == AGENT_RC_BAD_INPUT);

    AgentStatus st;
    S.status.pending = 3;
    assert(call_buf(AgentCmd_GetStatus, &st, sizeof(st), &n) == 0);
    assert(st.app_session && st.pending == 3);

    // The log keeps the newest lines, oldest first.
    char line[32];
    for (int i = 0; i < AGENT_LOG_LINES + 5; i++) {
        snprintf(line, sizeof(line), "line %d", i);
        agent_log(&S, line);
    }
    static char text[AGENT_LOG_LINES * AGENT_LOG_LINE];
    AgentCall c;
    memset(&c, 0, sizeof(c));
    c.buf_out = text;
    c.buf_out_cap = sizeof(text);
    assert(agent_dispatch(&S, AgentCmd_GetLog, &c) == 0 && c.buf_out_len == strlen(text));
    assert(!strncmp(text, "line 5\n", 7) && strstr(text, "line 204\n") && !strstr(text, "line 4\n"));
    // A small buffer gets whole lines only.
    c.buf_out_cap = 20;
    assert(agent_dispatch(&S, AgentCmd_GetLog, &c) == 0 && !strcmp(text, "line 5\nline 6\n"));
}

static void test_nro_state(void)
{
    AgentNroState n;
    agent_nro_parse(&n, NULL, 0);
    assert(!n.known && !agent_may_write(&n, "23.0.1"));

    const char *ok = "extra_weekday=4\nextra_date=2026-10-08\nextra_base=90\nextra_value=120\n"
                     "console_lock=0\nrelock_pending=0\nread_only=0\nfirmware=23.0.1\n";
    agent_nro_parse(&n, ok, strlen(ok));
    assert(n.known && !n.read_only && !strcmp(n.firmware, "23.0.1"));
    assert(n.records.extra_weekday == 4 && n.records.extra_value == 120);
    assert(agent_may_write(&n, "23.0.1"));
    assert(!agent_may_write(&n, "24.0.0"));   // a system update since PlayGuard last ran
    assert(!agent_may_write(&n, NULL));

    const char *ro = "read_only=1\nfirmware=23.0.1\n";
    agent_nro_parse(&n, ro, strlen(ro));
    assert(n.read_only && !agent_may_write(&n, "23.0.1"));

    // An older PlayGuard's file (no read_only key): never a write.
    const char *old = "extra_weekday=-1\nfirmware=23.0.1\n";
    agent_nro_parse(&n, old, strlen(old));
    assert(n.known && n.read_only && !agent_may_write(&n, "23.0.1"));
}

int main(void)
{
    test_session();
    test_documents();
    test_orders();
    test_requests_status_log();
    test_nro_state();
    puts("agent: session, documents, orders, requests, status, log and nro_state assertions passed");
    return 0;
}
