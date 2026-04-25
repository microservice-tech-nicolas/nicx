#include "commands/PassCommand.hpp"
#include "core/CommandRegistry.hpp"
#include "core/Config.hpp"
#include "output/TerminalRenderer.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace nicx::commands {

// ─── Self-registration ───────────────────────────────────────────────────────

static core::CommandRegistrar s_reg{
    "pass",
    [] {
        return std::make_unique<PassCommand>(
            std::make_unique<output::TerminalRenderer>());
    }
};

// ─── Helpers (file-local) ────────────────────────────────────────────────────

// Run cmd and return trimmed stdout. stderr is suppressed.
static std::string capture(const std::string& cmd) {
    std::array<char, 512> buf{};
    std::string out;
    FILE* p = popen(cmd.c_str(), "r");
    if (!p) return {};
    while (fgets(buf.data(), buf.size(), p)) out += buf.data();
    pclose(p);
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
    return out;
}

static bool inPath(const char* bin) {
    std::string cmd = std::string("command -v ") + bin + " >/dev/null 2>&1";
    return std::system(cmd.c_str()) == 0;
}

// Shell-quote a path so it is safe to embed in a command string.
static std::string shellQuote(const std::string& s) {
    std::string q = "'";
    for (char c : s) {
        if (c == '\'') q += "'\\''";
        else           q += c;
    }
    q += '\'';
    return q;
}

// ─── Construction ─────────────────────────────────────────────────────────────

PassCommand::PassCommand(std::unique_ptr<output::IRenderer> renderer)
    : m_renderer(std::move(renderer)) {}

std::string_view PassCommand::name()        const { return "pass"; }
std::string_view PassCommand::description() const { return "pass (password-store) setup and management"; }

std::vector<std::string_view> PassCommand::subcommands() const {
    return {"setup", "status", "pull", "push"};
}

int PassCommand::execute(std::span<const std::string_view> args) {
    if (args.empty()) { printHelp(); return 0; }

    const auto sub = args[0];
    if (sub == "setup")  return runSetup();
    if (sub == "status") return runStatus();
    if (sub == "pull")   return runPull();
    if (sub == "push")   return runPush();

    m_renderer->error("unknown subcommand: " + std::string(sub));
    printHelp();
    return 1;
}

void PassCommand::printHelp() const {
    m_renderer->section("nicx pass — password-store management");
    m_renderer->row("setup",  "Install pass + gpg, init store with configured GPG key");
    m_renderer->row("status", "Show pass store status, git remote, GPG key");
    m_renderer->row("pull",   "Pull latest from git remote (if configured)");
    m_renderer->row("push",   "Push to git remote");
}

// ─── Member helpers ───────────────────────────────────────────────────────────

bool PassCommand::ensureInstalled(std::string_view pkg) {
    if (inPath(std::string(pkg).c_str())) return true;
    m_renderer->warn(std::string(pkg) + " is not installed");
    m_renderer->info("Install it with: nicx pkg install " + std::string(pkg));
    return false;
}

std::string PassCommand::gpgKeyId() const {
    auto& cfg = core::Config::instance();
    if (auto key = cfg.get("pass.gpg_key_id"); key && !key->empty()) return *key;
    // Fall back to first available secret key short id
    return capture(
        "gpg --list-secret-keys --keyid-format LONG 2>/dev/null"
        " | grep '^sec' | head -1 | awk '{print $2}' | cut -d'/' -f2");
}

std::string PassCommand::storeDir() const {
    auto& cfg = core::Config::instance();
    auto optD = cfg.get("pass.store_dir");
    std::string d = (optD && !optD->empty()) ? *optD : "~/.password-store";
    if (d.size() >= 2 && d[0] == '~') {
        const char* home = std::getenv("HOME");
        if (home) d = std::string(home) + d.substr(1);
    }
    return d;
}

// ─── Subcommands ─────────────────────────────────────────────────────────────

