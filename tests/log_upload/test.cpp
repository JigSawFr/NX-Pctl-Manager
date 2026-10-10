// Host tests for source/util/log_upload.cpp: which debug files and saved
// reports are offered, the bundle and its cut on a whole UTF-8 character,
// bpa.st and GitHub answers, the bodies sent, percent-encoding and the prefilled bug-report link.
#include "check.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

#include "util/log_upload.hpp"
#include "util/paths.hpp"
#include "util/pt_log.hpp"

static void write(const std::string& path, const std::string& content)
{
    CHECK(paths::atomic_write(path, content));
}

static void test_files()
{
    CHECK(log_upload::debug_files().empty());
    CHECK(log_upload::saved_reports().empty());

    CHECK(paths::ensure_dir(paths::logs_dir()));
    write(paths::config_file(), "{\"pin_lock\": \"off\"}");
    write(paths::logs_dir() + "/play_timer_block.json", "{}");
    auto files = log_upload::debug_files();
    CHECK(files.size() == 2);
    CHECK(files[0].name == "logs/play_timer_block.json" && files[0].content == "{}");
    CHECK(files[1].name == "config.json");
    write(paths::history_file(), "[]");
    files = log_upload::debug_files();
    CHECK(files.size() == 3 && files[1].name == "history.json" && files[2].name == "config.json");
    // The GitHub token (util/github_auth.hpp) is never sent.
    write(paths::data_dir() + "/github_token", "ghu_secret");
    files = log_upload::debug_files();
    CHECK(files.size() == 3);
    for (const auto& f : files) CHECK(f.content.find("ghu_secret") == std::string::npos);

    // The recorder's file, after the block: only its end, header kept.
    std::string csv = pt_log::header();
    for (int i = 0; i < 5000; i++) csv += "2026-10-09 20:10:52,1791569454,0,1,0,0,1428,5772,7200,,\n";
    write(pt_log::path(), csv);
    files = log_upload::debug_files();
    CHECK(files.size() == 4 && files[1].name == "logs/play_timer_log.csv" && files[2].name == "history.json");
    CHECK(files[1].content.size() <= log_upload::PT_LOG_BYTES && files[1].content.size() > 40000);
    CHECK(files[1].content.compare(0, pt_log::header().size(), pt_log::header()) == 0);
    CHECK(std::remove(pt_log::path().c_str()) == 0);

    write(paths::logs_dir() + "/20261008_090000.txt", "a");
    write(paths::logs_dir() + "/20261009_141203.txt", "b");
    write(paths::logs_dir() + "/20261009_141203_1.txt", "c");
    write(paths::logs_dir() + "/notes.txt", "not a report");
    write(paths::logs_dir() + "/2026100_x.txt", "not a report");
    auto reports = log_upload::saved_reports();
    CHECK(reports.size() == 3);
    CHECK(reports[0] == "20261009_141203_1.txt" && reports[1] == "20261009_141203.txt" &&
           reports[2] == "20261008_090000.txt");
    reports = log_upload::saved_reports(1);
    CHECK(reports.size() == 1 && reports[0] == "20261009_141203_1.txt");
}

static void test_bundle()
{
    bool cut = true;
    const std::string b = log_upload::bundle({ { "diagnostic report", "line\n" }, { "config.json", "{}" } }, 1000, &cut);
    CHECK(!cut);
    CHECK(b == "===== diagnostic report =====\nline\n\n===== config.json =====\n{}\n");
    CHECK(log_upload::bundle({}, 10, &cut).empty() && !cut);

    // Too large: cut, a notice at the end, never more than the limit.
    const std::string big(5000, 'x');
    const std::string c = log_upload::bundle({ { "big", big } }, 200, &cut);
    CHECK(cut && c.size() <= 200);
    CHECK(c.find("(cut: too large to send whole)") != std::string::npos);
    CHECK(c.compare(0, 15, "===== big =====") == 0);

    // A cut never splits a character: "é" is two bytes.
    std::string accents;
    for (int i = 0; i < 200; i++) accents += "\xC3\xA9";
    for (size_t limit = 60; limit < 120; limit++) {
        const std::string d = log_upload::bundle({ { "fr", accents } }, limit, &cut);
        CHECK(cut && d.size() <= limit);
        const size_t body = d.find('\n') + 1;
        const size_t end = d.find("\n\n(cut");
        if (end == std::string::npos) continue;   // the notice itself was cut: nothing to check
        CHECK((end - body) % 2 == 0);
    }
    // A limit smaller than the notice still holds.
    CHECK(log_upload::bundle({ { "big", big } }, 10, &cut).size() <= 10 && cut);
}

