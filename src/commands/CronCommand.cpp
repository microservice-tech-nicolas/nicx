#include "commands/CronCommand.hpp"
#include "core/CommandRegistry.hpp"
#include "core/Config.hpp"
#include "output/TerminalRenderer.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace nicx::commands {

// ─── Self-registration ───────────────────────────────────────────────────────

static core::CommandRegistrar s_reg{
    "cron",
    [] {
        return std::make_unique<CronCommand>(
            std::make_unique<output::TerminalRenderer>());
    }
};

// ─── Constants ───────────────────────────────────────────────────────────────

// All managed intervals in order.
static constexpr std::array<std::string_view, 6> INTERVALS = {
    "minutely", "hourly", "daily", "weekly", "monthly", "yearly"
};

// crontab schedule for each interval.
// Format: minute hour day-of-month month day-of-week
static constexpr std::array<std::string_view, 6> SCHEDULES = {
    "* * * * *",        // minutely  — every minute
    "0 * * * *",        // hourly    — top of every hour
    "0 0 * * *",        // daily     — midnight every day
    "0 0 * * 0",        // weekly    — midnight every Sunday
    "0 0 1 * *",        // monthly   — midnight 1st of month
    "0 0 1 1 *"         // yearly    — midnight 1st January
};

static const std::string CRONTAB_MARKER = "# nicx-managed";

// ─── Helpers (file-local) ────────────────────────────────────────────────────

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

CronCommand::CronCommand(std::unique_ptr<output::IRenderer> renderer)
    : m_renderer(std::move(renderer)) {}

std::string_view CronCommand::name()        const { return "cron"; }
std::string_view CronCommand::description() const { return "Managed cron scripts and crontab installation"; }

std::vector<std::string_view> CronCommand::subcommands() const {
    return {"setup", "status", "edit", "uninstall"};
}

int CronCommand::execute(std::span<const std::string_view> args) {
    if (args.empty()) { printHelp(); return 0; }

    const auto sub = args[0];
    if (sub == "setup")     return runSetup();
    if (sub == "status")    return runStatus();
    if (sub == "uninstall") return runUninstall();
    if (sub == "edit") {
        if (args.size() < 2) {
            m_renderer->error("edit requires an interval: minutely hourly daily weekly monthly yearly");
            return 1;
        }
        return runEdit(args[1]);
    }

    m_renderer->error("unknown subcommand: " + std::string(sub));
    printHelp();
    return 1;
}

void CronCommand::printHelp() const {
    m_renderer->section("nicx cron — managed cron scripts");
    m_renderer->row("setup",           "Create stub scripts and install user crontab entries");
    m_renderer->row("status",          "Show installed cron entries and script paths");
    m_renderer->row("edit <interval>", "Open a script in $EDITOR");
    m_renderer->row("uninstall",       "Remove nicx crontab entries (scripts are kept)");
    m_renderer->blank();
    m_renderer->info("Intervals: minutely  hourly  daily  weekly  monthly  yearly");
    m_renderer->info("Scripts live in: ~/.config/nicx/cron/");
}

// ─── Path helpers ─────────────────────────────────────────────────────────────

std::filesystem::path CronCommand::cronDir() const {
    return core::Config::instance().configDir() / "cron";
}

std::filesystem::path CronCommand::scriptPath(std::string_view interval) const {
    return cronDir() / (std::string(interval) + ".sh");
}

// ─── Crontab helpers ──────────────────────────────────────────────────────────

bool CronCommand::userCrontabHasEntry(std::string_view interval) const {
    const std::string current = capture("crontab -l 2>/dev/null");
    return current.find(std::string(interval) + ".sh") != std::string::npos;
}

