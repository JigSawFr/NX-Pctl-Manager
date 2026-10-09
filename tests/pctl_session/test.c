// Host tests for source/core/pctl_ops.c: session ownership, firmware gating,
// the play-timer write gate, the unlock verification and read-only mode.
// Based on the lifecycle tests from anbingxi/NX-Pctl-Manager (diag/fw22-5-readonly).
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "pctl_ops.h"
#include "write_guard.h"

enum { MOCK_ERROR = 0x701 };
static Service service;
static struct {
    u32 hos;
    unsigned refs, init_calls, exit_calls, ipc_calls, failed_ipcs, writes, applets;
    unsigned fail_init_call;
    u32 fail_command, fail_command2;   /* commands that return MOCK_ERROR */
    bool enabled, restricted, unlocked, unlock_effective;
    u32 safety_level;
    u32 last_write_cmd;
    u8 last_write[0x44];
    size_t last_write_size;
    bool saw_1459, saw_1460, saw_1952;
    const char *pin;          /* what GetPinCode (1208) returns */
    char got_pin[32];         /* what UnlockRestrictionTemporarily (1201) received */
    size_t got_pin_size;
    u16 pt_block[34];         /* what GetPlayTimerSettings (145601) returns */
    bool no_pin;              /* GetPinCodeLength (1206) returns 0 */
    unsigned auth_applets;    /* pctlauthShowForConfiguration calls */
    Result auth_result;       /* what it returns */
    unsigned checks;          /* calls of the change check below */
    bool check_answer;
    u32 rating_org;           /* what SetDefaultRatingOrganization (1038) received */
    u32 exempt;               /* what GetExemptApplicationListCountForDebug (1903) returns */
    bool saw_1904;
    bool bed_live;            /* 1954/1956/1957 answer bed_day of pt_block, which 195101 replaces */
    bool bed_ignored;         /* ... except that 195101 leaves the bedtime they report as it was */
    int bed_day;
} model;

static void reset_with(u32 hos)
{
    assert(model.refs == 0); /* a test must release before the next one */
    memset(&model, 0, sizeof(model));
    model.hos = hos;
    model.unlock_effective = true;
    model.safety_level = PctlSafetyLevel_Custom;
    model.pin = "123456";
    /* The layout observed on hardware: a limit of 30 * n minutes on day n. */
    model.pt_block[0] = 0x0101; model.pt_block[1] = 1;
    for (int n = 0; n < 7; n++) {
        model.pt_block[7 + 4 * n] = 0x0600;
        model.pt_block[8 + 4 * n] = 0x0100;
        model.pt_block[9 + 4 * n] = (u16)(30 * n);
    }
}
static void reset(void) { reset_with(MAKEHOSVERSION(23, 0, 1)); }

bool hosversionAtLeast(u8 major, u8 minor, u8 micro)
{
    return model.hos >= MAKEHOSVERSION(major, minor, micro);
}

Result pctlInitialize(void)
{
    model.init_calls++;
    if (model.init_calls == model.fail_init_call) return MOCK_ERROR;
    assert(model.refs == 0); /* duplicate acquisition is a lifecycle defect */
    model.refs++;
    return 0;
}

void pctlExit(void)
{
    assert(model.refs == 1); /* exit without ownership / double release */
    model.refs--;
    model.exit_calls++;
}

Service *pctlGetServiceSession_Service(void)
{
    assert(model.refs == 1);
    return &service;
}

Result pctlauthRegisterPasscode(void)
{
    assert(model.refs == 0); /* the OS applet needs the privileged slot free */
    model.applets++;
    return MOCK_ERROR;
}

Result pctlauthShowForConfiguration(void)
{
    assert(model.refs == 0); /* the OS applet needs the privileged slot free */
    model.auth_applets++;
    return model.auth_result;
}

static Result put(void *out, size_t out_size, const void *value, size_t size)
{
    assert(out_size == size);
    memcpy(out, value, size);
    return 0;
}