int PassCommand::runSetup() {
    m_renderer->section("pass Setup");

    if (!ensureInstalled("gpg"))  return 1;
    m_renderer->row("gpg",  "installed");

    if (!ensureInstalled("pass")) return 1;
    m_renderer->row("pass", "installed");

    // Resolve the GPG key to use
    const std::string keyId = gpgKeyId();
    if (keyId.empty()) {
        m_renderer->error("No GPG key found. Run 'nicx gpg setup' first.");
        return 1;
    }
    m_renderer->row("gpg key", keyId);

    const std::string store = storeDir();
    m_renderer->row("store dir", store);

    // Check if the store is already initialised for this key
    const std::string gpgIdFile = store + "/.gpg-id";
    if (std::filesystem::exists(gpgIdFile)) {
        const std::string existing = capture("cat " + shellQuote(gpgIdFile) + " 2>/dev/null");
        if (!existing.empty()) {
            m_renderer->success("pass store already initialised (gpg-id: " + existing + ")");
            m_renderer->info("Run 'nicx pass status' to check the store.");
            return 0;
        }
    }

    // Initialise the store — pass init writes .gpg-id and re-encrypts all entries
    m_renderer->info("Initialising pass store at " + store + " with key " + keyId);
    std::cout.flush();
    const int rc = std::system(("pass init " + shellQuote(keyId)).c_str());
    if (rc != 0) { m_renderer->error("pass init failed"); return rc; }

    // Initialise git inside the store if git is available
    if (inPath("git")) {
        std::system("pass git init >/dev/null 2>&1");
        m_renderer->row("git", "initialised inside password store");
        m_renderer->info("To add a remote:  pass git remote add origin <url>");
        m_renderer->info("Then push:        nicx pass push");
    }

    m_renderer->success("pass store ready");
    return 0;
}

int PassCommand::runStatus() {
    m_renderer->section("pass Status");

    if (!inPath("pass")) {
        m_renderer->row("pass", "NOT installed");
        m_renderer->info("Install it with: nicx pkg install pass");
        return 1;
    }
    m_renderer->row("pass", "installed");

    const std::string store = storeDir();
    m_renderer->row("store dir", store);

    // GPG id(s) — .gpg-id may contain multiple keys, one per line
    const std::string gpgIdFile = store + "/.gpg-id";
    if (std::filesystem::exists(gpgIdFile)) {
        const std::string gpgId = capture("cat " + shellQuote(gpgIdFile) + " 2>/dev/null");
        m_renderer->row("gpg-id", gpgId.empty() ? "not initialised" : gpgId);
    } else {
        m_renderer->row("gpg-id", "not initialised — run 'nicx pass setup'");
    }

    // Git remote (only meaningful if git is set up inside the store)
    const std::string qStore = shellQuote(store);
    const std::string remote = capture(
        "git -C " + qStore + " remote get-url origin 2>/dev/null");
    m_renderer->row("git remote", remote.empty() ? "none" : remote);

    // Last sync timestamp (most recent git commit date)
    const std::string lastSync = capture(
        "git -C " + qStore + " log -1 --format='%ci' 2>/dev/null");
    if (!lastSync.empty())
        m_renderer->row("last sync", lastSync);

    // Password count
    const std::string count = capture(
        "find " + qStore + " -name '*.gpg' 2>/dev/null | wc -l | tr -d ' '");
    m_renderer->row("passwords", count.empty() ? "0" : count);

    return 0;
}

int PassCommand::runPull() {
    m_renderer->section("pass Pull");

    if (!inPath("pass")) { m_renderer->error("pass is not installed"); return 1; }

    const std::string store = storeDir();
    if (!std::filesystem::exists(store + "/.gpg-id")) {
        m_renderer->error("pass store not initialised. Run 'nicx pass setup' first.");
        return 1;
    }

    const std::string remote = capture(
        "git -C " + shellQuote(store) + " remote get-url origin 2>/dev/null");
    if (remote.empty()) {
        m_renderer->error("No git remote configured.");
        m_renderer->info("Add one with: pass git remote add origin <url>");
        return 1;
    }

    m_renderer->info("Pulling from " + remote);
    std::cout.flush();
    const int rc = std::system("pass git pull --rebase 2>&1");
    if (rc == 0) m_renderer->success("Pull complete");
    else         m_renderer->error("Pull failed (exit " + std::to_string(rc) + ")");
    return rc;
}

int PassCommand::runPush() {
    m_renderer->section("pass Push");

    if (!inPath("pass")) { m_renderer->error("pass is not installed"); return 1; }

    const std::string store = storeDir();
    if (!std::filesystem::exists(store + "/.gpg-id")) {
        m_renderer->error("pass store not initialised. Run 'nicx pass setup' first.");
        return 1;
    }

    const std::string remote = capture(
        "git -C " + shellQuote(store) + " remote get-url origin 2>/dev/null");
    if (remote.empty()) {
        m_renderer->error("No git remote configured.");
        m_renderer->info("Add one with: pass git remote add origin <url>");
        return 1;
    }

    m_renderer->info("Pushing to " + remote);
    std::cout.flush();
    const int rc = std::system("pass git push 2>&1");
    if (rc == 0) m_renderer->success("Push complete");
    else         m_renderer->error("Push failed (exit " + std::to_string(rc) + ")");
    return rc;
}

} // namespace nicx::commands
