// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "sync_entities.h"

#include <string.h>

// Discovery abbreviations (HA): unit_of_meas, dev_cla, stat_cla, ent_cat,
// pl_on / pl_off (binary_sensor, switch commands), stat_on / stat_off
// (switch state), ops (select options), en (enabled by default), ic (icon).
#define E_MINUTES "\"unit_of_meas\":\"min\",\"dev_cla\":\"duration\""
#define E_MEASURE E_MINUTES ",\"stat_cla\":\"measurement\""
#define E_BINARY "\"pl_on\":\"True\",\"pl_off\":\"False\""
#define E_SWITCH "\"stat_on\":\"True\",\"stat_off\":\"False\",\"pl_on\":\"ON\",\"pl_off\":\"OFF\""
#define E_LIMIT "\"min\":0,\"max\":1440,\"step\":5,\"mode\":\"box\"," E_MINUTES ",\"ent_cat\":\"config\""
#define E_DIAG "\"ent_cat\":\"diagnostic\""
#define E_CONFIG "\"ent_cat\":\"config\""

static const SyncEntity ENTITIES[] = {
    // binary sensors
    { "parental_controls_enabled", "binary_sensor", SyncWrite_None, "Parental controls",
      "{{ value_json.controls.enabled }}", E_BINARY ",\"ic\":\"mdi:shield-account\"" },
    { "temp_unlocked", "binary_sensor", SyncWrite_None, "Temporarily unlocked",
      "{{ value_json.controls.temp_unlocked }}", E_BINARY ",\"dev_cla\":\"lock\"" },
    { "pin_set", "binary_sensor", SyncWrite_None, "PIN set", "{{ value_json.controls.pin_set }}", E_BINARY "," E_DIAG },
    { "companion_linked", "binary_sensor", SyncWrite_None, "Companion app linked",
      "{{ value_json.controls.companion_linked }}", E_BINARY ",\"ic\":\"mdi:cellphone-link\"" },
    { "timer_enabled", "binary_sensor", SyncWrite_None, "Play timer", "{{ value_json.timer.enabled }}",
      E_BINARY ",\"ic\":\"mdi:timer-outline\"" },
    { "limit_reached", "binary_sensor", SyncWrite_None, "Time is up", "{{ value_json.timer.limit_reached }}",
      E_BINARY ",\"ic\":\"mdi:timer-alert-outline\"" },
    { "clock_accurate", "binary_sensor", SyncWrite_None, "Network clock accurate",
      "{{ value_json.clock_accurate }}", E_BINARY "," E_DIAG },

    // sensors
    { "used_screen_time", "sensor", SyncWrite_None, "Used screen time", "{{ value_json.timer.used_min }}", E_MEASURE },
    { "used_screen_time_log", "sensor", SyncWrite_None, "Used screen time (play log)",
      "{{ value_json.activity_today.used_min }}", E_MEASURE },
    { "screen_time_remaining", "sensor", SyncWrite_None, "Screen time remaining",
      "{{ value_json.timer.remaining_min }}", E_MEASURE },
    { "extended_screen_time", "sensor", SyncWrite_None, "Extended screen time",
      "{{ value_json.timer.extended_today_min }}", E_MINUTES },
    { "now_playing", "sensor", SyncWrite_None, "Now playing",
      "{{ value_json.activity_today.now_playing.name or value_json.activity_today.now_playing.app_id }}",
      "\"ic\":\"mdi:gamepad-variant\"" },
    { "rating_age", "sensor", SyncWrite_None, "Age rating", "{{ value_json.controls.rating_age }}", E_DIAG },
    { "firmware", "sensor", SyncWrite_None, "Firmware", "{{ value_json.console.firmware }}", E_DIAG },
    { "atmosphere", "sensor", SyncWrite_None, "Atmosphère", "{{ value_json.console.atmosphere }}", E_DIAG },
    { "spent_raw", "sensor", SyncWrite_None, "Time spent (1952)", "{{ value_json.timer.spent_raw_min }}",
      E_MINUTES "," E_DIAG ",\"en\":false" },
    { "last_order", "sensor", SyncWrite_None, "Last order", "{{ value_json.link.last_result }}", E_DIAG },

    // limits
    { "limit_sun", "number", SyncWrite_Timer, "Limit Sunday", "{{ value_json.timer.limits.sun }}", E_LIMIT },
    { "limit_mon", "number", SyncWrite_Timer, "Limit Monday", "{{ value_json.timer.limits.mon }}", E_LIMIT },
    { "limit_tue", "number", SyncWrite_Timer, "Limit Tuesday", "{{ value_json.timer.limits.tue }}", E_LIMIT },
    { "limit_wed", "number", SyncWrite_Timer, "Limit Wednesday", "{{ value_json.timer.limits.wed }}", E_LIMIT },
    { "limit_thu", "number", SyncWrite_Timer, "Limit Thursday", "{{ value_json.timer.limits.thu }}", E_LIMIT },
    { "limit_fri", "number", SyncWrite_Timer, "Limit Friday", "{{ value_json.timer.limits.fri }}", E_LIMIT },
    { "limit_sat", "number", SyncWrite_Timer, "Limit Saturday", "{{ value_json.timer.limits.sat }}", E_LIMIT },
    { "limit_uniform", "number", SyncWrite_Timer, "Limit every day", "{{ value_json.timer.uniform_min }}", E_LIMIT },
    { "max_screentime_today", "number", SyncWrite_Timer, "Max screentime today",
      "{{ value_json.timer.limit_today_min }}", E_LIMIT },

    // bedtime
    { "bedtime_alarm", "time", SyncWrite_Timer, "Bedtime alarm", "{{ value_json.timer.bedtime.start }}", E_CONFIG },
    { "bedtime_end_time", "time", SyncWrite_Timer, "Bedtime end time", "{{ value_json.timer.bedtime.end }}", E_CONFIG },
    { "bedtime_enabled", "switch", SyncWrite_Timer, "Bedtime alarm enabled",
      "{{ value_json.timer.bedtime.enabled }}", E_SWITCH "," E_CONFIG },

    // switches
    { "console_lock", "switch", SyncWrite_Timer, "Console lock", "{{ value_json.timer.console_locked }}",
      E_SWITCH ",\"ic\":\"mdi:lock\"" },
    { "play_timer_alarm", "switch", SyncWrite_Timer, "Time's up alarm", "{{ value_json.timer.alarm_on }}",
      E_SWITCH "," E_CONFIG },
    { "unlocked", "switch", SyncWrite_Timer, "Unlocked", "{{ value_json.controls.temp_unlocked }}",
      E_SWITCH ",\"ic\":\"mdi:lock-open-variant\"" },
    { "vr_restricted", "switch", SyncWrite_Free, "VR mode restricted", "{{ value_json.controls.vr_restricted }}",
      E_SWITCH "," E_CONFIG },
    { "sns_post_restricted", "switch", SyncWrite_Free, "Posting to social media restricted",
      "{{ value_json.controls.sns_post_restricted }}", E_SWITCH "," E_CONFIG },
    { "free_communication_restricted", "switch", SyncWrite_Free, "Communication restricted",
      "{{ value_json.controls.free_communication_restricted }}", E_SWITCH "," E_CONFIG },

    // selects (the profile's options are added when the discovery is built)
    { "restriction_level", "select", SyncWrite_Free, "Restriction level", "{{ value_json.controls.level }}",
      "\"ops\":[\"none\",\"young_child\",\"child\",\"teen\",\"custom\"]," E_CONFIG },
    { "profile", "select", SyncWrite_Timer, "Profile", "{{ None }}", E_CONFIG },

    // buttons
    { "sync_now", "button", SyncWrite_Free, "Sync now", NULL, "\"ic\":\"mdi:sync\"" },
    { "lock_now", "button", SyncWrite_Free, "Lock now", NULL, "\"ic\":\"mdi:lock\"" },
    { "export_report", "button", SyncWrite_Free, "Export a diagnostic report", NULL, E_DIAG },
    { "add_bonus_time_15", "button", SyncWrite_Timer, "Extra time +15 min", NULL, "\"ic\":\"mdi:timer-plus-outline\"" },
    { "add_bonus_time_30", "button", SyncWrite_Timer, "Extra time +30 min", NULL, "\"ic\":\"mdi:timer-plus-outline\"" },
    { "add_bonus_time_60", "button", SyncWrite_Timer, "Extra time +1 h", NULL, "\"ic\":\"mdi:timer-plus-outline\"" },
    { "stop_today", "button", SyncWrite_Timer, "No more play today", NULL, "\"ic\":\"mdi:timer-off-outline\"" },
    { "remove_limit", "button", SyncWrite_Timer, "Remove the limit", NULL, "\"ic\":\"mdi:timer-remove-outline\"" },

    // events (state topic and event types set when the discovery is built)
    { "events", "event", SyncWrite_None, "Events", NULL, "" },
};

const SyncEntity *sync_entities(size_t *count)
{
    *count = sizeof(ENTITIES) / sizeof(ENTITIES[0]);
    return ENTITIES;
}

const SyncEntity *sync_entity_find(const char *id)
{
    for (size_t i = 0; i < sizeof(ENTITIES) / sizeof(ENTITIES[0]); i++)
        if (!strcmp(ENTITIES[i].id, id)) return &ENTITIES[i];
    return NULL;
}
