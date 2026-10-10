// Host tests for source/util/pt_log.cpp: the recorder's CSV lines (a real
// reading, failed reads, the hex columns only when they change), the tail
// kept for a report and the file with its header and rotation.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <fcntl.h>
#include <unistd.h>

#include "util/paths.hpp"
#include "util/pt_log.hpp"

static int count(const std::string& s, char c)
{
    int n = 0;
    for (char x : s) n += x == c;
    return n;
}

// The 20:10 report of 2026-10-09 (22.0.0): Friday, 120 min, 1428 s left.
static PtSample reading()
{
    PtSample s;
    std::memset(&s, 0, sizeof(s));
    s.enabled = true;
    s.remaining_ns = 1428000000000ULL;
    s.spent_ns = 5772000000000ULL;
    s.display[0] = 2;
    std::memcpy(s.display + 0x10, &s.remaining_ns, 8);   // little-endian, as on the console
    static const uint8_t block[0x44] = {
        0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06,
        0x00, 0x01, 0xB4, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06,
        0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06,
        0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x01, 0x78, 0x00, 0x00, 0x00, 0x00, 0x06,
        0x00, 0x01, 0xB4, 0x00,
    };
    std::memcpy(s.block, block, sizeof(block));
    return s;
}

static void test_rows()
{
    const std::string head = pt_log::header();
    const int columns = count(head, ',') + 1;
    assert(columns == 16 && head.back() == '\n');
    assert(head.compare(head.size() - 7, 7, ",event\n") == 0);

    pt_log::Previous prev;
    const PtSample s = reading();
    const std::string first = pt_log::row("2026-10-09 20:10:52", 1791569454, s, &prev);
    assert(count(first, ',') + 1 == columns && first.back() == '\n');
    const std::string start = "2026-10-09 20:10:52,1791569454,0,1,0,0,1428,5772,7200,";
    assert(first.compare(0, start.size(), start) == 0);
    assert(first.find(",2,1428,off 00:00,0,02000000") != std::string::npos);
    assert(first.find(",010101000000000600") != std::string::npos);   // the block, as bytes
    assert(first.find("00C86E7B4C010000") != std::string::npos);      // 1459's remaining time at 0x10

    // Thirty seconds later: the hex columns are left empty when unchanged...
    PtSample later = s;
    later.remaining_ns -= 30000000000ULL;
    later.spent_ns += 30000000000ULL;
    std::string second = pt_log::row("2026-10-09 20:11:22", 1791569484, later, &prev);
    assert(count(second, ',') + 1 == columns);
    assert(second.find(",1398,5802,7200,2,1428,") != std::string::npos);   // 1459 not updated here
    assert(second.size() > 2 && second.compare(second.size() - 3, 3, ",,\n") == 0);
    // ... and written again when they change.
    std::memcpy(later.display + 0x10, &later.remaining_ns, 8);
    later.block[9] = 120;
    second = pt_log::row("2026-10-09 20:11:52", 1791569514, later, &prev);
    assert(second.find(",1398,off") != std::string::npos);
    assert(second.compare(second.size() - 3, 3, ",,\n") != 0 && count(second, ',') + 1 == columns);

    // Failed reads: "!" and the result, the sum left out, times not whole.
    PtSample bad = s;
    bad.spent_rc = 0x0001188E;
    bad.bedtime_rc = 0x0000F601;
    bad.remaining_ns = 1500000000ULL;   // 1.5 s
    const std::string third = pt_log::row("x", 1, bad, &prev);
    assert(third.find(",1.5,!0x0001188E,,") != std::string::npos);
    assert(third.find(",!0x0000F601,") != std::string::npos);

    // No session at all: every cell says why, the columns stay aligned.
    PtSample none;
    std::memset(&none, 0, sizeof(none));
    none.session_rc = none.unlocked_rc = none.enabled_rc = none.restricted_rc = none.alarm_off_rc =
        none.remaining_rc = none.block_rc = none.bedtime_rc = none.display_rc = none.spent_rc = none.extra_rc = 0x701;
    pt_log::Previous fresh;
    const std::string fourth = pt_log::row("y", 2, none, &fresh);
    assert(count(fourth, ',') + 1 == columns && fourth.find(",!0x00000701,!0x00000701,") != std::string::npos);
}