int CronCommand::installUserCrontab() {
    // Read existing crontab, strip all nicx-managed lines, then append fresh ones.
    const std::string existing = capture("crontab -l 2>/dev/null");

    std::ostringstream out;
    // Re-emit all non-nicx lines
    std::istringstream ss(existing);
    std::string line;
    bool inNicxBlock = false;
    while (std::getline(ss, line)) {
        if (line == "# >>> nicx cron >>>") { inNicxBlock = true; continue; }
        if (line == "# <<< nicx cron <<<") { inNicxBlock = false; continue; }
        if (!inNicxBlock) out << line << "\n";
    }

    // Append the nicx block
    out << "# >>> nicx cron >>>\n";
    out << "# Managed by 'nicx cron' — edit scripts in ~/.config/nicx/cron/\n";
    for (std::size_t i = 0; i < INTERVALS.size(); ++i) {
        const auto& interval = INTERVALS[i];
        const auto path      = scriptPath(interval).string();
        out << SCHEDULES[i] << "  " << shellQuote(path) << " >> "
            << shellQuote(cronDir().string() + "/" + std::string(interval) + ".log")
            << " 2>&1  " << CRONTAB_MARKER << " " << interval << "\n";
    }
    out << "# <<< nicx cron <<<\n";

    // Write via a temp file piped to crontab
    const std::string tmpFile = "/tmp/nicx-crontab-" + std::to_string(getpid());
    {
        std::ofstream f(tmpFile);
        if (!f) { m_renderer->error("Failed to write temp crontab file"); return 1; }
        f << out.str();
    }

    const int rc = std::system(("crontab " + shellQuote(tmpFile)).c_str());
    std::filesystem::remove(tmpFile);
    return rc;
}

int CronCommand::uninstallUserCrontab() {
    const std::string existing = capture("crontab -l 2>/dev/null");
    if (existing.empty()) return 0;

    std::ostringstream out;
    std::istringstream ss(existing);
    std::string line;
    bool inNicxBlock = false;
    while (std::getline(ss, line)) {
        if (line == "# >>> nicx cron >>>") { inNicxBlock = true; continue; }
        if (line == "# <<< nicx cron <<<") { inNicxBlock = false; continue; }
        if (!inNicxBlock) out << line << "\n";
    }

    const std::string remaining = out.str();
    const std::string tmpFile = "/tmp/nicx-crontab-" + std::to_string(getpid());
    {
        std::ofstream f(tmpFile);
        if (!f) { m_renderer->error("Failed to write temp crontab file"); return 1; }
        f << remaining;
    }

    int rc;
    // If nothing remains, remove the crontab entirely
    const bool isEmpty = remaining.find_first_not_of(" \t\r\n") == std::string::npos;
    if (isEmpty)
        rc = std::system("crontab -r 2>/dev/null; true");
    else
        rc = std::system(("crontab " + shellQuote(tmpFile)).c_str());

    std::filesystem::remove(tmpFile);
    return rc;
}

// ─── Subcommands ─────────────────────────────────────────────────────────────

