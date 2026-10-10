// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/modules.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "util/paths.hpp"
#include "util/sha256.hpp"

namespace modules
{

namespace
{
std::string join(const std::string& root, const std::string& rel)
{
    if (root.empty() || root.back() == '/') return root + rel;
    return root + "/" + rel;
}

bool exists(const std::string& path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

bool is_dir(const std::string& path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

std::string sha256_of(const std::string& bytes)
{
    Sha256 h;
    h.update(bytes.data(), bytes.size());
    return h.hex();
}

// "key=value" lines.
std::string value_of(const std::string& text, const std::string& key)
{
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(at, end - at);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.compare(0, key.size() + 1, key + "=") == 0) return line.substr(key.size() + 1);
        at = end + 1;
    }
    return "";
}

bool fail(std::string* error, const std::string& what)
{
    if (error) *error = what + (errno ? std::string(": ") + std::strerror(errno) : std::string());
    return false;
}

bool remove_tree(const std::string& path)
{
    if (!is_dir(path)) return std::remove(path.c_str()) == 0 || errno == ENOENT;
    DIR* d = opendir(path.c_str());
    if (!d) return false;
    bool ok = true;
    while (dirent* e = readdir(d)) {
        if (!std::strcmp(e->d_name, ".") || !std::strcmp(e->d_name, "..")) continue;
        ok = remove_tree(path + "/" + e->d_name) && ok;
    }
    closedir(d);
    return rmdir(path.c_str()) == 0 && ok;
}
}   // namespace

const std::vector<Module>& all()
{
    static const std::vector<Module> list = {
        { Id::Rescue, "rescue", 0x4200000000505247ULL, false, "PlayGuard rescue" },
        { Id::Agent, "agent", 0x4200000000504741ULL, true, "PlayGuard agent" },
    };
    return list;
}

const Module& get(Id id)
{
    for (const auto& m : all())
        if (m.id == id) return m;
    return all().front();
}

std::string tid_text(uint64_t tid)
{
    char text[17];
    std::snprintf(text, sizeof(text), "%016llX", (unsigned long long)tid);
    return text;
}

std::string dir(const std::string& sd_root, const Module& m)
{
    return join(sd_root, "atmosphere/contents/" + tid_text(m.tid));
}

Bundle bundled(const std::string& bundle_dir, const Module& m)
{
    Bundle b;
    b.nsp = join(bundle_dir, std::string(m.name) + "/exefs.nsp");
    std::string text;
    if (!exists(b.nsp) || !paths::read_file(join(bundle_dir, std::string(m.name) + "/version.txt"), text)) return b;
    b.version = value_of(text, "version");
    b.commit = value_of(text, "commit");
    b.sha256 = value_of(text, "sha256");
    b.present = b.sha256.size() == 64;
    return b;
}

State state(const std::string& sd_root, const Module& m)
{
    State s;
    const std::string d = dir(sd_root, m);
    std::string bytes;
    s.installed = paths::read_file(d + "/exefs.nsp", bytes);
    if (s.installed) s.sha256 = sha256_of(bytes);
    s.at_boot = exists(d + "/flags/boot2.flag");
    s.backup = exists(d + "/exefs.nsp.bak");
    std::string text;
    if (s.installed && paths::read_file(d + "/version.txt", text)) {
        // Only when it is about the module in place (a file put back by hand
        // is not that version).
        if (value_of(text, "sha256") == s.sha256) s.version = value_of(text, "version");
    }
    return s;
}

Offer offer(const State& s, const Bundle& b)
{
    if (!b.present) return Offer::None;
    if (!s.installed) return Offer::Install;
    return s.sha256 == b.sha256 ? Offer::None : Offer::Update;
}

std::string toolbox_json(const Module& m)
{
    // Read by the sysmodule managers (Hekate Toolbox, ovl-sysmodules):
    // the recovery module only acts at boot, so starting it now is pointless.
    return std::string("{\n  \"name\": \"") + m.title + "\",\n  \"tid\": \"" + tid_text(m.tid) +
           "\",\n  \"requires_reboot\": " + (m.resident ? "false" : "true") + "\n}\n";
}

bool install(const std::string& sd_root, const Module& m, const Bundle& b, bool keep_backup, std::string* error)
{
    errno = 0;
    if (!b.present) return fail(error, "this build carries no copy of the module");
    std::string bytes;
    if (!paths::read_file(b.nsp, bytes)) return fail(error, "cannot read the bundled module");
    if (sha256_of(bytes) != b.sha256) {
        errno = 0;
        return fail(error, "the bundled module is damaged");
    }
    const std::string d = dir(sd_root, m), target = d + "/exefs.nsp", fresh = target + ".new", old = target + ".bak";
    if (!paths::ensure_dir(d + "/flags")) return fail(error, "cannot create " + d);
    std::string werr;
    if (!paths::atomic_write(fresh, bytes, &werr)) {
        if (error) *error = werr;
        return false;
    }
    std::string check;
    if (!paths::read_file(fresh, check) || sha256_of(check) != b.sha256) {
        std::remove(fresh.c_str());
        errno = 0;
        return fail(error, "the copy on the SD card does not match");
    }
    const bool first = !exists(target);
    if (!first) {
        std::remove(old.c_str());   // an older backup: this one is newer
        if (std::rename(target.c_str(), old.c_str()) != 0) {
            std::remove(fresh.c_str());
            return fail(error, "cannot move the installed module aside");
        }
    }
    if (std::rename(fresh.c_str(), target.c_str()) != 0) {
        const int err = errno;
        if (!first) std::rename(old.c_str(), target.c_str());
        errno = err;
        return fail(error, "cannot put the module in place");
    }
    if (!keep_backup) std::remove(old.c_str());
    paths::atomic_write(d + "/toolbox.json", toolbox_json(m));
    paths::atomic_write(d + "/version.txt",
                        "version=" + b.version + "\ncommit=" + b.commit + "\nsha256=" + b.sha256 + "\n");
    if (first && !set_at_boot(sd_root, m, true, error)) return false;
    return true;
}

void confirm(const std::string& sd_root, const Module& m)
{
    std::remove((dir(sd_root, m) + "/exefs.nsp.bak").c_str());
}

bool rollback(const std::string& sd_root, const Module& m, std::string* error)
{
    errno = 0;
    const std::string d = dir(sd_root, m), target = d + "/exefs.nsp", old = target + ".bak";
    if (!exists(old)) return fail(error, "no previous module to put back");
    const std::string failed = target + ".failed";
    std::remove(failed.c_str());
    std::rename(target.c_str(), failed.c_str());
    if (std::rename(old.c_str(), target.c_str()) != 0) {
        const int err = errno;
        std::rename(failed.c_str(), target.c_str());
        errno = err;
        return fail(error, "cannot put the previous module back");
    }
    std::remove(failed.c_str());
    std::remove((d + "/version.txt").c_str());   // its version is not known any more
    return true;
}

bool set_at_boot(const std::string& sd_root, const Module& m, bool on, std::string* error)
{
    errno = 0;
    const std::string flag = dir(sd_root, m) + "/flags/boot2.flag";
    if (!on) {
        if (std::remove(flag.c_str()) != 0 && errno != ENOENT) return fail(error, "cannot remove " + flag);
        return true;
    }
    std::string werr;
    if (!paths::atomic_write(flag, "", &werr)) {
        if (error) *error = werr;
        return false;
    }
    return true;
}

bool uninstall(const std::string& sd_root, const Module& m, std::string* error)
{
    errno = 0;
    const std::string d = dir(sd_root, m);
    if (!exists(d)) return true;
    if (!remove_tree(d)) return fail(error, "cannot remove " + d);
    return true;
}

}   // namespace modules