Result mock_dispatch(Service *srv, u32 command, void *out, size_t out_size,
                     const void *in, size_t in_size, SfDispatchParams params)
{
    assert(srv == &service);
    assert(model.refs == 1);
    assert(command != 1456 && command != 1951); /* close the session on 21.0.0+ */
    model.ipc_calls++;

    /* Inputs that only select what to read: the level (1034), the offset (1044). */
    const bool is_write = (in != NULL && command != 1460 && command != 1034 && command != 1044 &&
                           command != 1904) ||
        command == 1007 || command == 1043 || command == 1941 || command == 1201 ||
        command == 1451 || command == 1452;
    if (is_write) {
        model.writes++;
        model.last_write_cmd = command;
    }
    if (command == model.fail_command || command == model.fail_command2) {
        model.failed_ipcs++;
        return MOCK_ERROR;
    }
    if (is_write && in != NULL) {
        assert(in_size <= sizeof(model.last_write));
        memcpy(model.last_write, in, in_size);
        model.last_write_size = in_size;
        switch (command) {
            case 195101:
                assert(in_size == 0x44);
                if (model.bed_live) memcpy(model.pt_block, in, in_size);
                break;
            case 1033:   assert(in_size == 4); model.safety_level = *(const u32 *)in; break;
            case 1036:   assert(in_size == 3); break;
            case 1063: case 1953: assert(in_size == 1); break;
            case 1038:   assert(in_size == 4); model.rating_org = *(const u32 *)in; break;
            default: assert(!"unexpected input command");
        }
        return 0;
    }
    if (command == 1201) {
        /* The PIN arrives NUL-terminated in an In|HipcPointer buffer. */
        assert(params.buffer_attrs[0] == (SfBufferAttr_HipcPointer | SfBufferAttr_In));
        assert(params.buffers[0].ptr != NULL && params.buffers[0].size <= sizeof(model.got_pin));
        memcpy(model.got_pin, params.buffers[0].ptr, params.buffers[0].size);
        model.got_pin_size = params.buffers[0].size;
        model.unlocked = model.unlock_effective;
        return 0;
    }
    if (command == 1007) { model.unlocked = false; return 0; }
    if (out == NULL) return 0;

    memset(out, 0, out_size);
    u8 b = 0; u32 w = 0; u64 q = 0;
    switch (command) {
        case 1453: b = model.enabled;    return put(out, out_size, &b, 1);
        case 1455: b = model.restricted; return put(out, out_size, &b, 1);
        case 1006: b = model.unlocked;   return put(out, out_size, &b, 1);
        case 1954: case 1956: case 1957:
            if (model.bed_live) {
                PtBedtime bt[7];
                pt_bedtime_decode(model.pt_block, bt);
                if (model.bed_ignored) memset(bt, 0, sizeof(bt));
                const PtBedtime *d = &bt[model.bed_day];
                b = command == 1954 ? d->on : command == 1956 ? d->hour : d->minute;
            } else {
                b = command == 1954 ? 1 : command == 1956 ? 21 : 30;
            }
            return put(out, out_size, &b, 1);
        case 1031: case 1403: case 1458: case 1062:
            b = 1; return put(out, out_size, &b, 1);
        case 1958: b = 7;  return put(out, out_size, &b, 1);
        case 1959: b = 0;  return put(out, out_size, &b, 1);
        case 1032: w = model.safety_level; return put(out, out_size, &w, 4);
        case 1206: w = model.no_pin ? 0 : 6; return put(out, out_size, &w, 4);
        case 1208:
            assert(params.buffer_attrs[0] == (SfBufferAttr_HipcPointer | SfBufferAttr_Out));
            assert(params.buffers[0].ptr != NULL && params.buffers[0].size > strlen(model.pin));
            memcpy((void *)params.buffers[0].ptr, model.pin, strlen(model.pin) + 1);
            w = (u32)strlen(model.pin);
            return put(out, out_size, &w, 4);
        case 1037: w = 6; return put(out, out_size, &w, 4);
        case 1039: w = 2; return put(out, out_size, &w, 4);
        case 1406: q = 1791381792ULL; return put(out, out_size, &q, 8);
        case 1454: q = 3600000000000ULL; return put(out, out_size, &q, 8);
        case 1952: model.saw_1952 = true; q = 60; return put(out, out_size, &q, 8);
        case 1960: q = 0; return put(out, out_size, &q, 8);
        case 1035: { u8 raw[3] = {12, 1, 0}; return put(out, out_size, raw, 3); }
        case 1034: {   /* the presets: 7 / 13 / 16 years, posting restricted below Teen */
            assert(in_size == 4);
            const u32 level = *(const u32 *)in;
            u8 raw[3] = { (u8)(level == 2 ? 7 : level == 3 ? 13 : level == 4 ? 16 : 0), (u8)(level >= 2 && level < 4), (u8)(level >= 2) };
            return put(out, out_size, raw, 3);
        }
        case 1044:
            assert(in_size == 4 && params.buffer_attrs[0] == (SfBufferAttr_HipcMapAlias | SfBufferAttr_Out));
            assert(params.buffers[0].ptr != NULL && params.buffers[0].size >= 0x20);
            memset((void *)params.buffers[0].ptr, 0xAB, 0x20);
            w = 1; return put(out, out_size, &w, 4);
        case 1903: w = model.exempt; return put(out, out_size, &w, 4);
        case 1904:
            model.saw_1904 = true;
            assert(in_size == 4 && params.buffer_attrs[0] == (SfBufferAttr_HipcMapAlias | SfBufferAttr_Out));
            assert(params.buffers[0].ptr != NULL && params.buffers[0].size >= 8 * model.exempt);
            memset((void *)params.buffers[0].ptr, 0xCD, 8 * model.exempt);
            w = model.exempt; return put(out, out_size, &w, 4);
        case 1459: model.saw_1459 = true; assert(out_size == 0x20); return 0;
        case 1460: model.saw_1460 = true; assert(out_size == 0x18 && in_size == 1); return 0;
        case 145601: return put(out, out_size, model.pt_block, sizeof(model.pt_block));
        default: assert(!"unexpected output command");
    }
    return 0;
}

static void assert_released(void)
{
    assert(model.refs == 0);
    assert(model.init_calls == model.exit_calls + (model.fail_init_call && model.fail_init_call <= model.init_calls ? 1u : 0u));
}

static void test_ownership(void)
{
    reset();
    model.fail_init_call = 1;
    assert(pctl_ops_init() == MOCK_ERROR);
    pctl_ops_exit();
    assert(model.exit_calls == 0 && model.refs == 0);

    reset();
    assert(pctl_ops_init() == 0);
    assert(pctl_ops_init() == 0);
    assert(model.init_calls == 1 && model.refs == 1);
    assert(pctl_ops_reinit() == 0);
    assert(model.init_calls == 2 && model.exit_calls == 1 && model.refs == 1);
    pctl_ops_exit();
    pctl_ops_exit();
    assert(model.exit_calls == 2 && model.refs == 0);
}

/* The Overview's read: one session, 1006 once, nothing it does not show. */
static void test_overview(void)
{
    PctlStatus s;
    PtState st;

    reset();
    model.unlocked = true;
    pctl_overview_fetch(&s, &st);
    assert(model.init_calls == 1 && model.exit_calls == 1 && model.refs == 0 && model.writes == 0);
    assert(s.session_rc == 0 && s.safety_level_ok && s.pin_length_ok && s.restriction_enabled_ok);
    assert(s.temp_unlocked_ok && s.temp_unlocked && s.pairing_active_ok);
    assert(!s.rating_org_ok && !s.free_comm_count_ok && !s.last_updated_ok && !s.settings_ok && !s.stereo_vision_ok);
    assert(st.fw_supported && st.session_valid && st.valid && st.enabled_valid && st.restricted_valid);
    assert(st.temporary_unlocked_valid && st.temporary_unlocked && st.remaining_valid);
    assert(st.day_min[0] == 0 && st.day_min[6] == 180 && st.bedtime_valid && st.bedtime_hour == 21);
    assert(st.alarm_disabled_valid && st.alarm_disabled && !st.bedtime_reset_valid);
    /* status 5 + play timer 3 + 145601 + bedtime 3 + alarm, against 10 + 1 + 13 in two sessions */
    assert(model.ipc_calls == 13);

    /* The same values as the two full reads. */
    PctlStatus full_s;
    PtState full_pt;
    pctl_status_fetch(&full_s);
    pctl_play_timer_query(&full_pt);
    assert(full_s.safety_level == s.safety_level && full_s.pin_length == s.pin_length);
    assert(full_pt.remaining_ns == st.remaining_ns && memcmp(full_pt.day_min, st.day_min, sizeof(st.day_min)) == 0);
    assert(full_pt.temporary_unlocked == st.temporary_unlocked && model.refs == 0);
    assert(full_pt.alarm_disabled == st.alarm_disabled);

    /* Below 21.0.0: the status only, no play-timer IPC. */
    reset_with(MAKEHOSVERSION(20, 5, 0));
    pctl_overview_fetch(&s, &st);
    assert(s.safety_level_ok && !st.fw_supported && !st.session_valid && model.ipc_calls == 5 && model.refs == 0);

    /* No session: nothing read, nothing left open. */
    reset();
    model.fail_init_call = 1;
    pctl_overview_fetch(&s, &st);
    assert(s.session_rc == MOCK_ERROR && st.session_rc == MOCK_ERROR && !st.session_valid);
    assert(model.ipc_calls == 0 && model.refs == 0);

    /* 1006 failing: neither copy claims to know. */
    reset();
    model.fail_command = 1006;
    pctl_overview_fetch(&s, &st);
    assert(!s.temp_unlocked_ok && !st.temporary_unlocked_valid && st.valid && model.refs == 0);

    /* 1458 failing: the alarm is not claimed off, the rest is read. */
    reset();
    model.fail_command = 1458;
    pctl_overview_fetch(&s, &st);
    assert(!st.alarm_disabled_valid && st.valid && st.enabled_valid && model.refs == 0);
}

