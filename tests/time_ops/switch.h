#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t s32;
typedef int64_t s64;
typedef u32 Result;
typedef struct { unsigned handle; } Service;
typedef struct {
    unsigned out_num_objects;
    Service *out_objects;
} SfDispatchParams;

#define R_SUCCEEDED(rc) ((rc) == 0)
#define R_FAILED(rc) ((rc) != 0)

typedef struct { u16 year; u8 month, day, hour, minute, second, pad; } TimeCalendarTime;
typedef struct { u32 wday, yday; char timezoneName[8]; u32 DST; int32_t offset; } TimeCalendarAdditionalInfo;
Result timeToCalendarTimeWithMyRule(u64 timestamp, TimeCalendarTime *caltime, TimeCalendarAdditionalInfo *info);
Result timeToPosixTimeWithMyRule(const TimeCalendarTime *caltime, u64 *timestamp_list, s32 timestamp_list_count, s32 *timestamp_count);
typedef enum { TimeType_UserSystemClock, TimeType_NetworkSystemClock, TimeType_LocalSystemClock } TimeType;
Result timeGetCurrentTime(TimeType type, u64 *timestamp);
typedef struct { u8 uuid[0x10]; } Uuid;
typedef struct { s64 time_point; Uuid source_id; } TimeSteadyClockTimePoint;
Result timeGetStandardSteadyClockTimePoint(TimeSteadyClockTimePoint *out);

Result smGetService(Service *service, const char *name);
void serviceClose(Service *service);
Result mock_dispatch(Service *service, u32 command, void *out, size_t out_size,
                     const void *in, size_t in_size, SfDispatchParams params);

/* The fake models handle ownership and commands, not Horizon IPC encoding. */
#define serviceDispatch(service, command, ...) \
    mock_dispatch((service), (command), NULL, 0, NULL, 0, \
                  (SfDispatchParams){ __VA_ARGS__ })
#define serviceDispatchOut(service, command, value, ...) \
    mock_dispatch((service), (command), &(value), sizeof(value), NULL, 0, \
                  (SfDispatchParams){ __VA_ARGS__ })
#define serviceDispatchIn(service, command, value, ...) \
    mock_dispatch((service), (command), NULL, 0, &(value), sizeof(value), \
                  (SfDispatchParams){ __VA_ARGS__ })