// The last column: why a line was written, quoted when CSV needs it.
static void test_events()
{
    pt_log::Previous prev;
    const PtSample s = reading();
    std::string line = pt_log::row("t", 1, s, &prev, "limits per_day [120 0 0 0 0 120 120] -> [180 0 0 0 0 120 180]");
    const std::string end = ",limits per_day [120 0 0 0 0 120 120] -> [180 0 0 0 0 120 180]\n";
    assert(line.size() > end.size() && line.compare(line.size() - end.size(), end.size(), end) == 0);
    line = pt_log::row("t", 2, s, &prev, "profile \"School, week\"");
    assert(line.find(",\"profile \"\"School, week\"\"\"\n") != std::string::npos);
    assert(count(pt_log::row("t", 3, s, &prev), ',') + 1 == 16);

    // The clock against the time that went by: the evening of 2026-10-09.
    pt_log::Clock c;
    assert(pt_log::clock_moved(&c, 1791569454, 1000).empty());           // first reading
    assert(pt_log::clock_moved(&c, 1791569484, 1030).empty());           // 30 s, 30 s
    assert(pt_log::clock_moved(&c, 1791569518, 1060).empty());           // 4 s off: within the slack
    assert(pt_log::clock_moved(&c, 1791570959, 1090) == "clock +1411 s vs elapsed");   // automatic correction on
    assert(pt_log::clock_moved(&c, 1791569602, 1120) == "clock -1387 s vs elapsed");   // NTP: back
    assert(c.known && c.posix == 1791569602 && c.steady_s == 1120);
}

static void test_tail()
{
    std::string text = pt_log::header();
    for (int i = 0; i < 1000; i++) text += "line " + std::to_string(i) + ",x\n";
    assert(pt_log::tail(text, text.size()) == text);
    const std::string t = pt_log::tail(text, 300);
    assert(t.size() <= 300 && t.compare(0, pt_log::header().size(), pt_log::header()) == 0);
    assert(t.size() > pt_log::header().size() && t[pt_log::header().size()] == 'l');   // a whole line
    assert(t.find("line 999,x\n") != std::string::npos);
    // Smaller than the header: the header alone.
    assert(pt_log::tail(text, 10) == pt_log::header());
}

static void test_file()
{
    std::string error;
    std::string text;
    assert(!paths::read_file(pt_log::path(), text));
    assert(pt_log::append("a\n", &error) && error.empty());
    assert(pt_log::append("b\n", &error));
    assert(paths::read_file(pt_log::path(), text) && text == pt_log::header() + "a\nb\n");

    // Over the limit: kept as the .old file, a new one with its header.
    {
        const int fd = ::open(pt_log::path().c_str(), O_WRONLY | O_APPEND);   // exists: not created here
        FILE* f = fd >= 0 ? ::fdopen(fd, "ab") : nullptr;
        assert(f);
        const std::string filler(pt_log::MAX_BYTES, 'z');
        assert(std::fwrite(filler.data(), 1, filler.size(), f) == filler.size());
        std::fclose(f);
    }
    assert(pt_log::append("c\n", &error));
    assert(paths::read_file(pt_log::path(), text) && text == pt_log::header() + "c\n");
    assert(paths::read_file(pt_log::old_path(), text) && text.size() > pt_log::MAX_BYTES);

    // A file of an earlier version (other columns) is put aside, not mixed.
    assert(paths::atomic_write(pt_log::path(), "local_time,posix,old\nx,1,2\n"));
    assert(pt_log::append("d\n", &error));
    assert(paths::read_file(pt_log::path(), text) && text == pt_log::header() + "d\n");
    assert(paths::read_file(pt_log::old_path(), text) && text == "local_time,posix,old\nx,1,2\n");
    // Ours is kept: the next line follows.
    assert(pt_log::append("e\n", &error));
    assert(paths::read_file(pt_log::path(), text) && text == pt_log::header() + "d\ne\n");
}

int main()
{
    char dir[] = "/tmp/playguard_pt_log_XXXXXX";
    assert(mkdtemp(dir) != nullptr);
    assert(chdir(dir) == 0);   // paths::data_dir() is ./playguard_data on the host

    test_rows();
    test_events();
    test_tail();
    test_file();

    const std::string cleanup = std::string("rm -rf '") + dir + "'";
    assert(std::system(cleanup.c_str()) == 0);
    std::puts("play-timer recorder lines, tail and file assertions passed");
    return 0;
}