/* The recorder's reading: two sessions, both released, nothing written. */
static void test_sample(void)
{
    PtSample smp;

    reset();
    model.unlocked = true;
    model.enabled = true;
    pctl_play_timer_sample(&smp);
    assert(smp.session_rc == 0 && model.init_calls == 2 && model.exit_calls == 2 && model.refs == 0);
    assert(model.writes == 0 && model.saw_1459 && model.saw_1952 && !model.saw_1460);
    assert(smp.unlocked_rc == 0 && smp.unlocked && smp.enabled_rc == 0 && smp.enabled);
    assert(smp.restricted_rc == 0 && !smp.restricted && smp.alarm_off_rc == 0 && smp.alarm_off);
    assert(smp.remaining_rc == 0 && smp.remaining_ns == 3600000000000ULL);
    assert(smp.spent_rc == 0 && smp.spent_ns == 60 && smp.extra_rc == 0 && smp.display_rc == 0);
    assert(smp.block_rc == 0 && memcmp(smp.block, model.pt_block, sizeof(smp.block)) == 0);
    assert(smp.bedtime_rc == 0 && smp.bedtime_on && smp.bedtime_hour == 21 && smp.bedtime_minute == 30);
    /* 1006..1454 5 + 145601 + bedtime 3, then 1459 1952 1960 */
    assert(model.ipc_calls == 12);

    /* One command failing leaves the others read. */
    reset();
    model.fail_command = 1454;
    model.fail_command2 = 1956;
    pctl_play_timer_sample(&smp);
    assert(smp.remaining_rc == MOCK_ERROR && smp.bedtime_rc == MOCK_ERROR);
    assert(smp.block_rc == 0 && smp.spent_rc == 0 && smp.enabled_rc == 0 && model.refs == 0);

    /* No first session: nothing read. No second one: its three reads say why. */
    reset();
    model.fail_init_call = 1;
    pctl_play_timer_sample(&smp);
    assert(smp.session_rc == MOCK_ERROR && model.ipc_calls == 0 && model.refs == 0);
    assert(smp.enabled_rc == MOCK_ERROR && smp.block_rc == MOCK_ERROR && smp.spent_rc == MOCK_ERROR);
    reset();
    model.fail_init_call = 2;
    pctl_play_timer_sample(&smp);
    assert(smp.session_rc == MOCK_ERROR && smp.block_rc == 0 && !model.saw_1952);
    assert(smp.display_rc == MOCK_ERROR && smp.spent_rc == MOCK_ERROR && smp.extra_rc == MOCK_ERROR);
    assert(model.refs == 0);

    /* Below 21.0.0: no play-timer IPC, as the report. */
    reset_with(MAKEHOSVERSION(20, 5, 0));
    pctl_play_timer_sample(&smp);
    assert(smp.session_rc == NXM_RC_FW_UNSUPPORTED && model.ipc_calls == 0 && model.init_calls == 0);
    assert(smp.enabled_rc == NXM_RC_FW_UNSUPPORTED && smp.display_rc == NXM_RC_FW_UNSUPPORTED);
}

static void test_reads(void)
{
    PtState st;
    PctlStatus s;
    char report[16384];

    reset();
    for (unsigned i = 0; i < 10; ++i) {
        pctl_play_timer_query(&st);
        assert(st.fw_supported && st.session_valid && st.valid);
        assert(st.enabled_valid && st.restricted_valid && st.temporary_unlocked_valid && st.remaining_valid);
        assert(st.day_min[0] == 0 && st.day_min[6] == 180);
        assert(st.bedtime_valid && st.bedtime_hour == 21 && st.bedtime_minute == 30);
        assert(st.bedtime_reset_valid && st.bedtime_reset_hour == 7);
        assert(model.refs == 0);
        pctl_status_fetch(&s);
        assert(s.safety_level_ok && s.pin_length_ok && s.restriction_enabled_ok && s.temp_unlocked_ok);
        assert(s.settings_ok && s.settings.rating_age == 12 && s.settings.sns_post_restriction);
        assert(s.rating_org_ok && s.rating_org == 6 && s.stereo_vision_ok && s.pairing_active_ok);
        assert(s.last_updated_ok && s.free_comm_count_ok && s.free_comm_count == 2);
        assert(model.refs == 0);
        pctl_dump(report, sizeof(report));
        assert(strstr(report, "content=not recorded") != NULL);
        assert(strstr(report, model.pin) == NULL);
        assert(strstr(report, "Tool-owned pctl session released.") != NULL);
        assert(model.refs == 0 && model.writes == 0);
    }
    assert(model.saw_1459 && model.saw_1460 && model.saw_1952);
    assert(strcmp(pctl_rating_org_name(6), "PEGI") == 0);

    /* The exemption list is listed only when 1903 counts an entry. */
    assert(!model.saw_1904 && strstr(report, "GetExemptApplicationListCountForDebug") != NULL);
    reset();
    model.exempt = 2;
    pctl_dump(report, sizeof(report));
    assert(model.saw_1904 && strstr(report, "count=2\nCD CD CD CD") != NULL && model.refs == 0 && model.writes == 0);

    /* 1460 exists only on 23.0.0+, 1459 only on 20.0.0+. */
    reset_with(MAKEHOSVERSION(22, 1, 0));
    pctl_dump(report, sizeof(report));
    assert(model.saw_1459 && !model.saw_1460 && model.refs == 0);

    /* Below 21.0.0 the 0x44-byte layout is unknown: no play-timer IPC at all. */
    reset_with(MAKEHOSVERSION(20, 5, 0));
    pctl_play_timer_query(&st);
    assert(!st.fw_supported && !st.session_valid && model.ipc_calls == 0);
    pctl_dump(report, sizeof(report));
    assert(strstr(report, "below 21.0.0") != NULL && !model.saw_1952 && model.refs == 0);

    /* Every reconnect in the dump can fail; the session is still released. */
    reset();
    pctl_dump(report, sizeof(report));
    const unsigned reconnects = model.init_calls;
    for (unsigned fail_at = 1; fail_at <= reconnects; ++fail_at) {
        reset();
        model.fail_init_call = fail_at;
        pctl_dump(report, sizeof(report));
        assert(strstr(report, "failed") != NULL);
        assert(model.init_calls == fail_at && model.exit_calls == fail_at - 1 && model.refs == 0);
    }

    const u32 commands[] = {1453, 1455, 1006, 1454, 145601};
    for (unsigned i = 0; i < 5; ++i) {
        reset();
        model.fail_command = commands[i];
        pctl_play_timer_query(&st);
        assert(model.refs == 0);
        switch (commands[i]) {
            case 1453: assert(!st.enabled_valid && st.enabled_rc == MOCK_ERROR); break;
            case 1455: assert(!st.restricted_valid && st.restricted_rc == MOCK_ERROR); break;
            case 1006: assert(!st.temporary_unlocked_valid && st.temporary_unlocked_rc == MOCK_ERROR); break;
            case 1454: assert(!st.remaining_valid && st.remaining_rc == MOCK_ERROR); break;
            case 145601: assert(!st.valid && st.config_rc == MOCK_ERROR && st.day_min[3] == PT_DAY_NOLIMIT); break;
        }
    }

    reset();
    model.fail_init_call = 1;
    pctl_status_fetch(&s);
    assert(s.session_rc == MOCK_ERROR && !s.restriction_enabled_ok && model.ipc_calls == 0);

    reset();
    model.fail_command = 1208;
    pctl_dump(report, sizeof(report));
    assert(strstr(report, "content=not recorded") != NULL && model.refs == 0 && model.writes == 0);

    reset();
    assert(pctl_ops_init() == 0);
    pctl_dump(report, 0);
    assert(model.refs == 0 && model.ipc_calls == 0);
}

