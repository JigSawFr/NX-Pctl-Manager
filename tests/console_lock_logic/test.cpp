// Host tests for source/action/console_lock_logic.cpp: the limits saved when
// the lock is turned on and what turning it off puts back.
#include "check.h"
#include <cstdio>
#include <cstring>
#include <vector>

#include "action/console_lock_logic.hpp"

static PtState read_week()
{
    PtState pt;
    std::memset(&pt, 0, sizeof(pt));
    pt.session_valid = pt.fw_supported = pt.valid = true;
    const uint16_t week[7] = { 180, 60, 60, 60, 60, 120, PT_DAY_NOLIMIT };
    for (int i = 0; i < 7; i++) pt.day_min[i] = week[i];
    return pt;
}

static void test_save()
{
    PtState pt = read_week();
    const std::vector<int> saved = console_lock_logic::to_save(pt);
    CHECK((saved == std::vector<int>{ 180, 60, 60, 60, 60, 120, (int)PT_DAY_NOLIMIT }));
    // Unread: nothing to save, the lock is not turned on.
    pt.valid = false;
    CHECK(console_lock_logic::to_save(pt).empty());
}

static void all_cleared(const console_lock_logic::Unlock& u)
{
    CHECK(!u.restore);
    for (uint16_t d : u.days) CHECK(d == PT_DAY_NOLIMIT);
}

static void test_unlock()
{
    // Saved limits go back as they were, "no limit" days included.
    console_lock_logic::Unlock u = console_lock_logic::plan_unlock({ 180, 60, 60, 60, 60, 120, (int)PT_DAY_NOLIMIT });
    CHECK(u.restore);
    const uint16_t want[7] = { 180, 60, 60, 60, 60, 120, PT_DAY_NOLIMIT };
    for (int i = 0; i < 7; i++) CHECK(u.days[i] == want[i]);
    // A single limited day is enough; 0 is a limit too.
    u = console_lock_logic::plan_unlock({ 0, 65535, 65535, 65535, 65535, 65535, 65535 });
    CHECK(u.restore && u.days[0] == 0 && u.days[1] == PT_DAY_NOLIMIT);

    // Nothing saved, no limit on any day, or not a week: the limit is cleared.
    all_cleared(console_lock_logic::plan_unlock({}));
    all_cleared(console_lock_logic::plan_unlock(std::vector<int>(7, (int)PT_DAY_NOLIMIT)));
    all_cleared(console_lock_logic::plan_unlock({ 60, 60, 60, 60, 60, 60 }));
    all_cleared(console_lock_logic::plan_unlock({ 60, 60, 60, 60, 60, 60, 60, 60 }));

    // Turning on then off gives the week back.
    const PtState pt = read_week();
    u = console_lock_logic::plan_unlock(console_lock_logic::to_save(pt));
    CHECK(u.restore && std::memcmp(u.days, pt.day_min, sizeof(u.days)) == 0);
}

int main()
{
    test_save();
    test_unlock();
    return CHECK_DONE("console_lock_logic save and put-back assertions passed");
}
