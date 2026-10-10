// Host tests for source/util/modules.cpp on fake SD cards: what PlayGuard
// carries (a bundle with its version.txt, a damaged one), the state on the
// card (installed by hand, at boot, a backup pending), what the screen
// offers, a first install (boot2.flag, toolbox.json, version.txt), an update
// keeping the previous module until confirmed or put back, the boot switch,
// and removing a module with everything in its folder.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "util/modules.hpp"
#include "util/paths.hpp"
#include "util/sha256.hpp"

using namespace modules;

static std::string g_base;
static int g_n = 0;

static std::string fresh(const char* what)
{
    std::string root = g_base + "/" + what + std::to_string(g_n++);
    assert(paths::ensure_dir(root));
    return root;
}

static void put(const std::string& path, const std::string& content)
{
    assert(paths::atomic_write(path, content));
}

static bool exists(const std::string& path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

static std::string read(const std::string& path)
{
    std::string text;
    assert(paths::read_file(path, text));
    return text;
}

static std::string sha(const std::string& bytes)
{
    Sha256 h;
    h.update(bytes.data(), bytes.size());
    return h.hex();
}

// A bundle as cmake/bundle_sysmodules.cmake writes it.
static std::string bundle(const std::string& content, const std::string& version, bool damaged = false)
{
    const std::string dir = fresh("romfs");
    put(dir + "/rescue/exefs.nsp", content);
    put(dir + "/rescue/version.txt",
        "version=" + version + "\r\ncommit=abc1234\nsha256=" + sha(damaged ? content + "x" : content) + "\n");
    return dir;
}

static void test_table()
{
    assert(all().size() == 2);
    assert(get(Id::Rescue).tid == 0x4200000000505247ULL && !get(Id::Rescue).resident);
    assert(get(Id::Agent).resident && std::string(get(Id::Agent).name) == "agent");
    assert(tid_text(0x4200000000505247ULL) == "4200000000505247");
    assert(dir("/", get(Id::Rescue)) == "/atmosphere/contents/4200000000505247");
    assert(dir("sd", get(Id::Rescue)) == "sd/atmosphere/contents/4200000000505247");
    const std::string tb = toolbox_json(get(Id::Rescue));
    assert(tb.find("\"tid\": \"4200000000505247\"") != std::string::npos);
    assert(tb.find("\"requires_reboot\": true") != std::string::npos);
    assert(toolbox_json(get(Id::Agent)).find("\"requires_reboot\": false") != std::string::npos);
}

static void test_bundle_and_state()
{
    const Module& m = get(Id::Rescue);
    const Bundle none = bundled(fresh("empty"), m);
    assert(!none.present);
    const Bundle b = bundled(bundle("NSP0 v1", "1.1.0"), m);
    assert(b.present && b.version == "1.1.0" && b.commit == "abc1234" && b.sha256 == sha("NSP0 v1"));

    const std::string sd = fresh("sd");
    State s = state(sd, m);
    assert(!s.installed && !s.at_boot && !s.backup && s.sha256.empty());
    assert(offer(s, b) == Offer::Install);
    assert(offer(s, none) == Offer::None);

    // Installed by hand from the release zip: no version.txt.
    put(dir(sd, m) + "/exefs.nsp", "NSP0 v0");
    put(dir(sd, m) + "/flags/boot2.flag", "");
    s = state(sd, m);
    assert(s.installed && s.at_boot && s.version.empty() && s.sha256 == sha("NSP0 v0"));
    assert(offer(s, b) == Offer::Update);
    assert(offer(s, none) == Offer::None);
    // A version.txt about another file says nothing.
    put(dir(sd, m) + "/version.txt", "version=9.9.9\nsha256=" + sha("other") + "\n");
    assert(state(sd, m).version.empty());
}

static void test_install_update_rollback()
{
    const Module& m = get(Id::Rescue);
    const std::string sd = fresh("sd");
    const Bundle v1 = bundled(bundle("NSP0 v1", "1.1.0"), m);
    std::string err;

    // First install: in place, at boot, toolbox.json, version.txt.
    assert(install(sd, m, v1, false, &err));
    State s = state(sd, m);
    assert(s.installed && s.at_boot && !s.backup && s.sha256 == v1.sha256 && s.version == "1.1.0");
    assert(read(dir(sd, m) + "/toolbox.json") == toolbox_json(m));
    assert(offer(s, v1) == Offer::None);
    assert(!exists(dir(sd, m) + "/exefs.nsp.new"));

    // The boot choice survives an update.
    assert(set_at_boot(sd, m, false, &err));
    assert(!state(sd, m).at_boot);
    assert(set_at_boot(sd, m, false, &err));   // already off
    const Bundle v2 = bundled(bundle("NSP0 v2", "1.2.0"), m);
    assert(offer(state(sd, m), v2) == Offer::Update);
    assert(install(sd, m, v2, true, &err));
    s = state(sd, m);
    assert(s.sha256 == v2.sha256 && s.version == "1.2.0" && s.backup && !s.at_boot);
    assert(read(dir(sd, m) + "/exefs.nsp.bak") == "NSP0 v1");

    // The new one did not start: the previous one goes back.
    assert(rollback(sd, m, &err));
    s = state(sd, m);
    assert(s.sha256 == v1.sha256 && !s.backup && s.version.empty());
    assert(!rollback(sd, m, &err) && !err.empty());   // nothing left to put back

    // Updated again, and confirmed: the backup goes.
    assert(install(sd, m, v2, true, &err));
    confirm(sd, m);
    s = state(sd, m);
    assert(s.sha256 == v2.sha256 && !s.backup);

    // A damaged bundle installs nothing and keeps what is there.
    const Bundle bad = bundled(bundle("NSP0 v3", "1.3.0", true), m);
    assert(bad.present);
    assert(!install(sd, m, bad, false, &err) && err.find("damaged") != std::string::npos);
    assert(state(sd, m).sha256 == v2.sha256);
    const Bundle absent;
    assert(!install(sd, m, absent, false, &err));

    // Removed with everything in its folder.
    put(dir(sd, m) + "/LICENSE.txt", "GPL");
    assert(set_at_boot(sd, m, true, &err));
    assert(uninstall(sd, m, &err));
    assert(!exists(dir(sd, m)) && !state(sd, m).installed);
    assert(uninstall(sd, m, &err));   // already gone
}

// Runs the update plan with the steps that fail, and returns the steps taken.
static std::vector<Step> plan(bool running, std::vector<Step> failing)
{
    std::vector<Step> out;
    Step s = first_step(running);
    for (int guard = 0; guard < 20; guard++) {
        out.push_back(s);
        if (step_final(s)) return out;
        bool ok = true;
        for (Step f : failing) ok &= f != s;
        s = next_step(s, ok, running);
    }
    assert(!"the update plan does not end");
    return out;
}

static void test_update_plan()
{
    using S = Step;
    // Running: stopped cleanly, swapped, started, answering, confirmed.
    assert(plan(true, {}) == std::vector<S>({ S::Shutdown, S::Stop, S::Swap, S::Start, S::Check, S::Confirm, S::Done }));
    // Not running: no stop; started and checked all the same.
    assert(plan(false, {}) == std::vector<S>({ S::Swap, S::Start, S::Check, S::Confirm, S::Done }));
    // A clean stop refused: stopped anyway.
    assert(plan(true, { S::Shutdown }).back() == S::Done);
    // Cannot be stopped: nothing changed.
    assert(plan(true, { S::Stop }) == std::vector<S>({ S::Shutdown, S::Stop, S::Failed }));
    // The swap failed (the previous one still in place): started again.
    assert(plan(true, { S::Swap }) == std::vector<S>({ S::Shutdown, S::Stop, S::Swap, S::Restart, S::RolledBack }));
    assert(plan(false, { S::Swap }) == std::vector<S>({ S::Swap, S::Failed }));
    // The new one does not answer: stopped, the previous one back and started.
    assert(plan(true, { S::Check }) == std::vector<S>({ S::Shutdown, S::Stop, S::Swap, S::Start, S::Check, S::StopNew,
                                                        S::Restore, S::Restart, S::RolledBack }));
    assert(plan(true, { S::Start }) == std::vector<S>({ S::Shutdown, S::Stop, S::Swap, S::Start, S::StopNew, S::Restore,
                                                        S::Restart, S::RolledBack }));
    // Not running before: put back, left stopped.
    assert(plan(false, { S::Check }) == std::vector<S>({ S::Swap, S::Start, S::Check, S::StopNew, S::Restore, S::RolledBack }));
    // Nothing to put back, or the previous one does not start again.
    assert(plan(true, { S::Check, S::Restore }).back() == S::Failed);
    assert(plan(true, { S::Check, S::Restart }).back() == S::Failed);
    // StopNew going wrong changes nothing: the files are put back anyway.
    assert(plan(true, { S::Check, S::StopNew }).back() == S::RolledBack);
    assert(std::string(step_name(S::RolledBack)) == "rolled_back" && step_final(S::Failed) && !step_final(S::Check));
}

int main()
{
    char base[] = "/tmp/playguard_modules_XXXXXX";
    assert(mkdtemp(base) != nullptr);
    g_base = base;

    test_table();
    test_bundle_and_state();
    test_install_update_rollback();
    test_update_plan();

    const std::string cleanup = std::string("rm -rf '") + base + "'";
    assert(std::system(cleanup.c_str()) == 0);
    std::puts("modules table, bundle, state, install, update, rollback, removal and update plan assertions passed");
    return 0;
}
