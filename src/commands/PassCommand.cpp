#include "commands/PassCommand.hpp"
#include "core/CommandRegistry.hpp"
#include "core/Config.hpp"
#include "output/TerminalRenderer.hpp"
#include <cstdlib>
#include <array>
#include <cstdio>
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

// ─── Helpers ─────────────────────────────────────────────────────────────────

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
    return std::system((std::string("command -v ") + bin + " >/dev/null 2>&1").c_str()) == 0;
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

    auto sub = args[0];
    if (sub == "setup")  return runSetup();
    if (sub == "status") return runStatus();
    if (sub == "pull")   return runPull();
    if (sub == "push")   return runPush();

    m_renderer->error(std::string("unknown subcommand: ") + std::string(sub));
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

// ─── Helpers (member) ────────────────────────────────────────────────────────

bool PassCommand::ensureInstalled(std::string_view pkg) {
    if (inPath(std::string(pkg).c_str())) return true;
    m_renderer->warn(std::string(pkg) + " is not installed");
    m_renderer->info("Install it with: nicx pkg install " + std::string(pkg));
    return false;
}

std::string PassCommand::gpgKeyId() const {
    auto& cfg = core::Config::instance();
    if (auto key = cfg.get("pass.gpg_key_id"); key && !key->empty()) return *key;
    // fall back to first secret key fingerprint
    return capture(
        "gpg --list-secret-keys --keyid-format LONG 2>/dev/null "
        "| grep '^sec' | head -1 | awk '{print $2}' | cut -d'/' -f2");
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
    m_renderer->row("gpg", "installed");

    if (!ensureInstalled("pass")) return 1;
    m_renderer->row("pass", "installed");

    std::string keyId = gpgKeyId();
    if (keyId.empty()) {
        m_renderer->error("No GPG key found. Run 'nicx gpg setup' first.");
        return 1;
    }
    m_renderer->row("gpg key", keyId);

    std::string store = storeDir();
    m_renderer->info("Initialising pass store at " + store + " with key " + keyId);
    int rc = std::system(("pass init " + keyId).c_str());
    if (rc != 0) { m_renderer->error("pass init failed"); return rc; }

    if (inPath("git")) {
        std::system("pass git init 2>/dev/null");
        m_renderer->row("git", "initialised inside password store");
        m_renderer->info("To add a remote: pass git remote add origin <url>");
        m_renderer->info("Then run: nicx pass push");
    }

    m_renderer->success("pass store ready");
    return 0;
}

int PassCommand::runStatus() {
    m_renderer->section("pass Status");

    if (!inPath("pass")) { m_renderer->row("pass", "NOT installed"); return 1; }
    m_renderer->row("pass", "installed");

    std::string store = storeDir();
    m_renderer->row("store dir", store);

    std::string gpgId = capture("cat " + store + "/.gpg-id 2>/dev/null");
    m_renderer->row("gpg-id", gpgId.empty() ? "not initialised" : gpgId);

    std::string remote = capture(
        "cd " + store + " 2>/dev/null && git remote get-url origin 2>/dev/null || true");
    m_renderer->row("git remote", remote.empty() ? "none" : remote);

    std::string count = capture("find " + store + " -name '*.gpg' 2>/dev/null | wc -l");
    m_renderer->row("passwords", count.empty() ? "0" : count);

    return 0;
}

int PassCommand::runPull() {
    m_renderer->section("pass Pull");
    if (!inPath("pass")) { m_renderer->error("pass is not installed"); return 1; }

    std::string store = storeDir();
    std::string remote = capture(
        "cd " + store + " 2>/dev/null && git remote get-url origin 2>/dev/null || true");
    if (remote.empty()) {
        m_renderer->error("No git remote configured.");
        m_renderer->info("Add one with: pass git remote add origin <url>");
        return 1;
    }

    m_renderer->info("Pulling from " + remote);
    return std::system("pass git pull");
}

int PassCommand::runPush() {
    m_renderer->section("pass Push");
    if (!inPath("pass")) { m_renderer->error("pass is not installed"); return 1; }

    std::string store = storeDir();
    std::string remote = capture(
        "cd " + store + " 2>/dev/null && git remote get-url origin 2>/dev/null || true");
    if (remote.empty()) {
        m_renderer->error("No git remote configured.");
        m_renderer->info("Add one with: pass git remote add origin <url>");
        return 1;
    }

    m_renderer->info("Pushing to " + remote);
    return std::system("pass git push");
}

} // namespace nicx::commands