static Result write_variant(unsigned variant)
{
    u16 days[7] = {0, 10, 20, 30, 40, 50, 60};
    switch (variant) {
        case 0: return pctl_play_timer_set_days(days);
        case 1: return pctl_play_timer_set_uniform(0);
        default: return pctl_play_timer_clear();
    }
}

static void test_read_only(void)
{
    PctlCustomSettings cs = {0, false, false};
    core_set_read_only(true);
    for (unsigned variant = 0; variant < 3; ++variant) {
        reset();
        assert(pctl_ops_init() == 0);
        assert(write_variant(variant) == NXM_RC_READ_ONLY);
        assert(model.refs == 0 && model.writes == 0);
    }
    reset();
    assert(pctl_delete_parental_controls() == NXM_RC_READ_ONLY);
    assert(pctl_delete_pairing() == NXM_RC_READ_ONLY);
    assert(pctl_set_pin() == NXM_RC_READ_ONLY);
    assert(pctl_unlock_restriction_temporarily() == NXM_RC_READ_ONLY);
    char pin[16];
    memset(pin, 'x', sizeof(pin));
    assert(pctl_get_pin(pin, sizeof(pin)) == NXM_RC_READ_ONLY);
    for (unsigned i = 0; i < sizeof(pin); ++i) assert(pin[i] == 0);
    assert(pctl_relock() == NXM_RC_READ_ONLY);
    assert(pctl_set_safety_level(PctlSafetyLevel_Teen) == NXM_RC_READ_ONLY);
    assert(pctl_set_custom_settings(&cs) == NXM_RC_READ_ONLY);
    assert(pctl_set_stereo_vision_restricted(true) == NXM_RC_READ_ONLY);
    assert(pctl_play_timer_set_alarm_disabled(true) == NXM_RC_READ_ONLY);
    assert(pctl_play_timer_start() == NXM_RC_READ_ONLY);
    assert(pctl_play_timer_stop() == NXM_RC_READ_ONLY);
    assert(model.init_calls == 0 && model.applets == 0 && model.writes == 0);

    /* Leaving read-only mode makes the same calls write again. */
    reset();
    core_set_read_only(false);
    assert(pctl_relock() == 0 && model.writes == 1 && model.last_write_cmd == 1007);
    assert(write_variant(1) == 0 && model.writes == 2);
    assert(model.refs == 0);
}

static void test_write_gate(void)
{
    /* states bit0 enabled, bit1 restricted, bit2 temporarily unlocked */
    const bool permitted[8] = {true, false, false, false, true, true, true, true};
    for (unsigned variant = 0; variant < 3; ++variant) {
        for (unsigned states = 0; states < 8; ++states) {
            reset();
            model.enabled = (states & 1) != 0;
            model.restricted = (states & 2) != 0;
            model.unlocked = (states & 4) != 0;
            Result rc = write_variant(variant);
            assert(R_SUCCEEDED(rc) == permitted[states]);
            if (!permitted[states]) assert(rc == NXM_RC_WRITE_GATED);
            assert(model.writes == (permitted[states] ? 1u : 0u));
            assert(model.refs == 0 && model.init_calls == model.exit_calls);
            if (permitted[states] && variant == 2)
                for (unsigned i = 0; i < 0x44; ++i) assert(model.last_write[i] == 0);
            if (permitted[states] && variant == 0) {
                u16 c[34];
                memcpy(c, model.last_write, sizeof(c));
                assert(c[0] == 0x0101 && c[8] == 0x0100 && c[9] == 0 && c[33] == 60);
            }
        }
        const u32 fails[] = {1453, 1455, 1006, 195101};
        for (unsigned i = 0; i < 4; ++i) {
            reset();
            model.fail_command = fails[i];
            assert(write_variant(variant) == MOCK_ERROR);
            assert(model.writes == (fails[i] == 195101 ? 1u : 0u));
            assert(model.refs == 0 && model.init_calls == 1 && model.exit_calls == 1);
        }
        reset();
        model.fail_init_call = 1;
        assert(write_variant(variant) == MOCK_ERROR);
        assert(model.ipc_calls == 0 && model.writes == 0 && model.refs == 0);
    }

    /* GetPlayTimerSettings failing while the timer is off does not block the
     * write: the layout observed on hardware is written, as earlier versions
     * always did. */
    reset();
    model.fail_command = 145601;
    assert(write_variant(0) == 0 && model.writes == 1 && model.refs == 0);
    {
        u16 c[34];
        memcpy(c, model.last_write, sizeof(c));
        assert(c[0] == 0x0101 && c[1] == 1 && c[7] == 0x0600 && c[8] == 0x0100 && c[9] == 0 && c[33] == 60);
    }
    /* While it is active (unlocked, so past the gate), the block holds what the
     * companion app set: a write built on zeros would wipe it, so refused —
     * unless no day keeps a limit, which writes all zeros whatever was read. */
    for (unsigned states = 1; states < 4; ++states) {
        for (unsigned variant = 0; variant < 3; ++variant) {
            reset();
            model.enabled = (states & 1) != 0;
            model.restricted = (states & 2) != 0;
            model.unlocked = true;
            model.fail_command = 145601;
            const Result rc = write_variant(variant);
            if (variant == 2) {
                assert(rc == 0 && model.writes == 1);
                for (unsigned i = 0; i < 0x44; ++i) assert(model.last_write[i] == 0);
            } else {
                assert(rc == NXM_RC_STATE_UNKNOWN && model.writes == 0);
            }
            assert(model.refs == 0 && model.init_calls == model.exit_calls);
        }
    }

    /* Out-of-range minutes and old firmware are refused before any IPC. */
    reset();
    u16 bad[7] = {0, 0, 0, 1441, 0, 0, 0};
    assert(pctl_play_timer_set_days(bad) == NXM_RC_INVALID_ARGUMENT && model.init_calls == 0);
    reset_with(MAKEHOSVERSION(20, 5, 0));
    assert(pctl_play_timer_clear() == NXM_RC_FW_UNSUPPORTED && model.init_calls == 0);
}

