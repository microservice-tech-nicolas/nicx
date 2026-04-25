#include "commands/GitCommand.hpp"
#include "core/CommandRegistry.hpp"
#include "core/Config.hpp"
#include "output/TerminalRenderer.hpp"
#include <iostream>
#include <format>
#include <cstdlib>
#include <array>
#include <cstdio>
#include <filesystem>
#include <regex>

namespace nicx::commands {

// ─── Self-registration ───────────────────────────────────────────────────────

static core::CommandRegistrar s_reg{
    "git",
    [] {
        auto& cfg   = core::Config::instance();
        auto store  = std::make_unique<git::RepoStore>(cfg.configDir() / "repos.toml");
        store->load();
        return std::make_unique<GitCommand>(
            std::make_unique<output::TerminalRenderer>(),
            std::move(store));
    }
};

// ─── Helpers ────────────────────────────────────────────────────────────────

static std::string capture(const std::string& cmd) {
    std::array<char, 512> buf;
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

// ─── Construction ────────────────────────────────────────────────────────────

GitCommand::GitCommand(
    std::unique_ptr<output::IRenderer> renderer,
    std::unique_ptr<git::RepoStore>    store)
    : m_renderer(std::move(renderer))
    , m_store(std::move(store))
{}

// ─── Metadata ────────────────────────────────────────────────────────────────

std::string_view GitCommand::name()        const { return "git"; }
std::string_view GitCommand::description() const { return "Git tooling, GitHub auth and repo management"; }
std::vector<std::string_view> GitCommand::subcommands() const {
    return {"setup", "auth", "status", "add", "remove", "list", "clone", "clone-all"};
}

// ─── Dispatch ────────────────────────────────────────────────────────────────

int GitCommand::execute(std::span<const std::string_view> args) {
    if (args.empty()) { printHelp(); return 0; }

    std::string_view sub = args[0];
    std::span<const std::string_view> rest(args.data() + 1, args.size() - 1);

    if (sub == "setup")     return runSetup();
    if (sub == "auth")      return runAuth();
    if (sub == "status")    return runStatus();
    if (sub == "add")       return runAdd(rest);
    if (sub == "remove")    {
        if (rest.empty()) { m_renderer->error("Usage: nicx git remove <name>"); return 1; }
        return runRemove(rest[0]);
    }
    if (sub == "list")      return runList();
    if (sub == "clone")     {
        if (rest.empty()) { m_renderer->error("Usage: nicx git clone <name>"); return 1; }
        return runClone(rest[0]);
    }
    if (sub == "clone-all") return runCloneAll();

    m_renderer->error(std::format("Unknown subcommand '{}'. Run 'nicx help git'.", sub));
    return 1;
}

// ─── Setup ───────────────────────────────────────────────────────────────────

bool GitCommand::checkTool(std::string_view bin, std::string_view installHint) {
    if (inPath(std::string(bin).c_str())) {
        m_renderer->row(std::string(bin), "found", "ok");
        return true;
    }
    m_renderer->row(std::string(bin), "not found", "missing");
    m_renderer->warn(std::format("Install with: {}", installHint));
    return false;
}

int GitCommand::runSetup() {
    m_renderer->header("Git Setup");

    m_renderer->section("Checking required tools");
    bool gitOk = checkTool("git", "nicx pkg install git");
    bool ghOk  = checkTool("gh",  "nicx pkg install gh");

    if (!gitOk || !ghOk) {
        m_renderer->blank();
        m_renderer->warn("Install missing tools, then re-run 'nicx git setup'.");
        m_renderer->blank();
        return 1;
    }

    // git identity
    m_renderer->section("Git identity");
    std::string gitUser  = capture("git config --global user.name");
    std::string gitEmail = capture("git config --global user.email");

    if (gitUser.empty() || gitEmail.empty()) {
        m_renderer->warn("Git user.name / user.email not configured.");
        m_renderer->info("Set them with:");
        m_renderer->info("  git config --global user.name  \"Your Name\"");
        m_renderer->info("  git config --global user.email \"you@example.com\"");
    } else {
        m_renderer->row("user.name",  gitUser,  "ok");
        m_renderer->row("user.email", gitEmail, "ok");
    }

    // gh auth status
    m_renderer->section("GitHub CLI auth");
    int ghAuthRc = std::system("gh auth status >/dev/null 2>&1");
    if (ghAuthRc != 0) {
        m_renderer->warn("Not authenticated to GitHub. Running 'gh auth login'...");
        m_renderer->blank();
        return runAuth();
    }
    std::string ghUser = capture("gh api user --jq '.login' 2>/dev/null");
    m_renderer->row("Authenticated as", ghUser, "ok");

    // default dev dir
    m_renderer->section("Development directory");
    m_renderer->row("Base path", devDir(), "ok");

    m_renderer->blank();
    m_renderer->success("Git setup complete. Run 'nicx git status' for full details.");
    m_renderer->blank();
    return 0;
}

// ─── Auth ────────────────────────────────────────────────────────────────────

int GitCommand::runAuth() {
    m_renderer->header("GitHub Authentication");

    if (!inPath("gh")) {
        m_renderer->error("gh CLI not found. Run 'nicx git setup' first.");
        return 1;
    }

    m_renderer->info("Launching GitHub CLI authentication...");
    m_renderer->info("Follow the prompts to authenticate via browser or token.");
    m_renderer->blank();

    // gh auth login is interactive — pass through directly
    int rc = std::system("gh auth login");

    m_renderer->blank();
    if (rc == 0) {
        std::string user = capture("gh api user --jq '.login' 2>/dev/null");
        m_renderer->success(std::format("Authenticated as: {}", user));

        // Persist the gh user in config
        core::Config::instance().set("git.gh_user", user);

        // Set gh as git credential helper
        std::system("gh auth setup-git 2>/dev/null");
        m_renderer->success("Git credential helper configured.");
    } else {
        m_renderer->error("Authentication failed or was cancelled.");
    }

    m_renderer->blank();
    return rc;
}

// ─── Status ──────────────────────────────────────────────────────────────────

int GitCommand::runStatus() {
    m_renderer->header("Git Status");

    // git
    m_renderer->section("Git");
    if (inPath("git")) {
        std::string ver = capture("git --version");
        m_renderer->row("Version", ver, "ok");
        m_renderer->row("user.name",  capture("git config --global user.name"),  "ok");
        m_renderer->row("user.email", capture("git config --global user.email"), "ok");
        std::string signingKey = capture("git config --global user.signingkey");
        if (!signingKey.empty()) {
            m_renderer->row("signing key", signingKey, "ok");
            std::string gpgSign = capture("git config --global commit.gpgsign");
            m_renderer->row("gpg signing", gpgSign == "true" ? "enabled" : "disabled",
                            gpgSign == "true" ? "ok" : "warn");
        } else {
            m_renderer->row("gpg signing", "not configured", "warn");
        }
    } else {
        m_renderer->row("git", "not installed", "missing");
    }

    // gh
    m_renderer->section("GitHub CLI");
    if (inPath("gh")) {
        std::string ver = capture("gh --version | head -1");
        m_renderer->row("Version", ver, "ok");
        int authRc = std::system("gh auth status >/dev/null 2>&1");
        if (authRc == 0) {
            std::string user  = capture("gh api user --jq '.login' 2>/dev/null");
            std::string proto = capture("gh config get git_protocol 2>/dev/null");
            m_renderer->row("Authenticated as", user,  "ok");
            m_renderer->row("Protocol",         proto.empty() ? "ssh" : proto, "ok");
        } else {
            m_renderer->row("Auth", "not authenticated", "missing");
            m_renderer->info("Run: nicx git auth");
        }
    } else {
        m_renderer->row("gh", "not installed", "missing");
    }

    // repos
    m_renderer->section("Tracked repos");
    m_renderer->row("Count", std::to_string(m_store->all().size()), "ok");
    m_renderer->row("Dev dir", devDir(), "ok");
    m_renderer->row("Config", (core::Config::instance().configDir() / "repos.toml").string(), "ok");

    m_renderer->blank();
    return 0;
}

// ─── Add ─────────────────────────────────────────────────────────────────────

int GitCommand::runAdd(std::span<const std::string_view> args) {
    if (args.empty()) {
        m_renderer->error("Usage: nicx git add <url> [name] [org]");
        m_renderer->info("Examples:");
        m_renderer->info("  nicx git add git@github.com:microservice-tech-nicolas/nicx.git");
        m_renderer->info("  nicx git add https://github.com/torvalds/linux.git linux torvalds");
        return 1;
    }

    std::string url  = std::string(args[0]);
    std::string name, org;

    if (args.size() >= 2) name = std::string(args[1]);
    if (args.size() >= 3) org  = std::string(args[2]);

    // Auto-extract name and org from URL if not provided
    // Supports: git@github.com:org/repo.git and https://github.com/org/repo.git
    if (name.empty() || org.empty()) {
        std::smatch m;
        std::string u = url;
        std::regex sshRe(R"(git@[^:]+:([^/]+)/([^/.]+)(?:\.git)?)");
        std::regex httpsRe(R"(https?://[^/]+/([^/]+)/([^/.]+)(?:\.git)?)");

        if (std::regex_search(u, m, sshRe) || std::regex_search(u, m, httpsRe)) {
            if (org.empty())  org  = m[1].str();
            if (name.empty()) name = m[2].str();
        }
    }

    if (name.empty()) {
        m_renderer->error("Could not determine repo name from URL. Pass it explicitly.");
        return 1;
    }

    m_renderer->header(std::format("Adding repo: {}", name));

    git::TrackedRepo repo;
    repo.name = name;
    repo.org  = org;
    repo.url  = url;

    m_store->add(std::move(repo));
    m_store->save();

    m_renderer->blank();
    m_renderer->success(std::format("Tracked: {} ({})", name, url));
    m_renderer->info("Run 'nicx git clone " + name + "' to clone it.");
    m_renderer->blank();
    return 0;
}

// ─── Remove ──────────────────────────────────────────────────────────────────

int GitCommand::runRemove(std::string_view name) {
    m_renderer->header(std::format("Removing repo: {}", name));

    if (!m_store->find(name)) {
        m_renderer->error(std::format("'{}' is not tracked.", name));
        return 1;
    }

    m_store->remove(name);
    m_store->save();

    m_renderer->blank();
    m_renderer->success(std::format("'{}' removed from tracking (local directory untouched).", name));
    m_renderer->blank();
    return 0;
}

// ─── List ────────────────────────────────────────────────────────────────────

int GitCommand::runList() {
    m_renderer->header("Tracked Repositories");

    const auto& repos = m_store->all();
    if (repos.empty()) {
        m_renderer->blank();
        m_renderer->info("No repos tracked yet.");
        m_renderer->info("Add one with: nicx git add <url>");
        m_renderer->blank();
        return 0;
    }

    m_renderer->blank();
    for (const auto& r : repos) {
        auto path   = resolveClonePath(r);
        bool exists = std::filesystem::exists(path);
        std::string status = exists ? "cloned" : "not cloned";
        std::string color  = exists ? "ok"     : "warn";

        std::cout << "  ";
        // name in bold white
        std::cout << "\033[1;97m" << r.name << "\033[0m";
        if (!r.org.empty())
            std::cout << " \033[90m(" << r.org << ")\033[0m";
        std::cout << "\n";

        m_renderer->row("  url",    r.url,           color);
        m_renderer->row("  path",   path.string(),   exists ? "ok" : "warn");
        if (!r.notes.empty())
            m_renderer->row("  notes", r.notes);
    }

    m_renderer->blank();
    m_renderer->info(std::format("{} repo(s) tracked", repos.size()));
    m_renderer->blank();
    return 0;
}

// ─── Clone ───────────────────────────────────────────────────────────────────

std::string GitCommand::devDir() const {
    auto val = core::Config::instance().get("git.dev_dir");
    if (val && !val->empty()) {
        // expand ~ manually
        std::string d = *val;
        if (!d.empty() && d[0] == '~') {
            const char* home = std::getenv("HOME");
            if (home) d = std::string(home) + d.substr(1);
        }
        return d;
    }
    const char* home = std::getenv("HOME");
    return home ? std::string(home) + "/Development" : "/tmp/Development";
}

std::filesystem::path GitCommand::resolveClonePath(const git::TrackedRepo& repo) const {
    if (!repo.localPath.empty()) {
        std::string p = repo.localPath;
        if (!p.empty() && p[0] == '~') {
            const char* home = std::getenv("HOME");
            if (home) p = std::string(home) + p.substr(1);
        }
        return p;
    }
    // Default: devDir/org/name  or devDir/name if no org
    auto base = std::filesystem::path(devDir());
    if (!repo.org.empty())
        return base / repo.org / repo.name;
    return base / repo.name;
}

int GitCommand::runClone(std::string_view name) {
    auto repo = m_store->find(name);
    if (!repo) {
        m_renderer->error(std::format("'{}' is not tracked. Add it first with 'nicx git add'.", name));
        return 1;
    }

    m_renderer->header(std::format("Cloning: {}", repo->name));

    auto path = resolveClonePath(*repo);

    if (std::filesystem::exists(path)) {
        m_renderer->info(std::format("Already exists at: {}", path.string()));
        m_renderer->info("Pulling latest changes...");
        int rc = std::system(std::format("git -C \"{}\" pull", path.string()).c_str());
        m_renderer->blank();
        return rc;
    }

    std::filesystem::create_directories(path.parent_path());
    m_renderer->info(std::format("Cloning into: {}", path.string()));
    m_renderer->blank();

    int rc = std::system(
        std::format("git clone \"{}\" \"{}\"", repo->url, path.string()).c_str());

    m_renderer->blank();
    if (rc == 0)
        m_renderer->success(std::format("Cloned: {}", path.string()));
    else
        m_renderer->error("Clone failed.");
    m_renderer->blank();
    return rc;
}

int GitCommand::runCloneAll() {
    const auto& repos = m_store->all();
    if (repos.empty()) {
        m_renderer->info("No repos tracked. Add some with 'nicx git add <url>'.");
        return 0;
    }

    m_renderer->header(std::format("Cloning all {} tracked repos", repos.size()));
    m_renderer->blank();

    int failed = 0;
    for (const auto& r : repos) {
        auto path = resolveClonePath(r);
        if (std::filesystem::exists(path)) {
            m_renderer->info(std::format("{}: already exists, skipping", r.name));
            continue;
        }
        m_renderer->info(std::format("Cloning {} → {}", r.name, path.string()));
        std::filesystem::create_directories(path.parent_path());
        int rc = std::system(
            std::format("git clone \"{}\" \"{}\"", r.url, path.string()).c_str());
        if (rc == 0)
            m_renderer->success(std::format("{}: done", r.name));
        else {
            m_renderer->error(std::format("{}: FAILED", r.name));
            ++failed;
        }
    }

    m_renderer->blank();
    if (failed == 0)
        m_renderer->success(std::format("All {} repos cloned.", repos.size()));
    else
        m_renderer->warn(std::format("{} repo(s) failed.", failed));
    m_renderer->blank();
    return failed ? 1 : 0;
}

// ─── Help ────────────────────────────────────────────────────────────────────

void GitCommand::printHelp() const {
    std::cout
        << "Usage:\n"
        << "  nicx git <subcommand> [args]\n\n"
        << "Subcommands:\n"
        << "  setup              Verify tools, configure git identity, run gh auth\n"
        << "  auth               Run GitHub CLI authentication interactively\n"
        << "  status             Show git/gh/gpg signing status\n"
        << "  add <url> [name] [org]  Track a repo (auto-detects name/org from URL)\n"
        << "  remove <name>      Untrack a repo (does not delete local directory)\n"
        << "  list               List all tracked repos with clone status\n"
        << "  clone <name>       Clone a tracked repo (or pull if already present)\n"
        << "  clone-all          Clone all tracked repos not yet present\n\n"
        << "Configuration (~/.config/nicx/config.toml):\n"
        << "  [git]\n"
        << "  dev_dir = ~/Development    # base path for cloned repos\n"
        << "  gh_user = myusername       # your GitHub username\n\n"
        << "Repo storage: ~/.config/nicx/repos.toml\n\n"
        << "Examples:\n"
        << "  nicx git setup\n"
        << "  nicx git add git@github.com:microservice-tech-nicolas/nicx.git\n"
        << "  nicx git add https://github.com/torvalds/linux.git linux torvalds\n"
        << "  nicx git clone nicx\n"
        << "  nicx git clone-all\n"
        << "  nicx git list\n\n";
}

} // namespace nicx::commands
