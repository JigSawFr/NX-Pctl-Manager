// Host tests for source/action/pin_lock_logic.cpp: the change check (modes,
// the 5-minute grace after the PIN, the 3-second hold after a refusal) and
// what moving between the modes needs.
#include <cassert>
#include <cstdio>
#include <string>

#include "action/pin_lock_logic.hpp"

using pin_lock_logic::Change;
using pin_lock_logic::Check;
using pin_lock_logic::Clock;
using std::chrono::minutes;
using std::chrono::seconds;

// A moment well after the clock's epoch, as on a console that has been on a while.
static const Clock::time_point T0 = Clock::time_point{} + std::chrono::hours(10);

static void test_rank()
{
    assert(pin_lock_logic::rank("off") == 0);
    assert(pin_lock_logic::rank("changes") == 1);
    assert(pin_lock_logic::rank("open") == 2);
    assert(pin_lock_logic::rank("") == 0);           // unknown: off
    assert(pin_lock_logic::rank("Changes") == 0);
    for (int i = 0; i < 3; i++) assert(pin_lock_logic::rank(pin_lock_logic::MODES[i]) == i);
}

static void test_check()
{
    const Clock::time_point never{};
    // Only "changes" asks: "open" asks once at the start, not per change.
    assert(pin_lock_logic::check("off", T0, never, never) == Check::Allow);
    assert(pin_lock_logic::check("open", T0, never, never) == Check::Allow);
    assert(pin_lock_logic::check("bogus", T0, never, never) == Check::Allow);
    assert(pin_lock_logic::check("changes", T0, never, never) == Check::Ask);
    // Even with a refusal held: other modes never refuse.
    assert(pin_lock_logic::check("off", T0, never, T0 + seconds(1)) == Check::Allow);

    // The PIN entered at T0: no asking for 5 minutes, then again.
    const Clock::time_point confirmed = T0 + pin_lock_logic::GRACE;
    assert(pin_lock_logic::check("changes", T0, confirmed, never) == Check::Allow);
    assert(pin_lock_logic::check("changes", T0 + minutes(4) + seconds(59), confirmed, never) == Check::Allow);
    assert(pin_lock_logic::check("changes", T0 + minutes(5), confirmed, never) == Check::Ask);
    assert(pin_lock_logic::check("changes", T0 + minutes(6), confirmed, never) == Check::Ask);

    // Refused at T0: the next changes of the same action are refused without
    // asking, for 3 seconds.
    const Clock::time_point refused = T0 + pin_lock_logic::REFUSAL_HOLDS;
    assert(pin_lock_logic::check("changes", T0, never, refused) == Check::Refuse);
    assert(pin_lock_logic::check("changes", T0 + seconds(2), never, refused) == Check::Refuse);
    assert(pin_lock_logic::check("changes", T0 + seconds(3), never, refused) == Check::Ask);

    // A grace still running wins over a refusal (the PIN was entered since).
    assert(pin_lock_logic::check("changes", T0, confirmed, refused) == Check::Allow);
}

static void test_change()
{
    // Turning it on needs a PIN on the console.
    assert(pin_lock_logic::change("off", 1, false) == Change::NoPin);
    assert(pin_lock_logic::change("off", 2, false) == Change::NoPin);
    assert(pin_lock_logic::change("changes", 2, false) == Change::NoPin);
    assert(pin_lock_logic::change("open", 1, false) == Change::NoPin);   // down, but still on
    // More protection: no PIN asked.
    assert(pin_lock_logic::change("off", 1, true) == Change::Allowed);
    assert(pin_lock_logic::change("off", 2, true) == Change::Allowed);
    assert(pin_lock_logic::change("changes", 2, true) == Change::Allowed);
    // Less protection: the PIN first.
    assert(pin_lock_logic::change("open", 1, true) == Change::AskPin);
    assert(pin_lock_logic::change("open", 0, true) == Change::AskPin);
    assert(pin_lock_logic::change("changes", 0, true) == Change::AskPin);
    // Turning it off with no PIN left on the console: nothing to ask.
    assert(pin_lock_logic::change("changes", 0, false) == Change::Allowed);
    assert(pin_lock_logic::change("open", 0, false) == Change::Allowed);
    // An unknown setting counts as off.
    assert(pin_lock_logic::change("bogus", 0, true) == Change::Allowed);
    assert(pin_lock_logic::change("bogus", 1, true) == Change::Allowed);
}

int main()
{
    test_rank();
    test_check();
    test_change();
    std::puts("pin_lock_logic mode, grace, refusal hold and mode-change assertions passed");
    return 0;
}
