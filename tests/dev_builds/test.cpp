// Host tests for source/util/sha256.cpp, dev_builds.cpp, zip_read.cpp and
// github_auth.cpp: the SHA-256 test vectors, digests, the latest release, the
// artifact and pull-request lists and how they combine (main's commits, each
// open pull request's newest build, forks included), the download checks
// (size, SHA-256, NRO header), zip extraction (deflated, stored, a changed
// byte, a cut archive), the replacement of the running file, and the
// device-flow answers and the token file.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

#include "util/dev_builds.hpp"
#include "util/paths.hpp"
#include "util/github_auth.hpp"
#include "util/sha256.hpp"
#include "util/zip_read.hpp"

using dev_builds::Build;
using dev_builds::Kind;

static std::string sha(const std::string& s)
{
    Sha256 h;
    h.update(s.data(), s.size());
    return h.hex();
}

static void test_sha256()
{
    assert(sha("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    assert(sha("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    assert(sha("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
           "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    // A million 'a', fed in uneven pieces: crosses many block boundaries.
    Sha256 h;
    const std::string chunk(997, 'a');
    size_t left = 1000000;
    while (left > 0) {
        const size_t n = std::min(left, chunk.size());
        h.update(chunk.data(), n);
        left -= n;
    }
    assert(h.hex() == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    // 55, 56 and 64 bytes: the padding edge cases.
    assert(sha(std::string(55, 'x')).size() == 64);
    assert(sha(std::string(56, 'a')) == "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a");
    assert(sha(std::string(64, 'a')) == "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb");
}

static void test_names()
{
    const std::string hex(64, 'a');
    assert(dev_builds::digest_hex("sha256:" + hex) == hex);
    assert(dev_builds::digest_hex("sha256:" + std::string(64, 'A')) == hex);
    assert(dev_builds::digest_hex("sha512:" + hex).empty());
    assert(dev_builds::digest_hex("sha256:" + hex.substr(1)).empty());
    assert(dev_builds::digest_hex("sha256:" + hex.substr(1) + "z").empty());
    assert(dev_builds::digest_hex("").empty());

    std::string head(0x20, '\0');
    assert(!dev_builds::is_nro(head));
    head.replace(0x10, 4, "NRO0");
    assert(dev_builds::is_nro(head));
    assert(!dev_builds::is_nro(head.substr(0, 0x12)));
}

static void test_release()
{
    Build b;
    const std::string rel = R"({"tag_name": "v1.2.0", "prerelease": false, "draft": false, "assets": [
        {"name": "playguard.zip", "size": 9, "updated_at": "x", "browser_download_url": "https://x/z"},
        {"name": "playguard.nro", "size": 1000, "updated_at": "2026-10-01T00:00:00Z",
         "browser_download_url": "https://github.com/o/r/releases/download/v1.2.0/playguard.nro",
         "digest": "sha256:)" + std::string(64, 'c') + R"("}]})";
    assert(dev_builds::parse_release(rel, &b));
    assert(b.kind == Kind::Release && !b.artifact && b.version == "1.2.0" && b.size == 1000 &&
           b.sha256 == std::string(64, 'c') && b.url.find("playguard.nro") != std::string::npos);
    const char* bad[] = {
        R"({"tag_name": "v1", "prerelease": true, "draft": false, "assets": [{"name": "playguard.nro", "size": 1, "browser_download_url": "https://x"}]})",
        R"({"tag_name": "v1", "prerelease": false, "draft": true, "assets": [{"name": "playguard.nro", "size": 1, "browser_download_url": "https://x"}]})",
        R"({"tag_name": "v1", "prerelease": false, "draft": false, "assets": [{"name": "playguard.zip", "size": 1, "browser_download_url": "https://x"}]})",
        R"({"tag_name": "v1", "prerelease": false, "draft": false, "assets": [{"name": "playguard.nro", "size": 0, "browser_download_url": "https://x"}]})",
        R"({"tag_name": "v1", "prerelease": false, "draft": false, "assets": [{"name": "playguard.nro", "size": 1, "browser_download_url": "http://x"}]})",
        R"({"message": "Not Found"})", "[]", "not json", "",
    };
    for (const char* j : bad) assert(!dev_builds::parse_release(j, &b));
}

static std::string artifact(const std::string& branch, const std::string& sha, const std::string& date,
                            int64_t head_repo = 1, bool expired = false, const std::string& name = "playguard_release")
{
    return R"({"name": ")" + name + R"(", "size_in_bytes": 4000, "expired": )" + (expired ? "true" : "false") +
           R"(, "created_at": ")" + date + R"(", "digest": "sha256:)" + std::string(64, 'd') +
           R"(", "archive_download_url": "https://api.github.com/repos/o/r/actions/artifacts/)" + sha.substr(0, 3) +
           R"(/zip", "workflow_run": {"id": 9, "repository_id": 1, "head_repository_id": )" +
           std::to_string(head_repo) + R"(, "head_branch": ")" + branch + R"(", "head_sha": ")" + sha + R"("}})";
}

static void test_artifacts()
{
    const std::string a = "aaaaaaa" + std::string(33, '0'), b = "bbbbbbb" + std::string(33, '0'),
                      c = "ccccccc" + std::string(33, '0'), d = "ddddddd" + std::string(33, '0'),
                      e = "eeeeeee" + std::string(33, '0'), f = "fffffff" + std::string(33, '0');
    const std::string json = R"({"total_count": 9, "artifacts": [)" +
                             artifact("main", b, "2026-10-09T12:00:00Z") + "," +
                             artifact("main", a, "2026-10-08T12:00:00Z") + "," +
                             artifact("main", a, "2026-10-08T12:05:00Z") + "," +   // re-run: one per commit
                             artifact("feat/x", c, "2026-10-09T10:00:00Z") + "," +
                             artifact("feat/x", d, "2026-10-09T11:00:00Z") + "," +
                             artifact("main", e, "2026-10-09T13:00:00Z", 2) + "," +   // a fork's main: its PR
                             artifact("main", f, "2026-10-09T14:00:00Z", 1, true) + "," +   // expired
                             artifact("main", f, "2026-10-09T14:00:00Z", 1, false, "playguard_linker_map") + "," +
                             R"({"name": "playguard_release", "size_in_bytes": 1, "expired": false, "archive_download_url": "https://x", "workflow_run": {}})" +
                             "]}";
    std::vector<dev_builds::Artifact> arts;
    assert(dev_builds::parse_artifacts(json, &arts));
    assert(arts.size() == 6);
    assert(arts[0].commit == "bbbbbbb" && arts[0].branch == "main" && arts[0].repo == 1 && arts[0].head_repo == 1 &&
           arts[0].size == 4000 && arts[0].sha256 == std::string(64, 'd'));

    std::vector<dev_builds::Pull> pulls;
    assert(dev_builds::parse_pulls(R"([
        {"number": 41, "title": "feat: x", "head": {"ref": "feat/x", "sha": "x", "repo": {"id": 1}}},
        {"number": 42, "title": "fix: from a fork", "head": {"ref": "main", "repo": {"id": 2}}},
        {"number": 43, "title": "no build yet", "head": {"ref": "feat/y", "repo": {"id": 1}}},
        {"number": 44, "title": "deleted fork", "head": {"ref": "z", "repo": null}}
    ])", &pulls));
    assert(pulls.size() == 3 && pulls[0].number == 41 && pulls[1].head_repo == 2);

    const auto builds = dev_builds::combine(arts, pulls);
    // main newest first, one per commit; the fork's "main" is not main.
    assert(builds.size() == 4);
    assert(builds[0].kind == Kind::Main && builds[0].commit == "bbbbbbb" && builds[0].artifact);
    assert(builds[1].kind == Kind::Main && builds[1].commit == "aaaaaaa" && builds[1].date == "2026-10-08T12:05:00Z");
    // Pull requests by number, highest first, each its newest build.
    assert(builds[2].kind == Kind::PullRequest && builds[2].pr == 42 && builds[2].commit == "eeeeeee" &&
           builds[2].title == "fix: from a fork");
    assert(builds[3].pr == 41 && builds[3].commit == "ddddddd");
    assert(dev_builds::combine(arts, pulls, 1).size() == 3);   // main kept to 1

    std::vector<dev_builds::Artifact> none;
    assert(!dev_builds::parse_artifacts("[]", &none) && !dev_builds::parse_artifacts("x", &none));
    std::vector<dev_builds::Pull> no_pulls;
    assert(!dev_builds::parse_pulls("{}", &no_pulls) && no_pulls.empty());
}

static std::string nro(size_t size)
{
    std::string s(size, 'z');
    s.replace(0x10, 4, "NRO0");
    return s;
}

static void test_files()
{
    const std::string dir = paths::data_dir();
    assert(paths::ensure_dir(dir));
    const std::string file = dir + "/fresh.nro";
    const std::string content = nro(5000);
    assert(paths::atomic_write(file, content));

    std::string err;
    assert(dev_builds::verify_file(file, content.size(), sha(content), &err));
    assert(dev_builds::verify_file(file, content.size(), "", &err));   // no digest: the size only
    assert(!dev_builds::verify_file(file, content.size(), sha(content + "x"), &err) && err == "SHA-256 does not match");
    assert(!dev_builds::verify_file(file, 4999, sha(content), &err) && err.find("size") == 0);
    assert(!dev_builds::verify_file(dir + "/missing.nro", 1, "", &err));
    assert(dev_builds::verify_nro(file, &err));
    assert(paths::atomic_write(dir + "/zip.nro", std::string(5000, 'z')));
    assert(!dev_builds::verify_nro(dir + "/zip.nro", &err) && err == "not an NRO file");
    assert(!dev_builds::verify_nro(dir + "/missing.nro", &err));

    // Replace: the old one goes, the new one takes its name.
    const std::string target = dir + "/playguard.nro";
    assert(paths::atomic_write(target, "old"));
    assert(dev_builds::replace(target, file, &err));
    std::string got;
    assert(paths::read_file(target, got) && got == content);
    assert(!paths::read_file(file, got) && !paths::read_file(target + ".old", got));
    // A missing new file: the old one stays in place.
    assert(!dev_builds::replace(target, dir + "/missing.nro", &err));
    assert(paths::read_file(target, got) && got == content);
    // No target yet: simply created.
    assert(paths::atomic_write(file, "new"));
    assert(dev_builds::replace(dir + "/first.nro", file, &err));
    assert(paths::read_file(dir + "/first.nro", got) && got == "new");
}

static void test_zip()
{
    const std::string dir = paths::data_dir();
    const std::string nro_content = nro(300000);
    assert(paths::atomic_write(dir + "/playguard.nro", nro_content));
    assert(paths::atomic_write(dir + "/build-info.txt", "commit: x\n"));
    // Built by Python's zipfile, deflated and stored, as upload-artifact would.
    const std::string py = "python3 -c \"import zipfile,sys; d=sys.argv[1]\n"
                           "for name, mode in (('a.zip', zipfile.ZIP_DEFLATED), ('s.zip', zipfile.ZIP_STORED)):\n"
                           "  z = zipfile.ZipFile(d + '/' + name, 'w', mode)\n"
                           "  z.write(d + '/build-info.txt', 'build-info.txt'); z.write(d + '/playguard.nro', 'playguard.nro'); z.close()\" ";
    assert(std::system((py + "'" + dir + "'").c_str()) == 0);

    std::string err, got;
    for (const char* name : { "/a.zip", "/s.zip" }) {
        const std::string out = dir + "/out.nro";
        assert(zip_read::extract(dir + name, "playguard.nro", out, 1 << 20, &err));
        assert(paths::read_file(out, got) && got == nro_content);
        std::remove(out.c_str());
    }
    const std::string out = dir + "/out.nro";
    assert(!zip_read::extract(dir + "/a.zip", "missing.nro", out, 1 << 20, &err) && err.find("not in") != std::string::npos);
    assert(!zip_read::extract(dir + "/a.zip", "playguard.nro", out, 1000, &err) && err.find("too large") != std::string::npos);
    assert(!zip_read::extract(dir + "/playguard.nro", "playguard.nro", out, 1 << 20, &err));
    assert(!zip_read::extract(dir + "/missing.zip", "playguard.nro", out, 1 << 20, &err));

    // A changed byte inside the stored data: the CRC-32 catches it, nothing is left.
    std::string zip;
    assert(paths::read_file(dir + "/s.zip", zip));
    const size_t at = zip.find("NRO0");
    assert(at != std::string::npos);
    zip[at + 100] ^= 1;
    assert(paths::atomic_write(dir + "/bad.zip", zip));
    assert(!zip_read::extract(dir + "/bad.zip", "playguard.nro", out, 1 << 20, &err) && err.find("CRC") != std::string::npos);
    assert(!paths::read_file(out, got));
    // Truncated: the directory is gone.
    assert(paths::atomic_write(dir + "/cut.zip", zip.substr(0, zip.size() / 2)));
    assert(!zip_read::extract(dir + "/cut.zip", "playguard.nro", out, 1 << 20, &err));
}

static void test_auth()
{
    github_auth::DeviceCode d;
    assert(github_auth::parse_device_code(R"({"device_code": "3584d83", "user_code": "WDJB-MJHT",
        "verification_uri": "https://github.com/login/device", "expires_in": 900, "interval": 5})", &d));
    assert(d.device_code == "3584d83" && d.user_code == "WDJB-MJHT" && d.interval == 5 && d.expires_in == 900);
    assert(github_auth::parse_device_code(R"({"device_code": "x", "user_code": "Y", "verification_uri": "https://g",
        "expires_in": 999999, "interval": 0})", &d) && d.interval == 1 && d.expires_in == 3600);
    assert(!github_auth::parse_device_code(R"({"error": "unauthorized_client"})", &d));
    assert(!github_auth::parse_device_code(R"({"device_code": "x", "user_code": "Y", "verification_uri": "http://g"})", &d));
    assert(!github_auth::parse_device_code("nope", &d));

    std::string token, error;
    assert(github_auth::parse_poll(R"({"access_token": "gho_16C7e42F292c6912E7710c838347Ae178B4a", "token_type": "bearer", "scope": ""})",
                                   &token, &error) == github_auth::Poll::Token);
    assert(token == "gho_16C7e42F292c6912E7710c838347Ae178B4a");
    assert(github_auth::parse_poll(R"({"error": "authorization_pending"})", &token, &error) == github_auth::Poll::Pending);
    assert(github_auth::parse_poll(R"({"error": "slow_down", "interval": 10})", &token, &error) == github_auth::Poll::SlowDown);
    assert(github_auth::parse_poll(R"({"error": "expired_token"})", &token, &error) == github_auth::Poll::Expired);
    assert(github_auth::parse_poll(R"({"error": "access_denied"})", &token, &error) == github_auth::Poll::Denied);
    assert(github_auth::parse_poll(R"({"error": "incorrect_client_credentials"})", &token, &error) == github_auth::Poll::Failed &&
           error == "incorrect_client_credentials");
    // Nothing that could break an HTTP header is taken as a token.
    assert(github_auth::parse_poll("{\"access_token\": \"gho_x\\r\\nX-Evil: 1\"}", &token, &error) == github_auth::Poll::Failed);
    assert(github_auth::parse_poll("garbage", &token, &error) == github_auth::Poll::Failed);

    // The token file: its own, not config.json; forgotten on sign-out.
    assert(github_auth::token().empty());
    assert(github_auth::token_file() == paths::data_dir() + "/github_token");
    assert(github_auth::save_token("gho_abc123", &error));
    assert(github_auth::token() == "gho_abc123");
    assert(!github_auth::save_token("gho abc", &error));
    assert(github_auth::token() == "gho_abc123");
    assert(paths::atomic_write(github_auth::token_file(), "not a token!\n"));
    assert(github_auth::token().empty());
    github_auth::forget_token();
    assert(github_auth::token().empty());

    const auto h = github_auth::api_headers("gho_abc");
    assert(h.size() == 3 && h[2] == "Authorization: Bearer gho_abc");
    assert(github_auth::api_headers("").size() == 2);
}

int main()
{
    char dir[] = "/tmp/playguard_dev_builds_XXXXXX";
    assert(mkdtemp(dir) != nullptr);
    assert(chdir(dir) == 0);   // paths::data_dir() is ./playguard_data on the host

    test_sha256();
    test_names();
    test_release();
    test_artifacts();
    test_files();
    test_zip();
    test_auth();

    std::system((std::string("rm -rf '") + dir + "'").c_str());
    std::puts("sha256, release and artifact lists, download checks, zip extraction, replacement and GitHub sign-in assertions passed");
    return 0;
}
