#include "check.h"
#include <stdio.h>
#include <string.h>
#include "time_ops.h"
#include "write_guard.h"

enum { MOCK_ERROR = 0x701, ROOT_KIND = 99, MAX_HANDLES = 64 };
typedef struct { bool active; unsigned root_call; u32 kind; } Handle;
static struct {
    Handle handles[MAX_HANDLES];
    unsigned next_handle, active, opened, closed, root_calls, writes;
    unsigned fail_root_call, fail_child_root, fail_read_root, fail_flag_root;
    u32 fail_child_kind, fail_read_kind, fail_flag_command;
    bool fail_write, automatic, accuracy, use_readback;
    u64 clock_value, written, readback;
    bool fail_now;            /* timeGetCurrentTime fails */
    u64 now;                  /* what it returns */
    int posix_count;          /* timeToPosixTimeWithMyRule: how many candidates */
    u64 posix0;               /* the first one (the next is an hour later) */
} model;

static void reset(void)
{
    CHECK(model.active == 0);
    memset(&model, 0, sizeof(model));
    model.automatic = true;
    model.accuracy = true;
    model.clock_value = 1000;
}

static void assert_released(void)
{
    CHECK(model.active == 0);
    CHECK(model.opened == model.closed);
}

static void open_handle(Service *service, unsigned root_call, u32 kind)
{
    CHECK(service->handle == 0);
    CHECK(model.next_handle + 1 < MAX_HANDLES);
    service->handle = ++model.next_handle;
    model.handles[service->handle] = (Handle){ true, root_call, kind };
    model.active++;
    model.opened++;
    /* Only one root and its current child may be open at once. */
    CHECK(model.active <= 2);
}

Result smGetService(Service *service, const char *name)
{
    CHECK(strcmp(name, "time:s") == 0);
    CHECK(model.active == 0); /* A prior operation must have released its root. */
    model.root_calls++;
    if (model.root_calls == model.fail_root_call) return MOCK_ERROR;
    open_handle(service, model.root_calls, ROOT_KIND);
    return 0;
}

void serviceClose(Service *service)
{
    if (service->handle == 0) return; /* libnx accepts an inactive service. */
    CHECK(service->handle < MAX_HANDLES);
    Handle *handle = &model.handles[service->handle];
    CHECK(handle->active);
    if (handle->kind == ROOT_KIND) CHECK(model.active == 1);
    handle->active = false;
    model.active--;
    model.closed++;
    service->handle = 0;
}

Result mock_dispatch(Service *service, u32 command, void *out, size_t out_size,
                     const void *in, size_t in_size, SfDispatchParams params)
{
    REQUIRE(service->handle > 0 && service->handle < MAX_HANDLES);
    Handle *handle = &model.handles[service->handle];
    CHECK(handle->active); /* No IPC may use a released handle. */
    if (handle->kind == ROOT_KIND) {
        CHECK(in == NULL && in_size == 0);
        if (params.out_num_objects != 0) {
            REQUIRE(params.out_num_objects == 1 && params.out_objects != NULL);
            CHECK(out == NULL && out_size == 0);
            CHECK(command == 0 || command == 1 || command == 3 || command == 4);
            if (handle->root_call == model.fail_child_root && command == model.fail_child_kind)
                return MOCK_ERROR;
            open_handle(params.out_objects, handle->root_call, command);
            return 0;
        }
        CHECK(command == 100 || command == 200);
        REQUIRE(out != NULL && out_size == 1);
        if (handle->root_call == model.fail_flag_root && command == model.fail_flag_command)
            return MOCK_ERROR;
        bool value = command == 100 ? model.automatic : model.accuracy;
        memcpy(out, &value, sizeof(value));
        return 0;
    }
    CHECK(params.out_num_objects == 0);
    if (in != NULL) {
        CHECK(handle->kind == 1 && command == 1);
        REQUIRE(in_size == sizeof(u64) && out == NULL);
        model.writes++;
        memcpy(&model.written, in, sizeof(model.written));
        if (model.fail_write) return MOCK_ERROR;
        model.clock_value = model.written;
        return 0;
    }
    if (handle->kind == 3) { /* ITimeZoneService::GetDeviceLocationName */
        REQUIRE(command == 0 && out != NULL && out_size == 0x24);
        memset(out, 0, out_size);
        memcpy(out, "Europe/Paris", 12);
        return 0;
    }
    REQUIRE(command == 0 && out != NULL && out_size == sizeof(u64));
    if (handle->root_call == model.fail_read_root && handle->kind == model.fail_read_kind)
        return MOCK_ERROR;
    u64 value = model.clock_value;
    if (model.use_readback && handle->root_call == 2 && handle->kind == 1)
        value = model.readback;
    memcpy(out, &value, sizeof(value));
    return 0;
}