/* The other play-timer writes (alarm, start, stop) go through the same gate. */
static Result command_variant(unsigned variant)
{
    switch (variant) {
        case 0: return pctl_play_timer_set_alarm_disabled(true);
        case 1: return pctl_play_timer_start();
        default: return pctl_play_timer_stop();
    }
}

static void test_command_gate(void)
{
    static const u32 cmds[] = {1953, 1451, 1452};
    /* states bit0 enabled, bit1 restricted, bit2 temporarily unlocked */
    const bool permitted[8] = {true, false, false, false, true, true, true, true};
    for (unsigned variant = 0; variant < 3; ++variant) {
        for (unsigned states = 0; states < 8; ++states) {
            reset();
            model.enabled = (states & 1) != 0;
            model.restricted = (states & 2) != 0;
            model.unlocked = (states & 4) != 0;
            Result rc = command_variant(variant);
            assert(R_SUCCEEDED(rc) == permitted[states]);
            if (!permitted[states]) assert(rc == NXM_RC_WRITE_GATED);
            assert(model.writes == (permitted[states] ? 1u : 0u));
            if (permitted[states]) assert(model.last_write_cmd == cmds[variant]);
            assert(model.refs == 0 && model.init_calls == 1 && model.exit_calls == 1);
        }
        /* A gate read failing: refused with that error, nothing written. */
        const u32 fails[] = {1453, 1455, 1006};
        for (unsigned i = 0; i < 3; ++i) {
            reset();
            model.fail_command = fails[i];
            assert(command_variant(variant) == MOCK_ERROR);
            assert(model.writes == 0 && model.refs == 0 && model.init_calls == 1 && model.exit_calls == 1);
        }
        /* The session held by the caller is released first, then reopened. */
        reset();
        assert(pctl_ops_init() == 0);
        assert(command_variant(variant) == 0 && model.refs == 0 && model.init_calls == model.exit_calls);
        reset();
        model.fail_init_call = 1;
        assert(command_variant(variant) == MOCK_ERROR && model.ipc_calls == 0 && model.refs == 0);
    }
    /* The byte 1953 receives. */
    reset();
    assert(pctl_play_timer_set_alarm_disabled(false) == 0 && model.last_write_size == 1 && model.last_write[0] == 0);
    assert(pctl_play_timer_set_alarm_disabled(true) == 0 && model.last_write[0] == 1);
    assert(model.refs == 0);
}

/* The write starts from what 145601 returns and changes only flag + minutes. */
static void test_block_preserved(void)
{
    u16 c[34], expect[34];

    /* Fields the limits do not use: an odd header, [2..6], [+0] and [+3] of
     * every group, and a day (Monday) without a limit whose [+0] is set. The
     * [+3] (the next day's bedtime switch, low byte) stay off: pt_encode
     * keeps a day's times while its bedtime is on (test_bedtime). */
    reset();
    for (int i = 0; i < 34; i++) model.pt_block[i] = 0;
    model.pt_block[0] = 0x0103; model.pt_block[1] = 0x0002;
    for (int i = 2; i < 7; i++) model.pt_block[i] = (u16)(0xA000 + i);
    for (int n = 0; n < 7; n++) {
        u16 *g = &model.pt_block[7 + 4 * n];
        g[0] = (u16)(0x0700 + n);
        if (n != 1) { g[1] = 0x0100; g[2] = (u16)(60 + n); }
        if (n < 6) g[3] = (u16)(0xB000 + (n << 8));
    }
    memcpy(expect, model.pt_block, sizeof(expect));

    /* Writing back the limits just read: the same block, byte for byte. */
    PtState st;
    pctl_play_timer_query(&st);
    assert(st.valid && st.day_min[1] == PT_DAY_NOLIMIT && st.day_min[0] == 60);
    assert(pctl_play_timer_set_days(st.day_min) == 0 && model.refs == 0);
    assert(model.last_write_cmd == 195101 && memcmp(model.last_write, expect, sizeof(expect)) == 0);

    /* Sunday changes, Monday gains a limit, Tuesday loses its own, the rest
     * keeps theirs. */
    u16 days[7] = {90, 45, PT_DAY_NOLIMIT, 63, 64, 65, 66};
    assert(pctl_play_timer_set_days(days) == 0);
    memcpy(c, model.last_write, sizeof(c));
    expect[9] = 90;                                             /* Sunday: minutes only */
    expect[11] = 0x0600; expect[12] = 0x0100; expect[13] = 45;  /* Monday: [+3] kept */
    expect[15] = 0; expect[16] = 0; expect[17] = 0;             /* Tuesday: [+3] kept */
    assert(memcmp(c, expect, sizeof(c)) == 0);

    /* Timer off (all zeros): the observed header and per-day values. */
    reset();
    for (int i = 0; i < 34; i++) model.pt_block[i] = 0;
    u16 one[7] = {PT_DAY_NOLIMIT, 30, PT_DAY_NOLIMIT, PT_DAY_NOLIMIT, PT_DAY_NOLIMIT, PT_DAY_NOLIMIT, PT_DAY_NOLIMIT};
    assert(pctl_play_timer_set_days(one) == 0);
    memcpy(c, model.last_write, sizeof(c));
    assert(c[0] == 0x0101 && c[1] == 1 && c[11] == 0x0600 && c[12] == 0x0100 && c[13] == 30);
    for (int i = 0; i < 34; i++) if (i != 0 && i != 1 && (i < 11 || i > 13)) assert(c[i] == 0);

    /* No limit on any day: all zeros, whatever was read. */
    reset();
    model.pt_block[2] = 0xFFFF;
    assert(pctl_play_timer_clear() == 0);
    for (unsigned i = 0; i < 0x44; ++i) assert(model.last_write[i] == 0);
    assert(model.refs == 0);
}

static PtBedtime bed(bool on, u8 h, u8 m, u8 eh, u8 em)
{
    PtBedtime b = { on, h, m, eh, em };
    return b;
}

/* Bedtime: only its bytes change, the console's answer is checked, and the
 * block is put back when the console does not report it. */
