#include <assert.h>
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
} model;

static void reset(void)
{
    assert(model.active == 0);
    memset(&model, 0, sizeof(model));
    model.automatic = true;
    model.accuracy = true;
    model.clock_value = 1000;
}

static void assert_released(void)
{
    assert(model.active == 0);
    assert(model.opened == model.closed);
}

static void open_handle(Service *service, unsigned root_call, u32 kind)
{
    assert(service->handle == 0);
    assert(model.next_handle + 1 < MAX_HANDLES);
    service->handle = ++model.next_handle;
    model.handles[service->handle] = (Handle){ true, root_call, kind };
    model.active++;
    model.opened++;
    /* Only one root and its current child may be open at once. */
    assert(model.active <= 2);
}

Result smGetService(Service *service, const char *name)
{
    assert(strcmp(name, "time:s") == 0);
    assert(model.active == 0); /* A prior operation must have released its root. */
    model.root_calls++;
    if (model.root_calls == model.fail_root_call) return MOCK_ERROR;
    open_handle(service, model.root_calls, ROOT_KIND);
    return 0;
}

void serviceClose(Service *service)
{
    if (service->handle == 0) return; /* libnx accepts an inactive service. */
    assert(service->handle < MAX_HANDLES);
    Handle *handle = &model.handles[service->handle];
    assert(handle->active);
    if (handle->kind == ROOT_KIND) assert(model.active == 1);
    handle->active = false;
    model.active--;
    model.closed++;
    service->handle = 0;
}

Result mock_dispatch(Service *service, u32 command, void *out, size_t out_size,
                     const void *in, size_t in_size, SfDispatchParams params)
{
    assert(service->handle > 0 && service->handle < MAX_HANDLES);
    Handle *handle = &model.handles[service->handle];
    assert(handle->active); /* No IPC may use a released handle. */
    if (handle->kind == ROOT_KIND) {
        assert(in == NULL && in_size == 0);
        if (params.out_num_objects != 0) {
            assert(params.out_num_objects == 1 && params.out_objects != NULL);
            assert(out == NULL && out_size == 0);
            assert(command == 0 || command == 1 || command == 3 || command == 4);
            if (handle->root_call == model.fail_child_root && command == model.fail_child_kind)
                return MOCK_ERROR;
            open_handle(params.out_objects, handle->root_call, command);
            return 0;
        }
        assert(command == 100 || command == 200);
        assert(out != NULL && out_size == 1);
        if (handle->root_call == model.fail_flag_root && command == model.fail_flag_command)
            return MOCK_ERROR;
        bool value = command == 100 ? model.automatic : model.accuracy;
        memcpy(out, &value, sizeof(value));
        return 0;
    }
    assert(params.out_num_objects == 0);
    if (in != NULL) {
        assert(handle->kind == 1 && command == 1);
        assert(in_size == sizeof(u64) && out == NULL);
        model.writes++;
        memcpy(&model.written, in, sizeof(model.written));
        if (model.fail_write) return MOCK_ERROR;
        model.clock_value = model.written;
        return 0;
    }
    if (handle->kind == 3) { /* ITimeZoneService::GetDeviceLocationName */
        assert(command == 0 && out != NULL && out_size == 0x24);
        memset(out, 0, out_size);
        memcpy(out, "Europe/Paris", 12);
        return 0;
    }
    assert(command == 0 && out != NULL && out_size == sizeof(u64));
    if (handle->root_call == model.fail_read_root && handle->kind == model.fail_read_kind)
        return MOCK_ERROR;
    u64 value = model.clock_value;
    if (model.use_readback && handle->root_call == 2 && handle->kind == 1)
        value = model.readback;
    memcpy(out, &value, sizeof(value));
    return 0;
}