int CronCommand::runSetup() {
    m_renderer->section("Cron Setup");

    if (!inPath("crontab")) {
        m_renderer->error("crontab is not available on this system");
        return 1;
    }

    // 1. Create cron directory
    const auto dir = cronDir();
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        m_renderer->error("Failed to create " + dir.string() + ": " + ec.message());
        return 1;
    }
    m_renderer->row("cron dir", dir.string());

    // 2. Create stub scripts (never overwrite existing ones)
    for (const auto& interval : INTERVALS) {
        const auto path = scriptPath(interval);
        if (!std::filesystem::exists(path)) {
            std::ofstream f(path);
            if (!f) {
                m_renderer->error("Failed to create " + path.string());
                return 1;
            }
            f << "#!/usr/bin/env bash\n"
              << "# nicx cron — " << interval << " script\n"
              << "# Runs " << interval << ". Add your commands below.\n"
              << "# Logs are written to: " << dir.string() << "/" << interval << ".log\n"
              << "\n"
              << "# Example:\n"
              << "# echo \"[$(date)] " << interval << " tick\" >> ~/nicx-cron.log\n";
            // Make executable
            std::filesystem::permissions(path,
                std::filesystem::perms::owner_read  |
                std::filesystem::perms::owner_write |
                std::filesystem::perms::owner_exec  |
                std::filesystem::perms::group_read  |
                std::filesystem::perms::group_exec  |
                std::filesystem::perms::others_read |
                std::filesystem::perms::others_exec,
                std::filesystem::perm_options::replace, ec);
            m_renderer->row(std::string(interval) + ".sh", "created");
        } else {
            m_renderer->row(std::string(interval) + ".sh", "exists (unchanged)");
        }
    }

    // 3. Install user crontab entries
    m_renderer->info("Installing user crontab entries...");
    std::cout.flush();
    const int rc = installUserCrontab();
    if (rc != 0) {
        m_renderer->error("Failed to install crontab entries");
        return rc;
    }

    m_renderer->success("Cron scripts and crontab entries installed");
    m_renderer->info("Edit scripts with: nicx cron edit <interval>");
    m_renderer->info("Logs appear in:    " + dir.string() + "/<interval>.log");
    return 0;
}

int CronCommand::runStatus() {
    m_renderer->section("Cron Status");

    if (!inPath("crontab")) {
        m_renderer->row("crontab", "not available");
        return 1;
    }
    m_renderer->row("crontab", "available");

    const auto dir = cronDir();
    m_renderer->row("cron dir", dir.string());
    m_renderer->blank();

    // Script file status
    m_renderer->section("Scripts");
    for (const auto& interval : INTERVALS) {
        const auto path = scriptPath(interval);
        const bool exists = std::filesystem::exists(path);
        std::string detail = exists ? path.string() : "missing — run 'nicx cron setup'";
        m_renderer->row(std::string(interval) + ".sh", detail);
    }

    // Crontab entries
    m_renderer->blank();
    m_renderer->section("User Crontab Entries");
    const std::string crontab = capture("crontab -l 2>/dev/null");
    bool foundAny = false;
    if (!crontab.empty()) {
        std::istringstream ss(crontab);
        std::string line;
        while (std::getline(ss, line)) {
            if (line.find(CRONTAB_MARKER) != std::string::npos)  {
                m_renderer->info(line);
                foundAny = true;
            }
        }
    }
    if (!foundAny)
        m_renderer->warn("No nicx crontab entries found — run 'nicx cron setup'");

    return 0;
}

int CronCommand::runEdit(std::string_view interval) {
    // Validate interval
    bool valid = false;
    for (const auto& iv : INTERVALS) if (iv == interval) { valid = true; break; }
    if (!valid) {
        m_renderer->error("Unknown interval: " + std::string(interval));
        m_renderer->info("Valid intervals: minutely  hourly  daily  weekly  monthly  yearly");
        return 1;
    }

    const auto path = scriptPath(interval);
    if (!std::filesystem::exists(path)) {
        m_renderer->warn("Script does not exist yet. Run 'nicx cron setup' first.");
        return 1;
    }

    // Resolve editor: $VISUAL > $EDITOR > vi
    const char* editor = std::getenv("VISUAL");
    if (!editor || std::string(editor).empty()) editor = std::getenv("EDITOR");
    if (!editor || std::string(editor).empty()) editor = "vi";

    m_renderer->info("Opening " + path.string() + " in " + std::string(editor));
    std::cout.flush();
    return std::system((std::string(editor) + " " + shellQuote(path.string())).c_str());
}

int CronCommand::runUninstall() {
    m_renderer->section("Cron Uninstall");

    const int rc = uninstallUserCrontab();
    if (rc == 0)
        m_renderer->success("nicx crontab entries removed (scripts in " + cronDir().string() + " are kept)");
    else
        m_renderer->error("Failed to update crontab");
    return rc;
}

} // namespace nicx::commands