static void test_network_accuracy(void)
{
    bool accurate = true;
    reset();
    model.accuracy = false;
    CHECK(time_network_accuracy(&accurate) == 0 && !accurate);
    CHECK(model.root_calls == 1 && model.opened == 1);   /* no clock sub-session */
    assert_released();

    reset();
    CHECK(time_network_accuracy(&accurate) == 0 && accurate);
    assert_released();

    reset();
    model.fail_root_call = 1;
    CHECK(time_network_accuracy(&accurate) == MOCK_ERROR && !accurate);
    assert_released();

    reset();
    model.fail_flag_root = 1;
    model.fail_flag_command = 200;
    CHECK(time_network_accuracy(&accurate) == MOCK_ERROR && !accurate);
    assert_released();
}

static void test_snapshot_failures(void)
{
    TimeSnapshot snapshot;
    reset();
    time_clock_snapshot(&snapshot);
    CHECK(snapshot.service_rc == 0 && snapshot.user_rc == 0 && snapshot.network_rc == 0);
    CHECK(snapshot.local_rc == 0 && snapshot.automatic_rc == 0 && snapshot.accuracy_rc == 0);
    CHECK(snapshot.user_time == 1000 && snapshot.network_time == 1000 && snapshot.local_time == 1000);
    CHECK(snapshot.automatic && snapshot.accuracy);
    CHECK(snapshot.location_rc == 0 && strcmp(snapshot.location, "Europe/Paris") == 0);
    assert_released();

    reset();
    model.fail_root_call = 1;
    time_clock_snapshot(&snapshot);
    CHECK(snapshot.service_rc == MOCK_ERROR && snapshot.user_rc == MOCK_ERROR);
    CHECK(snapshot.network_rc == MOCK_ERROR && snapshot.local_rc == MOCK_ERROR);
    CHECK(snapshot.automatic_rc == MOCK_ERROR && snapshot.accuracy_rc == MOCK_ERROR);
    CHECK(!snapshot.automatic && !snapshot.accuracy);
    CHECK(model.opened == 0);
    assert_released();

    const u32 clock_commands[] = { 0, 1, 4 };
    for (unsigned i = 0; i < 3; i++) {
        for (unsigned read_failure = 0; read_failure < 2; read_failure++) {
            reset();
            if (read_failure) {
                model.fail_read_root = 1;
                model.fail_read_kind = clock_commands[i];
            } else {
                model.fail_child_root = 1;
                model.fail_child_kind = clock_commands[i];
            }
            time_clock_snapshot(&snapshot);
            Result results[] = { snapshot.user_rc, snapshot.network_rc, snapshot.local_rc };
            u64 values[] = { snapshot.user_time, snapshot.network_time, snapshot.local_time };
            for (unsigned j = 0; j < 3; j++) {
                CHECK(results[j] == (i == j ? MOCK_ERROR : 0));
                CHECK(values[j] == (i == j ? 0 : 1000));
            }
            assert_released();
        }
    }
    for (unsigned i = 0; i < 2; i++) {
        reset();
        model.fail_flag_root = 1;
        model.fail_flag_command = i ? 200 : 100;
        time_clock_snapshot(&snapshot);
        CHECK((i ? snapshot.accuracy_rc : snapshot.automatic_rc) == MOCK_ERROR);
        CHECK(!(i ? snapshot.accuracy : snapshot.automatic));
        assert_released();
    }
}

static void test_automatic_gate(bool read_only)
{
    TimeApply apply;
    core_set_read_only(read_only);
    for (unsigned scenario = 0; scenario < 3; scenario++) {
        reset();
        if (scenario == 0) model.automatic = false;
        if (scenario == 1) {
            model.fail_flag_root = 1;
            model.fail_flag_command = 100;
        }
        if (scenario == 2) model.fail_root_call = 1;
        time_clock_apply(2000, &apply);
        CHECK(!apply.write_attempted && !apply.verify_attempted && !apply.verified);
        CHECK(model.writes == 0);
        if (read_only) {
            CHECK(apply.open_rc == NXM_RC_READ_ONLY);   /* read-only is checked first */
        } else {
            CHECK(apply.refused_automatic);
            CHECK(model.root_calls == 1);
        }
        assert_released();
    }
    core_set_read_only(false);
}