static void test_bedtime(void)
{
    PtBedtime bt[7];
    u16 orig[34];

    /* The observed layout (limits, no bedtime): 21:00, allowed again at 07:30. */
    reset();
    model.bed_live = true;
    model.bed_day = 2;
    memcpy(orig, model.pt_block, sizeof(orig));
    for (int n = 0; n < 7; n++) bt[n] = bed(true, 21, 0, 7, 30);
    assert(pctl_play_timer_set_bedtime(bt, 2) == 0 && model.refs == 0 && model.writes == 1);
    for (int n = 0; n < 7; n++) {
        assert(model.pt_block[6 + 4 * n] == 0x1501);   /* on, 21 h */
        assert(model.pt_block[7 + 4 * n] == 0x0700);   /* :00, 07 h */
        assert(model.pt_block[8 + 4 * n] == 0x011E);   /* :30, the limit flag kept */
        assert(model.pt_block[9 + 4 * n] == 30 * n);   /* the limit kept */
    }
    assert(model.pt_block[0] == 0x0101 && model.pt_block[1] == 1);
    PtState st;
    pctl_play_timer_query(&st);
    assert(st.valid && st.bed[4].on && st.bed[4].hour == 21 && st.bed[4].end_minute == 30);
    assert(st.day_min[3] == 90 && st.bedtime_enabled && st.bedtime_hour == 21 && st.bedtime_minute == 0);

    /* The same bedtimes again: nothing written. */
    assert(pctl_play_timer_set_bedtime(bt, 2) == 0 && model.writes == 1);

    /* A limit changed meanwhile keeps the times; one removed too. */
    u16 days[7] = {10, PT_DAY_NOLIMIT, 20, 30, 40, 50, 60};
    assert(pctl_play_timer_set_days(days) == 0);
    assert(model.pt_block[7 + 4] == 0x0700 && model.pt_block[8 + 4] == 0x001E && model.pt_block[9 + 4] == 0);
    pctl_play_timer_query(&st);
    assert(st.day_min[1] == PT_DAY_NOLIMIT && st.day_min[0] == 10 && st.bed[1].on && st.bed[1].end_hour == 7);
    /* Every limit removed: the block stays while a bedtime is on. */
    assert(pctl_play_timer_clear() == 0);
    pctl_play_timer_query(&st);
    for (int n = 0; n < 7; n++) assert(st.day_min[n] == PT_DAY_NOLIMIT && st.bed[n].on);
    assert(model.pt_block[0] == 0x0101);
    /* A limit back on a day whose bedtime is on: its times stay. */
    u16 sat[7] = {PT_DAY_NOLIMIT, PT_DAY_NOLIMIT, PT_DAY_NOLIMIT, PT_DAY_NOLIMIT, PT_DAY_NOLIMIT, PT_DAY_NOLIMIT, 45};
    assert(pctl_play_timer_set_days(sat) == 0);
    assert(model.pt_block[31] == 0x0700 && model.pt_block[32] == 0x011E && model.pt_block[33] == 45);

    /* Off everywhere but the limit kept: switch and alarm cleared, the
     * allowed-again time kept (as the companion app does). */
    for (int n = 0; n < 7; n++) bt[n] = bed(false, 0, 0, 0, 0);
    assert(pctl_play_timer_set_bedtime(bt, 2) == 0);
    pctl_play_timer_query(&st);
    assert(st.day_min[6] == 45 && !st.bed[6].on && st.bed[6].hour == 0 && st.bed[6].end_hour == 7);
    /* Then no limit either: all zeros, as for the limits. */
    assert(pctl_play_timer_clear() == 0);
    for (int i = 0; i < 34; i++) assert(model.pt_block[i] == 0);

    /* Timer off: one evening on, the observed header added. */
    for (int n = 0; n < 7; n++) bt[n] = bed(false, 0, 0, 0, 0);
    bt[2] = bed(true, 20, 45, 6, 0);
    assert(pctl_play_timer_set_bedtime(bt, 2) == 0);
    assert(model.pt_block[0] == 0x0101 && model.pt_block[1] == 1);
    assert(model.pt_block[14] == 0x1401 && model.pt_block[15] == 0x062D && model.pt_block[16] == 0);
    for (int i = 2; i < 34; i++) if (i < 14 || i > 16) assert(model.pt_block[i] == 0);

    /* After midnight the console may still report the evening before. */
    model.bed_day = 2;
    bt[2] = bed(true, 22, 0, 6, 0);
    assert(pctl_play_timer_set_bedtime(bt, 3) == 0);
    model.bed_day = 4;
    bt[4] = bed(true, 23, 0, 6, 0);
    assert(pctl_play_timer_set_bedtime(bt, 3) == NXM_RC_NOT_APPLIED);   /* 23:00 is neither day 3's nor day 2's */

    /* Not reported: the block read before is written back. */
    reset();
    model.bed_live = true;
    model.bed_ignored = true;
    memcpy(orig, model.pt_block, sizeof(orig));
    for (int n = 0; n < 7; n++) bt[n] = bed(true, 21, 0, 7, 0);
    assert(pctl_play_timer_set_bedtime(bt, 0) == NXM_RC_NOT_APPLIED && model.refs == 0);
    assert(model.writes == 2 && memcmp(model.pt_block, orig, sizeof(orig)) == 0);
    /* ... and when the bedtime cannot be read back. */
    model.writes = 0;
    model.fail_command = 1957;
    assert(pctl_play_timer_set_bedtime(bt, 0) == NXM_RC_NOT_APPLIED && model.writes == 2);
    assert(memcmp(model.pt_block, orig, sizeof(orig)) == 0);

    /* Refused before writing: out of range, today out of range, the block
     * unreadable, the timer counting down. */
    reset();
    for (int n = 0; n < 7; n++) bt[n] = bed(true, 21, 0, 7, 0);
    bt[3] = bed(true, 15, 59, 7, 0);
    assert(pctl_play_timer_set_bedtime(bt, 0) == NXM_RC_INVALID_ARGUMENT);
    bt[3] = bed(true, 21, 0, 9, 1);
    assert(pctl_play_timer_set_bedtime(bt, 0) == NXM_RC_INVALID_ARGUMENT);
    bt[3] = bed(true, 21, 0, 4, 59);
    assert(pctl_play_timer_set_bedtime(bt, 0) == NXM_RC_INVALID_ARGUMENT);
    bt[3] = bed(false, 99, 99, 99, 99);   /* off: the times do not matter */
    assert(pctl_play_timer_set_bedtime(bt, 7) == NXM_RC_INVALID_ARGUMENT);
    assert(model.ipc_calls == 0);
    model.fail_command = 145601;
    assert(pctl_play_timer_set_bedtime(bt, 0) == NXM_RC_STATE_UNKNOWN && model.writes == 0 && model.refs == 0);
    model.fail_command = 0;
    model.enabled = true;
    assert(pctl_play_timer_set_bedtime(bt, 0) == NXM_RC_WRITE_GATED && model.writes == 0 && model.refs == 0);
    model.unlocked = true;
    assert(pctl_play_timer_set_bedtime(bt, 0) == NXM_RC_NOT_APPLIED);   /* the mock's fixed 21:30 */
    assert(model.writes == 2 && model.refs == 0);

    /* Below 21.0.0: nothing sent. */
    reset_with(MAKEHOSVERSION(20, 5, 0));
    assert(pctl_play_timer_set_bedtime(bt, 0) == NXM_RC_FW_UNSUPPORTED && model.ipc_calls == 0);
}

