// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "util/paths.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

namespace paths
{

std::string data_dir()
{
#ifdef __SWITCH__
    return "/switch/playguard";
#else
    return "./playguard_data";
#endif
}

std::string logs_dir()     { return data_dir() + "/logs"; }
std::string profiles_dir() { return data_dir() + "/profiles"; }
std::string backups_dir()  { return data_dir() + "/backups"; }
std::string exports_dir()  { return data_dir() + "/exports"; }
std::string config_file()  { return data_dir() + "/config.json"; }
std::string history_file() { return data_dir() + "/history.json"; }

std::string sd_root()
{
#ifdef __SWITCH__
    return "/";
#else
    return "./playguard_data/sd";
#endif
}

bool ensure_dir(const std::string& dir)
{
    std::string partial;
    std::stringstream ss(dir);
    std::string part;
    bool absolute = !dir.empty() && dir[0] == '/';
    if (absolute) partial = "/";
    while (std::getline(ss, part, '/')) {
        if (part.empty()) continue;
        partial += part;
        if (mkdir(partial.c_str(), 0777) != 0 && errno != EEXIST) return false;
        partial += "/";
    }
    return true;
}

bool atomic_write(const std::string& path, const std::string& content, std::string* error)
{
    auto fail = [&](const char* what, int err) {
        if (error) *error = std::string(what) + ": " + std::strerror(err);
        return false;
    };
    std::size_t slash = path.find_last_of('/');
    if (slash != std::string::npos && !ensure_dir(path.substr(0, slash)))
        return fail("Could not create the folder", errno);

    std::string tmp = path + ".tmp";
    // A previous write stopped between removing the file and renaming the
    // ".tmp": that ".tmp" is the only good copy. Promote it before reopening
    // the ".tmp" for writing truncates it.
    struct stat st;
    if (stat(path.c_str(), &st) != 0 && stat(tmp.c_str(), &st) == 0)
        std::rename(tmp.c_str(), path.c_str());
    FILE* f = std::fopen(tmp.c_str(), "wb");
    if (!f) return fail("Could not open the file", errno);
    bool ok = std::fwrite(content.data(), 1, content.size(), f) == content.size();
    int err = ok ? 0 : errno;
    if (ok && std::fflush(f) != 0) { ok = false; err = errno; }
    if (std::fclose(f) != 0 && ok) { ok = false; err = errno; }
    if (!ok) {
        std::remove(tmp.c_str());
        return fail("Could not write the file", err);
    }
    std::remove(path.c_str());   // FAT (SD card) rename does not replace an existing file
    if (std::rename(tmp.c_str(), path.c_str()) != 0) {
        // The old file is gone and the new one is complete: keep it, read_file
        // falls back to it.
        return fail("Could not rename the file", errno);
    }
    return true;
}

static bool read_whole(const std::string& path, std::string& out)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::stringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

bool read_file(const std::string& path, std::string& out)
{
    if (read_whole(path, out)) return true;
    // atomic_write stopped between removing the old file and renaming the
    // new one (crash, power loss, rename error): the ".tmp" is the complete
    // new content. It is only used when the file itself is missing.
    struct stat st;
    if (stat(path.c_str(), &st) == 0) return false;
    return read_whole(path + ".tmp", out);
}

std::vector<std::string> list_files(const std::string& dir, const std::string& suffix)
{
    std::vector<std::string> names;
    DIR* d = opendir(dir.c_str());
    if (!d) return names;
    while (dirent* e = readdir(d)) {
        std::string n = e->d_name;
        if (n.size() > suffix.size() && n.compare(n.size() - suffix.size(), suffix.size(), suffix) == 0)
            names.push_back(n);
    }
    closedir(d);
    std::sort(names.begin(), names.end());
    return names;
}

}   // namespace paths
