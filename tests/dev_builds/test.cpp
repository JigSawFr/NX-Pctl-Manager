// Host tests for source/util/sha256.cpp and source/util/dev_builds.cpp: the
// SHA-256 test vectors, asset names and digests, which builds a release
// object of the GitHub API holds ("dev", "pr-<n>", a release; drafts and
// stray assets ignored), their order, the download check (size, SHA-256,
// NRO header) and the replacement of the running file.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

#include "util/dev_builds.hpp"
#include "util/paths.hpp"
#include "util/sha256.hpp"

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
    assert(dev_builds::commit_of("playguard-1a2b3c4.nro") == "1a2b3c4");
    assert(dev_builds::commit_of("playguard-1A2B3C4.nro") == "1a2b3c4");
    assert(dev_builds::commit_of("playguard.nro").empty());
    assert(dev_builds::commit_of("playguard-1a2b3c.nro").empty());
    assert(dev_builds::commit_of("playguard-1a2b3c45.nro").empty());
    assert(dev_builds::commit_of("playguard-1a2b3cg.nro").empty());
    assert(dev_builds::commit_of("playguard-1a2b3c4.zip").empty());
    assert(dev_builds::commit_of("xlayguard-1a2b3c4.nro").empty());

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

static std::string asset(const std::string& name, const std::string& date, int size = 1000,
                         const std::string& digest = "")
{
    return R"({"name": ")" + name + R"(", "size": )" + std::to_string(size) + R"(, "updated_at": ")" + date +
           R"(", "browser_download_url": "https://github.com/o/r/releases/download/t/)" + name + R"(")" +
           (digest.empty() ? "" : R"(, "digest": ")" + digest + R"(")") + "}";
}

