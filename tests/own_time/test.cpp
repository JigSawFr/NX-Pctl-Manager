// Host tests for the file of util/own_time.hpp: PlayGuard's own sessions.
#include <cassert>
#include <cstdio>

#include "util/own_time.hpp"

int main()
{
    // Round trip.
    const std::vector<PlayLogSpan> spans = { { 1000, 1600 }, { 5000, 5030 } };
    const std::string text = own_time::format(spans);
    assert(text == "1000 1600\n5000 5030\n");
    auto back = own_time::parse(text, 0);
    assert(back.size() == 2 && back[1].start == 5000 && back[1].end == 5030);

    // Old sessions dropped; broken lines skipped, not fatal.
    back = own_time::parse("1000 1600\n\ngarbage\n3000 2000\n4000 4000\n9 9999999\n5000 5030\n", 2000);
    assert(back.size() == 1 && back[0].start == 5000);

    // An empty session is not written.
    assert(own_time::format({ { 7, 7 } }).empty());
    puts("own_time file assertions passed");
    return 0;
}