static void test_unlock_and_relock(void)
{
    reset();
    model.enabled = true;
    assert(pctl_unlock_restriction_temporarily() == 0);
    assert(model.unlocked && model.refs == 0 && model.init_calls == model.exit_calls);
    assert(model.got_pin_size == 7 && strcmp(model.got_pin, "123456") == 0);
    /* After a verified unlock the gate lets the write through. */
    assert(pctl_play_timer_set_uniform(45) == 0);
    assert(pctl_relock() == 0 && !model.unlocked && model.last_write_cmd == 1007);
    assert(model.refs == 0);

    reset();
    model.unlock_effective = false;
    assert(pctl_unlock_restriction_temporarily() == NXM_RC_UNLOCK_NOT_EFFECTIVE);
    assert(model.refs == 0);

    reset();
    model.fail_command = 1208;
    assert(pctl_unlock_restriction_temporarily() == MOCK_ERROR);
    assert(model.writes == 0 && model.refs == 0); /* no 1201 without a PIN */

    reset();
    model.fail_command = 1201;
    assert(pctl_unlock_restriction_temporarily() == MOCK_ERROR && model.refs == 0);
    assert(model.last_write_cmd == 1201);   /* nothing unlocked: nothing to undo */

    /* Unlocked, but the check cannot be read: locked again before returning. */
    reset();
    model.fail_command = 1006;
    assert(pctl_unlock_restriction_temporarily() == MOCK_ERROR);
    assert(model.last_write_cmd == 1007 && !model.unlocked);
    assert(model.refs == 0 && model.init_calls == model.exit_calls);

    /* ... and when locking again fails too, the caller learns it may still be
     * unlocked. */
    reset();
    model.fail_command = 1006;
    model.fail_command2 = 1007;
    assert(pctl_unlock_restriction_temporarily() == NXM_RC_RELOCK_FAILED);
    assert(model.last_write_cmd == 1007 && model.unlocked);
    assert(model.refs == 0 && model.init_calls == model.exit_calls);

    /* Not effective: nothing to undo either. */
    reset();
    model.unlock_effective = false;
    assert(pctl_unlock_restriction_temporarily() == NXM_RC_UNLOCK_NOT_EFFECTIVE);
    assert(model.last_write_cmd == 1201);
}

/* The rescue sysmodule's light read: 1206 and 1006 in one session, nothing written. */
static void test_lock_state(void)
{
    u32 len = 99;
    bool unlocked = true;

    reset();
    assert(pctl_lock_state(&len, &unlocked) == 0);
    assert(len == 6 && !unlocked && model.ipc_calls == 2 && model.writes == 0);
    assert(model.refs == 0 && model.init_calls == 1 && model.exit_calls == 1);

    reset();
    model.no_pin = true;
    model.unlocked = true;
    assert(pctl_lock_state(&len, NULL) == 0 && len == 0 && model.ipc_calls == 1);
    assert(pctl_lock_state(NULL, &unlocked) == 0 && unlocked);
    assert(model.refs == 0);

    reset();
    model.fail_command = 1206;
    assert(pctl_lock_state(&len, &unlocked) == MOCK_ERROR && model.ipc_calls == 1 && model.refs == 0);

    reset();
    model.fail_init_call = 1;
    assert(pctl_lock_state(&len, &unlocked) == MOCK_ERROR && model.refs == 0);
}

static void test_get_pin(void)
{
    char pin[16];

    reset();
    model.enabled = true;   /* a read: no unlock needed, nothing written */
    memset(pin, 'x', sizeof(pin));
    assert(pctl_get_pin(pin, sizeof(pin)) == 0);
    assert(strcmp(pin, "123456") == 0);
    assert(model.writes == 0 && !model.unlocked && model.refs == 0 && model.init_calls == model.exit_calls);

    reset();
    model.pin = "1234";
    assert(pctl_get_pin(pin, 5) == 0 && strcmp(pin, "1234") == 0 && model.refs == 0);

    /* Too small for the PIN + NUL: refused and wiped. */
    reset();
    memset(pin, 'x', sizeof(pin));
    assert(pctl_get_pin(pin, 6) == NXM_RC_INVALID_ARGUMENT);
    for (unsigned i = 0; i < 6; ++i) assert(pin[i] == 0);
    assert(model.refs == 0);

    /* Not 4-8 digits: never shown. */
    const char *bad[] = {"", "123", "123456789", "12a4"};
    for (unsigned i = 0; i < 4; ++i) {
        reset();
        model.pin = bad[i];
        memset(pin, 'x', sizeof(pin));
        assert(pctl_get_pin(pin, sizeof(pin)) == NXM_RC_STATE_UNKNOWN);
        for (unsigned j = 0; j < sizeof(pin); ++j) assert(pin[j] == 0);
        assert(model.refs == 0);
    }

    reset();
    model.fail_command = 1208;
    memset(pin, 'x', sizeof(pin));
    assert(pctl_get_pin(pin, sizeof(pin)) == MOCK_ERROR && pin[0] == 0 && model.refs == 0);

    reset();
    model.fail_init_call = 1;
    assert(pctl_get_pin(pin, sizeof(pin)) == MOCK_ERROR && model.ipc_calls == 0 && model.refs == 0);

    reset();
    assert(pctl_get_pin(NULL, 8) == NXM_RC_INVALID_ARGUMENT && model.init_calls == 0);
}