static void test_parse()
{
    const std::string d = "sha256:" + std::string(64, 'b');
    const std::string dev = R"({"tag_name": "dev", "prerelease": true, "draft": false, "assets": [)" +
                            asset("playguard-aaaaaaa.nro", "2026-10-08T10:00:00Z", 1000, d) + "," +
                            asset("playguard-bbbbbbb.nro", "2026-10-09T10:00:00Z") + "," +
                            asset("build-info.txt", "2026-10-09T10:00:00Z") + "]}";
    std::vector<Build> b;
    assert(dev_builds::parse_release(dev, &b));
    assert(b.size() == 2 && b[0].kind == Kind::Main && b[0].commit == "aaaaaaa" && b[0].sha256 == std::string(64, 'b'));
    assert(b[1].sha256.empty() && b[1].size == 1000 && b[1].date == "2026-10-09T10:00:00Z");
    assert(b[1].url == "https://github.com/o/r/releases/download/t/playguard-bbbbbbb.nro");

    // A pull request: only its newest build, with its title.
    const std::string pr = R"({"tag_name": "pr-38", "name": "feat: things", "prerelease": true, "assets": [)" +
                           asset("playguard-ccccccc.nro", "2026-10-09T09:00:00Z") + "," +
                           asset("playguard-ddddddd.nro", "2026-10-09T11:00:00Z") + "]}";
    b.clear();
    assert(dev_builds::parse_release(pr, &b));
    assert(b.size() == 1 && b[0].kind == Kind::PullRequest && b[0].pr == 38 && b[0].commit == "ddddddd" &&
           b[0].title == "feat: things");

    const std::string rel = R"({"tag_name": "v1.2.0", "prerelease": false, "assets": [)" +
                            asset("playguard.nro", "2026-10-01T00:00:00Z") + "," +
                            asset("playguard.zip", "2026-10-01T00:00:00Z") + "]}";
    b.clear();
    assert(dev_builds::parse_release(rel, &b));
    assert(b.size() == 1 && b[0].kind == Kind::Release && b[0].version == "1.2.0" && b[0].commit.empty());

    // Not builds: a draft, a pre-release under another tag, a release without
    // playguard.nro, a commit asset in a release, bad fields.
    const char* none[] = {
        R"({"tag_name": "dev", "prerelease": true, "draft": true, "assets": [{"name": "playguard-aaaaaaa.nro", "size": 1, "updated_at": "", "browser_download_url": "https://x/y"}]})",
        R"({"tag_name": "nightly", "prerelease": true, "assets": [{"name": "playguard-aaaaaaa.nro", "size": 1, "updated_at": "", "browser_download_url": "https://x/y"}]})",
        R"({"tag_name": "pr-x", "prerelease": true, "assets": [{"name": "playguard-aaaaaaa.nro", "size": 1, "updated_at": "", "browser_download_url": "https://x/y"}]})",
        R"({"tag_name": "pr-012", "prerelease": true, "assets": [{"name": "playguard-aaaaaaa.nro", "size": 1, "updated_at": "", "browser_download_url": "https://x/y"}]})",
        R"({"tag_name": "v1.0.0", "prerelease": false, "assets": [{"name": "playguard.zip", "size": 1, "updated_at": "", "browser_download_url": "https://x/y"}]})",
        R"({"tag_name": "v1.0.0", "prerelease": false, "assets": [{"name": "playguard-aaaaaaa.nro", "size": 1, "updated_at": "", "browser_download_url": "https://x/y"}]})",
        R"({"tag_name": "dev", "prerelease": true, "assets": [{"name": "playguard-aaaaaaa.nro", "size": 0, "updated_at": "", "browser_download_url": "https://x/y"}]})",
        R"({"tag_name": "dev", "prerelease": true, "assets": [{"name": "playguard-aaaaaaa.nro", "size": -5, "updated_at": "", "browser_download_url": "https://x/y"}]})",
        R"({"tag_name": "dev", "prerelease": true, "assets": [{"name": "playguard-aaaaaaa.nro", "size": "1", "updated_at": "", "browser_download_url": "https://x/y"}]})",
        R"({"tag_name": "dev", "prerelease": true, "assets": [{"name": "playguard-aaaaaaa.nro", "size": 1, "updated_at": "", "browser_download_url": "http://x/y"}]})",
        R"({"tag_name": "dev", "prerelease": true, "assets": "none"})",
        R"({"message": "Not Found"})",
        "[]",
        "not json",
        "",
    };
    for (const char* j : none) {
        b.clear();
        assert(!dev_builds::parse_release(j, &b));
        assert(b.empty());
    }

    // The list, then the order: release, main newest first, PRs highest first.
    b.clear();
    assert(dev_builds::parse_releases("[" + pr + "," + rel + "," + dev + R"(, {"tag_name": "pr-40", "prerelease": true, "name": "fix", "assets": [)" +
                                          asset("playguard-eeeeeee.nro", "2026-10-07T00:00:00Z") + "]}]",
                                      &b));
    // The same release again (fetched by name and in the list): kept once.
    assert(dev_builds::parse_release(dev, &b));
    dev_builds::sort(&b);
    assert(b.size() == 5);
    assert(b[0].kind == Kind::Release);
    assert(b[1].commit == "bbbbbbb" && b[2].commit == "aaaaaaa");
    assert(b[3].pr == 40 && b[4].pr == 38);
    assert(!dev_builds::parse_releases("{}", &b) && !dev_builds::parse_releases("[]", &b));
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

    Build b;
    b.size = content.size();
    b.sha256 = sha(content);
    std::string err;
    assert(dev_builds::verify(file, b, &err));
    b.sha256.clear();   // no digest from GitHub: size and header only
    assert(dev_builds::verify(file, b, &err));

    b.sha256 = sha(content + "x");
    assert(!dev_builds::verify(file, b, &err) && err == "SHA-256 does not match");
    b.sha256 = sha(content);
    b.size = 4999;
    assert(!dev_builds::verify(file, b, &err) && err.find("size") == 0);
    assert(!dev_builds::verify(dir + "/missing.nro", b, &err));

    const std::string not_nro(5000, 'z');
    assert(paths::atomic_write(dir + "/zip.nro", not_nro));
    Build z;
    z.size = not_nro.size();
    z.sha256 = sha(not_nro);
    assert(!dev_builds::verify(dir + "/zip.nro", z, &err) && err == "not an NRO file");

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

int main()
{
    char dir[] = "/tmp/playguard_dev_builds_XXXXXX";
    assert(mkdtemp(dir) != nullptr);
    assert(chdir(dir) == 0);   // paths::data_dir() is ./playguard_data on the host

    test_sha256();
    test_names();
    test_parse();
    test_files();

    std::system((std::string("rm -rf '") + dir + "'").c_str());
    std::puts("sha256, build list, download check and replacement assertions passed");
    return 0;
}