static void test_snapshot_failures(void)
{
    TimeSnapshot snapshot;
    reset();
    time_clock_snapshot(&snapshot);
    assert(snapshot.service_rc == 0 && snapshot.user_rc == 0 && snapshot.network_rc == 0);
    assert(snapshot.local_rc == 0 && snapshot.automatic_rc == 0 && snapshot.accuracy_rc == 0);
    assert(snapshot.user_time == 1000 && snapshot.network_time == 1000 && snapshot.local_time == 1000);
    assert(snapshot.automatic && snapshot.accuracy);
    assert(snapshot.location_rc == 0 && strcmp(snapshot.location, "Europe/Paris") == 0);
    assert_released();

    reset();
    model.fail_root_call = 1;
    time_clock_snapshot(&snapshot);
    assert(snapshot.service_rc == MOCK_ERROR && snapshot.user_rc == MOCK_ERROR);
    assert(snapshot.network_rc == MOCK_ERROR && snapshot.local_rc == MOCK_ERROR);
    assert(snapshot.automatic_rc == MOCK_ERROR && snapshot.accuracy_rc == MOCK_ERROR);
    assert(!snapshot.automatic && !snapshot.accuracy);
    assert(model.opened == 0);
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
                assert(results[j] == (i == j ? MOCK_ERROR : 0));
                assert(values[j] == (i == j ? 0 : 1000));
            }
            assert_released();
        }
    }
    for (unsigned i = 0; i < 2; i++) {
        reset();
        model.fail_flag_root = 1;
        model.fail_flag_command = i ? 200 : 100;
        time_clock_snapshot(&snapshot);
        assert((i ? snapshot.accuracy_rc : snapshot.automatic_rc) == MOCK_ERROR);
        assert(!(i ? snapshot.accuracy : snapshot.automatic));
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
        assert(!apply.write_attempted && !apply.verify_attempted && !apply.verified);
        assert(model.writes == 0);
        if (read_only) {
            assert(apply.open_rc == NXM_RC_READ_ONLY);   /* read-only is checked first */
        } else {
            assert(apply.refused_automatic);
            assert(model.root_calls == 1);
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
        assert(!apply.verified);
        if (scenario < 2) {
            assert(apply.open_rc == MOCK_ERROR && !apply.write_attempted);
            assert(model.writes == 0);
        } else {
            assert(apply.open_rc == 0 && apply.write_attempted && model.writes == 1);
            assert(apply.write_rc == (scenario == 2 ? MOCK_ERROR : 0));
            assert(apply.verify_attempted == (scenario == 3));
            if (scenario == 3) assert(apply.verify_rc == MOCK_ERROR);
        }
        assert(apply.after.service_rc == 0);
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
        assert(apply.write_attempted && apply.verify_attempted);
        assert(apply.write_rc == 0 && apply.verify_rc == 0);
        assert(apply.verified == (i == 1 || i == 2));
        assert(apply.readback == readbacks[i]);
        assert(model.writes == 1 && model.written == 2000);
        assert_released();
    }
    for (unsigned failed_accuracy = 0; failed_accuracy < 2; failed_accuracy++) {
        TimeApply apply;
        reset();
        model.accuracy = false;
        if (failed_accuracy) { model.fail_flag_root = 3; model.fail_flag_command = 200; }
        time_clock_apply(2000, &apply);
        assert(apply.verified); /* Readback success cannot manufacture accuracy. */
        assert(!apply.before.accuracy && !apply.after.accuracy);
        assert(apply.after.accuracy_rc == (failed_accuracy ? MOCK_ERROR : 0));
        assert_released();
    }
    /* Overflow boundary: readback subtraction must never wrap into success. */
    TimeApply apply;
    reset();
    model.use_readback = true;
    model.readback = 0;
    time_clock_apply(UINT64_MAX, &apply);
    assert(!apply.verified);
    assert_released();
}

static void test_read_only(void)
{
    TimeApply apply;
    reset();
    core_set_read_only(true);
    time_clock_apply(2000, &apply);
    assert(apply.open_rc == NXM_RC_READ_ONLY);
    assert(!apply.write_attempted && !apply.verify_attempted && !apply.verified);
    assert(model.writes == 0 && model.clock_value == 1000);
    assert(model.root_calls == 2); /* Both snapshots release their own handles. */
    assert_released();

    /* Leaving read-only mode makes the clock writable again. */
    reset();
    core_set_read_only(false);
    time_clock_apply(2000, &apply);
    assert(apply.write_attempted && model.writes == 1);
    assert_released();
}

Result timeToCalendarTimeWithMyRule(u64 timestamp, TimeCalendarTime *caltime, TimeCalendarAdditionalInfo *info)
{
    (void)info;
    if (timestamp == 0) return MOCK_ERROR;
    memset(caltime, 0, sizeof(*caltime));
    caltime->year = 2026; caltime->month = 10; caltime->day = 7;
    caltime->hour = 14; caltime->minute = 3; caltime->second = 12;
    return 0;
}

static void test_formatting(void)
{
    char text[64];
    time_format_local(1, text, sizeof(text));
    assert(strcmp(text, "2026-10-07 14:03:12") == 0);
    time_format_local(0, text, sizeof(text)); /* falls back to UTC */
    assert(strcmp(text, "1970-01-01 00:00:00 UTC") == 0);
    time_format_utc(1791381792ULL, text, sizeof(text));
    assert(strstr(text, " UTC") != NULL);
}

static void test_dump_and_repetition(void)
{
    char dump[1024];
    TimeSnapshot snapshot;
    reset();
    model.fail_flag_root = 1;
    model.fail_flag_command = 200;
    time_clock_dump(dump, sizeof(dump));
    assert(strstr(dump, "Network clock accuracy sufficient: rc=0x00000701 unavailable") != NULL);
    assert(strstr(dump, "Clock service handles released.") != NULL);
    assert_released();
    for (unsigned i = 0; i < 20; i++) {
        reset();
        time_clock_snapshot(&snapshot);
        time_clock_dump(dump, sizeof(dump));
        assert(model.writes == 0);
        assert_released();
    }
}

int main(void)
{
    test_snapshot_failures();
    test_automatic_gate(false);
    test_automatic_gate(true);
    test_apply_failures();
    test_readback_and_accuracy();
    test_read_only();
    test_dump_and_repetition();
    test_formatting();
    puts("time_ops lifecycle and read-only tests passed");
    return 0;
}