static void test_apply_failures(void)
{
    TimeApply apply;
    for (unsigned scenario = 0; scenario < 4; scenario++) {
        reset();
        if (scenario == 0) model.fail_root_call = 2;
        if (scenario == 1) { model.fail_child_root = 2; model.fail_child_kind = 1; }
        if (scenario == 2) model.fail_write = true;
        if (scenario == 3) { model.fail_read_root = 2; model.fail_read_kind = 1; }
        time_clock_apply(2000, &apply);
        CHECK(!apply.verified);
        if (scenario < 2) {
            CHECK(apply.open_rc == MOCK_ERROR && !apply.write_attempted);
            CHECK(model.writes == 0);
        } else {
            CHECK(apply.open_rc == 0 && apply.write_attempted && model.writes == 1);
            CHECK(apply.write_rc == (scenario == 2 ? MOCK_ERROR : 0));
            CHECK(apply.verify_attempted == (scenario == 3));
            if (scenario == 3) CHECK(apply.verify_rc == MOCK_ERROR);
        }
        CHECK(apply.after.service_rc == 0);
        assert_released();
    }
}

static void test_readback_and_accuracy(void)
{
    const u64 readbacks[] = { 1999, 2000, 2005, 2006 };
    for (unsigned i = 0; i < 4; i++) {
        TimeApply apply;
        reset();
        model.use_readback = true;
        model.readback = readbacks[i];
        time_clock_apply(2000, &apply);
        CHECK(apply.write_attempted && apply.verify_attempted);
        CHECK(apply.write_rc == 0 && apply.verify_rc == 0);
        CHECK(apply.verified == (i == 1 || i == 2));
        CHECK(apply.readback == readbacks[i]);
        CHECK(model.writes == 1 && model.written == 2000);
        assert_released();
    }
    for (unsigned failed_accuracy = 0; failed_accuracy < 2; failed_accuracy++) {
        TimeApply apply;
        reset();
        model.accuracy = false;
        if (failed_accuracy) { model.fail_flag_root = 3; model.fail_flag_command = 200; }
        time_clock_apply(2000, &apply);
        CHECK(apply.verified); /* Readback success cannot manufacture accuracy. */
        CHECK(!apply.before.accuracy && !apply.after.accuracy);
        CHECK(apply.after.accuracy_rc == (failed_accuracy ? MOCK_ERROR : 0));
        assert_released();
    }
    /* Overflow boundary: readback subtraction must never wrap into success. */
    TimeApply apply;
    reset();
    model.use_readback = true;
    model.readback = 0;
    time_clock_apply(UINT64_MAX, &apply);
    CHECK(!apply.verified);
    assert_released();
}

static void test_read_only(void)
{
    TimeApply apply;
    reset();
    core_set_read_only(true);
    time_clock_apply(2000, &apply);
    CHECK(apply.open_rc == NXM_RC_READ_ONLY);
    CHECK(!apply.write_attempted && !apply.verify_attempted && !apply.verified);
    CHECK(model.writes == 0 && model.clock_value == 1000);
    CHECK(model.root_calls == 2); /* Both snapshots release their own handles. */
    assert_released();

    /* Leaving read-only mode makes the clock writable again. */
    reset();
    core_set_read_only(false);
    time_clock_apply(2000, &apply);
    CHECK(apply.write_attempted && model.writes == 1);
    assert_released();
}

Result timeToCalendarTimeWithMyRule(u64 timestamp, TimeCalendarTime *caltime, TimeCalendarAdditionalInfo *info)
{
    if (timestamp == 0) return MOCK_ERROR;
    memset(caltime, 0, sizeof(*caltime));
    caltime->year = 2026; caltime->month = 10; caltime->day = 7;
    caltime->hour = 14; caltime->minute = 3; caltime->second = 12;
    if (info) { memset(info, 0, sizeof(*info)); info->wday = 3; info->offset = 7200; }
    return 0;
}

