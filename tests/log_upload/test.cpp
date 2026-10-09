// Host tests for source/util/log_upload.cpp: which debug files and saved
// reports are offered, the bundle and its cut on a whole UTF-8 character,
// dpaste.org answers and the form sent, percent-encoding and the prefilled bug-report link.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

#include "util/log_upload.hpp"
#include "util/paths.hpp"

static void write(const std::string& path, const std::string& content)
{
    assert(paths::atomic_write(path, content));
}

static void test_files()
{
    assert(log_upload::debug_files().empty());
    assert(log_upload::saved_reports().empty());

    assert(paths::ensure_dir(paths::logs_dir()));
    write(paths::config_file(), "{\"pin_lock\": \"off\"}");
    write(paths::logs_dir() + "/play_timer_block.json", "{}");
    auto files = log_upload::debug_files();
    assert(files.size() == 2);
    assert(files[0].name == "logs/play_timer_block.json" && files[0].content == "{}");
    assert(files[1].name == "config.json");
    write(paths::history_file(), "[]");
    files = log_upload::debug_files();
    assert(files.size() == 3 && files[1].name == "history.json" && files[2].name == "config.json");

    write(paths::logs_dir() + "/20261008_090000.txt", "a");
    write(paths::logs_dir() + "/20261009_141203.txt", "b");
    write(paths::logs_dir() + "/20261009_141203_1.txt", "c");
    write(paths::logs_dir() + "/notes.txt", "not a report");
    write(paths::logs_dir() + "/2026100_x.txt", "not a report");
    auto reports = log_upload::saved_reports();
    assert(reports.size() == 3);
    assert(reports[0] == "20261009_141203_1.txt" && reports[1] == "20261009_141203.txt" &&
           reports[2] == "20261008_090000.txt");
    reports = log_upload::saved_reports(1);
    assert(reports.size() == 1 && reports[0] == "20261009_141203_1.txt");
}

static void test_bundle()
{
    bool cut = true;
    const std::string b = log_upload::bundle({ { "diagnostic report", "line\n" }, { "config.json", "{}" } }, 1000, &cut);
    assert(!cut);
    assert(b == "===== diagnostic report =====\nline\n\n===== config.json =====\n{}\n");
    assert(log_upload::bundle({}, 10, &cut).empty() && !cut);

    // Too large: cut, a notice at the end, never more than the limit.
    const std::string big(5000, 'x');
    const std::string c = log_upload::bundle({ { "big", big } }, 200, &cut);
    assert(cut && c.size() <= 200);
    assert(c.find("(cut: too large to send whole)") != std::string::npos);
    assert(c.compare(0, 15, "===== big =====") == 0);

    // A cut never splits a character: "é" is two bytes.
    std::string accents;
    for (int i = 0; i < 200; i++) accents += "\xC3\xA9";
    for (size_t limit = 60; limit < 120; limit++) {
        const std::string d = log_upload::bundle({ { "fr", accents } }, limit, &cut);
        assert(cut && d.size() <= limit);
        const size_t body = d.find('\n') + 1;
        const size_t end = d.find("\n\n(cut");
        if (end == std::string::npos) continue;   // the notice itself was cut: nothing to check
        assert((end - body) % 2 == 0);
    }
    // A limit smaller than the notice still holds.
    assert(log_upload::bundle({ { "big", big } }, 10, &cut).size() <= 10 && cut);
}

static void test_reply()
{
    std::string url = "kept";
    assert(log_upload::parse_reply("https://dpaste.org/AbC1\n", &url) && url == "https://dpaste.org/AbC1");
    assert(log_upload::parse_reply("  https://dpaste.org/x9  ", &url) && url == "https://dpaste.org/x9");
    url = "kept";
    const char* bad[] = { "", "https://dpaste.org/", "http://dpaste.org/AbC1", "https://evil.example/AbC1",
                          "https://dpaste.org.evil.example/AbC1", "https://dpaste.org/Ab/C1",
                          "https://dpaste.org/AbC1.txt", "https://dpaste.org/Ab C1", "<html>error</html>",
                          "\"https://dpaste.org/AbC1\"", "https://dpaste.org/AbC1?x=1" };
    for (const char* b : bad) {
        assert(!log_upload::parse_reply(b, &url));
        assert(url == "kept");
    }
}

static void test_form()
{
    assert(log_upload::form("a b&c=\n") ==
           "content=a%20b%26c%3D%0A&format=url&lexer=_text&expires=" + std::to_string(30L * 24 * 3600));
}

static void test_urls()
{
    assert(log_upload::url_encode("aZ09-_.~") == "aZ09-_.~");
    assert(log_upload::url_encode("https://dpaste.org/A b") == "https%3A%2F%2Fdpaste.org%2FA%20b");
    assert(log_upload::url_encode("\xC3\xA8") == "%C3%A8");
    assert(log_upload::issue_url("https://github.com/o/r", "https://dpaste.org/AbC1", "1.2.0", "23.0.1", "1.12.0") ==
           "https://github.com/o/r/issues/new?template=1-bug.yml&report=https%3A%2F%2Fdpaste.org%2FAbC1"
           "&version=1.2.0&firmware=23.0.1&atmosphere=1.12.0");
    assert(log_upload::issue_url("https://github.com/o/r", "https://dpaste.org/X", "", "", "") ==
           "https://github.com/o/r/issues/new?template=1-bug.yml&report=https%3A%2F%2Fdpaste.org%2FX");
    assert(log_upload::short_url("https://dpaste.org/AbC1") == "dpaste.org/AbC1");
    assert(log_upload::short_url("dpaste.org/AbC1") == "dpaste.org/AbC1");
}

int main()
{
    char dir[] = "/tmp/playguard_log_upload_XXXXXX";
    assert(mkdtemp(dir) != nullptr);
    assert(chdir(dir) == 0);   // paths::data_dir() is ./playguard_data on the host

    test_files();
    test_bundle();
    test_reply();
    test_form();
    test_urls();

    std::system((std::string("rm -rf '") + dir + "'").c_str());
    std::puts("log upload files, bundle, dpaste.org answer and form, bug-report link assertions passed");
    return 0;
}