static void test_reply()
{
    std::string url = "kept", removal = "kept";
    const std::string ok = "Paste URL:   https://bpa.st/ABCD\nRaw URL:     https://bpa.st/raw/ABCD\n"
                           "Removal URL: https://bpa.st/remove/EFGH2\n";
    CHECK(log_upload::parse_bpaste(ok, &url, &removal));
    CHECK(url == "https://bpa.st/ABCD" && removal == "https://bpa.st/remove/EFGH2");
    url = removal = "kept";
    const char* bad[] = { "", "<html>error</html>", "Invalid `raw` supplied.\n",
                          "Enhance your calm, you have exceeded the ratelimit.",
                          "Paste URL:   https://bpa.st/ABCD\n",   // no removal link
                          "Paste URL:   http://bpa.st/ABCD\nRemoval URL: https://bpa.st/remove/EFGH\n",
                          "Paste URL:   https://bpa.st.evil.example/ABCD\nRemoval URL: https://bpa.st/remove/EFGH\n",
                          "Paste URL:   https://bpa.st/AB/CD\nRemoval URL: https://bpa.st/remove/EFGH\n",
                          "Paste URL:   https://bpa.st/ABCD?x\nRemoval URL: https://bpa.st/remove/EFGH\n",
                          "Paste URL:   https://bpa.st/ABCD\nRemoval URL: https://evil.example/remove/EFGH\n",
                          "x Paste URL: https://bpa.st/ABCD\nRemoval URL: https://bpa.st/remove/EFGH\n" };
    for (const char* b : bad) {
        CHECK(!log_upload::parse_bpaste(b, &url, &removal));
        CHECK(url == "kept" && removal == "kept");
    }

    CHECK(log_upload::parse_gist(R"({"id": "aa5a", "html_url": "https://gist.github.com/aa5a315d61ae9438b18d"})",
                                  &url));
    CHECK(url == "https://gist.github.com/aa5a315d61ae9438b18d");
    CHECK(log_upload::parse_gist(R"({"html_url": "https://gist.github.com/octo-cat/aa5a"})", &url) &&
           url == "https://gist.github.com/octo-cat/aa5a");
    url = "kept";
    const char* bad_gist[] = { "", "not json", "[]", R"({"message": "Not Found"})", R"({"html_url": 5})",
                               R"({"html_url": "https://gist.github.com/"})",
                               R"({"html_url": "http://gist.github.com/aa5a"})",
                               R"({"html_url": "https://gist.github.com.evil.example/aa5a"})",
                               R"({"html_url": "https://gist.github.com//aa5a"})",
                               R"({"html_url": "https://gist.github.com/aa5a/"})",
                               R"({"html_url": "https://gist.github.com/aa5a?x=1"})" };
    for (const char* b : bad_gist) {
        CHECK(!log_upload::parse_gist(b, &url));
        CHECK(url == "kept");
    }
}

static void test_bodies()
{
    std::string type;
    const std::string f = log_upload::form("a b&c=\n{\"é\": 1}", &type);
    CHECK(type == "multipart/form-data; boundary=PlayGuardBoundary");
    CHECK(f == "--PlayGuardBoundary\r\nContent-Disposition: form-data; name=\"raw\"\r\n\r\na b&c=\n{\"é\": 1}\r\n"
                "--PlayGuardBoundary\r\nContent-Disposition: form-data; name=\"lexer\"\r\n\r\ntext\r\n"
                "--PlayGuardBoundary\r\nContent-Disposition: form-data; name=\"expiry\"\r\n\r\n1month\r\n"
                "--PlayGuardBoundary--\r\n");
    // A text holding the boundary gets another one.
    log_upload::form("x --PlayGuardBoundary PlayGuardBoundary0", &type);
    CHECK(type == "multipart/form-data; boundary=PlayGuardBoundary1");

    CHECK(log_upload::gist_body("a \"b\"\n\xC3\xA9", "1.2.0") ==
           R"({"description":"PlayGuard 1.2.0 diagnostic report","files":{"playguard-report.txt":{"content":"a \"b\"\n)"
           "\xC3\xA9"
           R"("}},"public":false})");

    CHECK(log_upload::max_bytes(log_upload::Host::Bpaste) == 128 * 1024);
    CHECK(log_upload::max_bytes(log_upload::Host::Gist) == 512 * 1024);
}

static void test_urls()
{
    CHECK(log_upload::url_encode("aZ09-_.~") == "aZ09-_.~");
    CHECK(log_upload::url_encode("https://bpa.st/A b") == "https%3A%2F%2Fbpa.st%2FA%20b");
    CHECK(log_upload::url_encode("\xC3\xA8") == "%C3%A8");
    CHECK(log_upload::issue_url("https://github.com/o/r", "https://bpa.st/ABCD", "1.2.0", "23.0.1", "1.12.0") ==
           "https://github.com/o/r/issues/new?template=1-bug.yml&report=https%3A%2F%2Fbpa.st%2FABCD"
           "&version=1.2.0&firmware=23.0.1&atmosphere=1.12.0");
    CHECK(log_upload::issue_url("https://github.com/o/r", "https://gist.github.com/aa5a", "", "", "") ==
           "https://github.com/o/r/issues/new?template=1-bug.yml&report=https%3A%2F%2Fgist.github.com%2Faa5a");
    CHECK(log_upload::short_url("https://bpa.st/ABCD") == "bpa.st/ABCD");
    CHECK(log_upload::short_url("bpa.st/ABCD") == "bpa.st/ABCD");
}

int main()
{
    char dir[] = "/tmp/playguard_log_upload_XXXXXX";
    REQUIRE(mkdtemp(dir) != nullptr);
    REQUIRE(chdir(dir) == 0);   // paths::data_dir() is ./playguard_data on the host

    test_files();
    test_bundle();
    test_reply();
    test_bodies();
    test_urls();

    std::system((std::string("rm -rf '") + dir + "'").c_str());
    return CHECK_DONE("log upload files, bundle, bpa.st and gist answers and bodies, bug-report link assertions passed");
}