Result timeToPosixTimeWithMyRule(const TimeCalendarTime *caltime, u64 *list, s32 list_count, s32 *count)
{
    REQUIRE(caltime && list && count && list_count >= 2);
    if (model.posix_count < 0) return MOCK_ERROR;
    for (int i = 0; i < model.posix_count && i < list_count; i++) list[i] = model.posix0 + 3600u * (u64)i;
    *count = model.posix_count;
    return 0;
}

Result timeGetCurrentTime(TimeType type, u64 *timestamp)
{
    CHECK(type == TimeType_UserSystemClock);   /* what the HOME menu shows */
    if (model.fail_now) return MOCK_ERROR;
    *timestamp = model.now;
    return 0;
}

static bool refuse(void) { return false; }

static void test_change_check(void)
{
    /* Setting the network clock is a change: the PIN check can refuse it. */
    reset();
    model.automatic = true;
    core_set_change_check(refuse);
    TimeApply apply;
    time_clock_apply(2000, &apply);
    CHECK(apply.open_rc == NXM_RC_NOT_CONFIRMED && !apply.write_attempted && model.writes == 0);
    core_set_change_check(NULL);
    assert_released();
}

static void test_local_time(void)
{
    reset();
    model.now = 1791374592;
    u64 posix = 0;
    LocalTime l;
    CHECK(time_local_now(&posix, &l));
    CHECK(posix == model.now);
    CHECK(l.year == 2026 && l.month == 10 && l.day == 7 && l.hour == 14 && l.minute == 3 && l.second == 12);
    CHECK(l.wday == 3);
    CHECK(time_local_now(&posix, NULL) && posix == model.now);

    /* The live clock unreadable: the C library's clock instead. */
    model.fail_now = true;
    CHECK(time_local_now(&posix, &l) && posix > 1700000000u);
    /* No local time for it (the rule refuses 0): false, POSIX time still set. */
    model.fail_now = false;
    model.now = 0;
    CHECK(!time_local_now(&posix, &l) && posix == 0);

    /* The rule's wall-time lookup: 0, 1 or 2 answers, never more. */
    const TimeRule *rule = time_console_rule();
    const LocalTime wall = { 2026, 10, 25, 2, 30, 0, 0 };
    u64 c[2] = { 0, 0 };
    model.posix_count = 2; model.posix0 = 5000;
    CHECK(rule->to_posix(rule->ctx, &wall, c) == 2 && c[0] == 5000 && c[1] == 8600);
    model.posix_count = 1;
    CHECK(rule->to_posix(rule->ctx, &wall, c) == 1);
    model.posix_count = 0;
    CHECK(rule->to_posix(rule->ctx, &wall, c) == 0);
    model.posix_count = -1;   /* the service fails */
    CHECK(rule->to_posix(rule->ctx, &wall, c) == 0);
    CHECK(model.active == 0);   /* no time:s handle involved */
}

static void test_formatting(void)
{
    char text[64];
    time_format_local(1, text, sizeof(text));
    CHECK(strcmp(text, "2026-10-07 14:03:12") == 0);
    time_format_local(0, text, sizeof(text)); /* falls back to UTC */
    CHECK(strcmp(text, "1970-01-01 00:00:00 UTC") == 0);
    time_format_utc(1791381792ULL, text, sizeof(text));
    CHECK(strstr(text, " UTC") != NULL);
}

static void test_dump_and_repetition(void)
{
    char dump[1024];
    TimeSnapshot snapshot;
    reset();
    model.fail_flag_root = 1;
    model.fail_flag_command = 200;
    time_clock_dump(dump, sizeof(dump));
    CHECK(strstr(dump, "Network clock accuracy sufficient: rc=0x00000701 unavailable") != NULL);
    CHECK(strstr(dump, "Clock service handles released.") != NULL);
    assert_released();
    for (unsigned i = 0; i < 20; i++) {
        reset();
        time_clock_snapshot(&snapshot);
        time_clock_dump(dump, sizeof(dump));
        CHECK(model.writes == 0);
        assert_released();
    }
}

int main(void)
{
    test_snapshot_failures();
    test_network_accuracy();
    test_automatic_gate(false);
    test_automatic_gate(true);
    test_apply_failures();
    test_readback_and_accuracy();
    test_read_only();
    test_dump_and_repetition();
    test_formatting();
    test_local_time();
    test_change_check();
    return CHECK_DONE("time_ops lifecycle and read-only tests passed");
}
