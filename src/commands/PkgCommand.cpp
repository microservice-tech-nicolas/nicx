#include "commands/PkgCommand.hpp"
#include "core/CommandRegistry.hpp"
#include "core/Config.hpp"
#include "output/TerminalRenderer.hpp"
#include "pkg/PackageManagerDetector.hpp"
#include "pkg/PackageStore.hpp"
#include <iostream>
#include <format>

namespace nicx::commands {

// ─── Self-registration ───────────────────────────────────────────────────────

static core::CommandRegistrar s_reg{
    "pkg",
    [] {
        auto& cfg   = core::Config::instance();
        auto store  = std::make_unique<pkg::PackageStore>(
            cfg.configDir() / "packages.toml");
        store->load();

        auto pm = pkg::PackageManagerDetector::detect();

        return std::make_unique<PkgCommand>(
            std::make_unique<output::TerminalRenderer>(),
            std::move(pm),
            std::move(store));
    }
};

// ─── Construction ────────────────────────────────────────────────────────────

PkgCommand::PkgCommand(
    std::unique_ptr<output::IRenderer>    renderer,
    std::unique_ptr<pkg::IPackageManager> pm,
    std::unique_ptr<pkg::PackageStore>    store)
    : m_renderer(std::move(renderer))
    , m_pm(std::move(pm))
    , m_store(std::move(store))
{}

// ─── Metadata ────────────────────────────────────────────────────────────────

std::string_view PkgCommand::name()        const { return "pkg"; }
std::string_view PkgCommand::description() const {
    return "Package management with install tracking";
}
std::vector<std::string_view> PkgCommand::subcommands() const {
    return {"install", "remove", "update", "search", "list", "sync"};
}

// ─── Dispatch ────────────────────────────────────────────────────────────────

int PkgCommand::execute(std::span<const std::string_view> args) {
    if (!m_pm) {
        m_renderer->error("No supported package manager found on this system.");
        m_renderer->info("Supported: dnf, pacman, yay, apt, zypper");
        return 1;
    }

    if (args.empty()) { printHelp(); return 0; }

    std::string_view sub = args[0];
    std::span<const std::string_view> rest(args.data() + 1, args.size() - 1);

    if (sub == "install") return runInstall(rest);
    if (sub == "remove")  return runRemove(rest);
    if (sub == "update")  return runUpdate();
    if (sub == "search")  {
        if (rest.empty()) {
            m_renderer->error("Usage: nicx pkg search <term>");
            return 1;
        }
        return runSearch(rest[0]);
    }
    if (sub == "list")  return runList();
    if (sub == "sync")  return runSync();

    m_renderer->error(std::format("Unknown subcommand '{}'. Run 'nicx help pkg'.", sub));
    return 1;
}

// ─── Install ─────────────────────────────────────────────────────────────────

int PkgCommand::runInstall(std::span<const std::string_view> names) {
    if (names.empty()) {
        m_renderer->error("Usage: nicx pkg install <package> [package...]");
        return 1;
    }

    m_renderer->header(std::format("Installing via {}", m_pm->displayName()));

    std::vector<std::string> toInstall;
    for (auto name : names) toInstall.emplace_back(name);

    int rc = m_pm->install(toInstall);

    if (rc == 0) {
        // Track each package
        for (const auto& name : toInstall) {
            pkg::TrackedPackage tracked;
            tracked.name   = name;
            tracked.method = pkg::InstallMethod::PackageManager;
            tracked.pmId   = std::string(m_pm->id());
            m_store->add(std::move(tracked));
        }
        m_store->save();

        m_renderer->blank();
        for (const auto& name : toInstall)
            m_renderer->success(std::format("{} installed and tracked", name));
        m_renderer->info(std::format("Saved to {}", (core::Config::instance().configDir() / "packages.toml").string()));
    } else {
        m_renderer->blank();
        m_renderer->error("Installation failed. Packages not tracked.");
    }

    m_renderer->blank();
    return rc;
}

// ─── Remove ──────────────────────────────────────────────────────────────────

int PkgCommand::runRemove(std::span<const std::string_view> names) {
    if (names.empty()) {
        m_renderer->error("Usage: nicx pkg remove <package> [package...]");
        return 1;
    }

    m_renderer->header(std::format("Removing via {}", m_pm->displayName()));

    std::vector<std::string> toRemove;
    for (auto name : names) toRemove.emplace_back(name);

    int rc = m_pm->remove(toRemove);

    m_renderer->blank();
    if (rc == 0) {
        for (const auto& name : toRemove) {
            m_store->remove(name);
            m_renderer->success(std::format("{} removed and untracked", name));
        }
        m_store->save();
    } else {
        m_renderer->error("Removal failed. Tracking list unchanged.");
    }

    m_renderer->blank();
    return rc;
}

// ─── Update ──────────────────────────────────────────────────────────────────

int PkgCommand::runUpdate() {
    m_renderer->header(std::format("Updating all packages via {}", m_pm->displayName()));
    m_renderer->blank();
    int rc = m_pm->update();
    m_renderer->blank();
    if (rc == 0)
        m_renderer->success("System update complete");
    else
        m_renderer->error("Update failed");
    m_renderer->blank();
    return rc;
}

// ─── Search ──────────────────────────────────────────────────────────────────

int PkgCommand::runSearch(std::string_view query) {
    m_renderer->header(std::format("Search: \"{}\"  [{}]", query, m_pm->displayName()));

    auto results = m_pm->search(query);

    if (results.empty()) {
        m_renderer->blank();
        m_renderer->info("No results found.");
        m_renderer->blank();
        return 0;
    }

    m_renderer->blank();
    for (const auto& r : results)
        m_renderer->pkg(r.name, r.version, r.description, r.installed);

    m_renderer->blank();
    m_renderer->info(std::format("{} result(s)", results.size()));
    m_renderer->blank();
    return 0;
}

// ─── List ────────────────────────────────────────────────────────────────────

int PkgCommand::runList() {
    const auto& pkgs = m_store->all();

    m_renderer->header("Tracked Packages");

    if (pkgs.empty()) {
        m_renderer->blank();
        m_renderer->info("No packages tracked yet.");
        m_renderer->info("Use 'nicx pkg install <name>' to install and track packages.");
        m_renderer->blank();
        return 0;
    }

    m_renderer->blank();
    for (const auto& pkg : pkgs) {
        std::string method = pkg.method == pkg::InstallMethod::Manual ? "manual" : "pm";
        m_renderer->trackedPkg(pkg.name, pkg.pmId, method, pkg.notes);
    }

    m_renderer->blank();
    m_renderer->info(std::format("{} package(s) tracked  |  {}",
        pkgs.size(),
        (core::Config::instance().configDir() / "packages.toml").string()));
    m_renderer->blank();
    return 0;
}

// ─── Sync ────────────────────────────────────────────────────────────────────

int PkgCommand::runSync() {
    const auto& pkgs = m_store->all();

    if (pkgs.empty()) {
        m_renderer->info("Nothing to sync — no packages tracked.");
        return 0;
    }

    m_renderer->header(std::format("Syncing {} tracked package(s) via {}",
        pkgs.size(), m_pm->displayName()));
    m_renderer->blank();

    std::vector<std::string> toInstall;
    std::vector<std::string> skipped;

    for (const auto& pkg : pkgs) {
        if (pkg.method == pkg::InstallMethod::Manual) {
            skipped.push_back(pkg.name);
            continue;
        }
        std::string resolved = m_store->resolveNameFor(pkg, m_pm->id());
        if (m_pm->isInstalled(resolved)) {
            m_renderer->info(std::format("{} already installed, skipping", resolved));
        } else {
            toInstall.push_back(resolved);
        }
    }

    if (!skipped.empty()) {
        m_renderer->blank();
        m_renderer->warn("Skipping manual installs (install these yourself):");
        for (const auto& s : skipped)
            m_renderer->warn(std::format("  {}", s));
    }

    if (toInstall.empty()) {
        m_renderer->blank();
        m_renderer->success("All packages already installed.");
        m_renderer->blank();
        return 0;
    }

    m_renderer->blank();
    m_renderer->info(std::format("Installing: {}", [&]{
        std::string s;
        for (const auto& n : toInstall) s += n + " ";
        return s;
    }()));
    m_renderer->blank();

    int rc = m_pm->install(toInstall);

    m_renderer->blank();
    if (rc == 0)
        m_renderer->success(std::format("Sync complete. {} package(s) installed.", toInstall.size()));
    else
        m_renderer->error("Sync encountered errors.");
    m_renderer->blank();
    return rc;
}

// ─── Help ────────────────────────────────────────────────────────────────────

void PkgCommand::printHelp() const {
    std::cout
        << "\nUsage:\n"
        << "  nicx pkg <subcommand> [args]\n\n"
        << "Subcommands:\n"
        << "  install <pkg...>   Install package(s) and add to tracking list\n"
        << "  remove  <pkg...>   Remove package(s) and remove from tracking list\n"
        << "  update             Update all system packages\n"
        << "  search  <term>     Search available packages\n"
        << "  list               List all tracked packages\n"
        << "  sync               Install all tracked packages (use on a fresh system)\n\n"
        << "Package tracking:\n"
        << "  Installed packages are saved to ~/.config/nicx/packages.toml\n"
        << "  Run 'nicx pkg sync' on a new machine to restore your package list.\n\n"
        << "Examples:\n"
        << "  nicx pkg install neovim tmux fzf\n"
        << "  nicx pkg remove htop\n"
        << "  nicx pkg search ripgrep\n"
        << "  nicx pkg update\n"
        << "  nicx pkg list\n"
        << "  nicx pkg sync\n\n";
}

} // namespace nicx::commands