static void test_other_writes(void)
{
    PctlCustomSettings cs = {16, true, false};

    reset();
    model.safety_level = PctlSafetyLevel_Teen;
    assert(pctl_set_custom_settings(&cs) == NXM_RC_NOT_CUSTOM && model.writes == 0 && model.refs == 0);

    reset();
    assert(pctl_set_custom_settings(&cs) == 0 && model.last_write_cmd == 1036);
    assert(model.last_write[0] == 16 && model.last_write[1] == 1 && model.last_write[2] == 0);
    assert(model.refs == 0);
    cs.rating_age = 99;
    assert(pctl_set_custom_settings(&cs) == NXM_RC_INVALID_ARGUMENT);

    reset();
    assert(pctl_set_safety_level(7) == NXM_RC_INVALID_ARGUMENT && model.init_calls == 0);
    assert(pctl_set_safety_level(PctlSafetyLevel_Child) == 0 && model.safety_level == PctlSafetyLevel_Child);
    assert(pctl_set_stereo_vision_restricted(true) == 0 && model.last_write_cmd == 1063);
    assert(pctl_play_timer_set_alarm_disabled(true) == 0 && model.last_write_cmd == 1953);
    assert(pctl_play_timer_stop() == 0 && model.last_write_cmd == 1452);
    assert(pctl_play_timer_start() == 0 && model.last_write_cmd == 1451);
    assert(model.refs == 0 && model.init_calls == model.exit_calls);

    reset();
    assert(pctl_ops_init() == 0);
    assert(pctl_set_pin() == MOCK_ERROR);
    assert(model.applets == 1 && model.refs == 0);

    const u32 deletes[] = {1043, 1941};
    for (unsigned i = 0; i < 2; ++i) {
        reset();
        model.fail_command = deletes[i];
        Result rc = i == 0 ? pctl_delete_parental_controls() : pctl_delete_pairing();
        assert(rc == MOCK_ERROR && model.writes == 1 && model.refs == 0);
        assert(model.init_calls == 1 && model.exit_calls == 1);
    }
}

static void test_level_settings_and_rating_org(void)
{
    /* 1034: read only, what a preset restricts. */
    reset();
    PctlCustomSettings s;
    memset(&s, 0xFF, sizeof(s));
    assert(pctl_get_level_settings(PctlSafetyLevel_Child, &s) == 0);
    assert(s.rating_age == 13 && s.sns_post_restriction && s.free_communication_restriction);
    assert(pctl_get_level_settings(PctlSafetyLevel_Teen, &s) == 0 && s.rating_age == 16 && !s.sns_post_restriction);
    assert(pctl_get_level_settings(5, &s) == NXM_RC_INVALID_ARGUMENT);
    assert(pctl_get_level_settings(PctlSafetyLevel_Child, NULL) == NXM_RC_INVALID_ARGUMENT);
    assert(model.writes == 0 && model.refs == 0);
    /* Read-only mode does not stop a read. */
    core_set_read_only(true);
    assert(pctl_get_level_settings(PctlSafetyLevel_Teen, &s) == 0);
    core_set_read_only(false);

    /* 1038: a change, the value checked first. */
    reset();
    assert(pctl_set_rating_org(6) == 0 && model.writes == 1 && model.last_write_cmd == 1038 && model.rating_org == 6);
    assert(pctl_set_rating_org(13) == NXM_RC_INVALID_ARGUMENT && model.writes == 1);
    core_set_read_only(true);
    assert(pctl_set_rating_org(3) == NXM_RC_READ_ONLY && model.writes == 1);
    core_set_read_only(false);
    assert(model.refs == 0);
}

static void test_ask_pin(void)
{
    /* The PIN screen: shown with the session released, only when a PIN exists. */
    reset();
    assert(pctl_ask_pin() == 0 && model.auth_applets == 1 && model.writes == 0);
    model.auth_result = MOCK_ERROR;   /* cancelled */
    assert(pctl_ask_pin() == MOCK_ERROR && model.auth_applets == 2);
    assert(model.refs == 0);

    reset();
    model.no_pin = true;
    assert(pctl_ask_pin() == NXM_RC_NO_PIN && model.auth_applets == 0);
    assert(model.refs == 0);

    /* Read-only mode changes nothing about it: it writes nothing. */
    reset();
    core_set_read_only(true);
    assert(pctl_ask_pin() == 0 && model.auth_applets == 1);
    core_set_read_only(false);
    assert(model.refs == 0);
}

/* The UI's change check: runs before any session is opened (it may show the
   PIN applet), refuses every change but locking again. */
static bool change_check(void)
{
    assert(model.refs == 0);
    model.checks++;
    return model.check_answer;
}

static void test_change_check(void)
{
    reset();
    core_set_change_check(change_check);
    model.check_answer = false;
    u16 days[7] = { 60, 60, 60, 60, 60, 60, 60 };
    char pin[16];
    PctlCustomSettings cs = { 12, true, false };
    assert(pctl_play_timer_set_days(days) == NXM_RC_NOT_CONFIRMED);
    assert(pctl_unlock_restriction_temporarily() == NXM_RC_NOT_CONFIRMED);
    assert(pctl_get_pin(pin, sizeof(pin)) == NXM_RC_NOT_CONFIRMED && pin[0] == '\0');
    assert(pctl_set_safety_level(PctlSafetyLevel_Teen) == NXM_RC_NOT_CONFIRMED);
    assert(pctl_set_custom_settings(&cs) == NXM_RC_NOT_CONFIRMED);
    assert(pctl_set_stereo_vision_restricted(true) == NXM_RC_NOT_CONFIRMED);
    assert(pctl_delete_parental_controls() == NXM_RC_NOT_CONFIRMED);
    assert(pctl_delete_pairing() == NXM_RC_NOT_CONFIRMED);
    assert(pctl_play_timer_set_alarm_disabled(true) == NXM_RC_NOT_CONFIRMED);
    assert(pctl_play_timer_start() == NXM_RC_NOT_CONFIRMED);
    assert(pctl_play_timer_stop() == NXM_RC_NOT_CONFIRMED);
    assert(pctl_set_pin() == NXM_RC_NOT_CONFIRMED && model.applets == 0);
    assert(model.writes == 0 && model.ipc_calls == 0 && model.checks == 12);
    /* Locking again is never refused for want of a PIN. */
    assert(pctl_relock() == 0 && model.writes == 1 && model.checks == 12);
    /* Reads never ask. */
    PctlStatus st;
    pctl_status_fetch(&st);
    assert(model.checks == 12);

    /* Confirmed: the change goes through. */
    model.check_answer = true;
    assert(pctl_set_safety_level(PctlSafetyLevel_Teen) == 0 && model.checks == 13);
    /* Read-only still wins, without asking. */
    core_set_read_only(true);
    assert(pctl_set_safety_level(PctlSafetyLevel_Teen) == NXM_RC_READ_ONLY && model.checks == 13);
    core_set_read_only(false);
    core_set_change_check(NULL);
    assert(model.refs == 0);
}

int main(void)
{
    test_ownership();
    test_reads();
    test_sample();
    test_overview();
    test_write_gate();
    test_command_gate();
    test_block_preserved();
    test_bedtime();
    test_unlock_and_relock();
    test_lock_state();
    test_get_pin();
    test_other_writes();
    test_read_only();
    test_level_settings_and_rating_org();
    test_ask_pin();
    test_change_check();
    puts("pctl_ops lifecycle, gating, write and read-only assertions passed");
    (void)assert_released;
    return 0;
}
