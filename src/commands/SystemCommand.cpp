#include "commands/SystemCommand.hpp"
#include "core/CommandRegistry.hpp"
#include "output/TerminalRenderer.hpp"
#include <iostream>
#include <format>
#include <cmath>

namespace nicx::commands {

// ─── Self-registration ───────────────────────────────────────────────────────

static core::CommandRegistrar s_reg{
    "system",
    [] { return std::make_unique<SystemCommand>(
            std::make_unique<output::TerminalRenderer>()); }
};

// ─── Helpers ────────────────────────────────────────────────────────────────

namespace {

std::string fmtBytes(uint64_t bytes) {
    constexpr double GiB = 1024.0 * 1024.0 * 1024.0;
    constexpr double MiB = 1024.0 * 1024.0;
    if (bytes >= GiB)
        return std::format("{:.1f} GiB", bytes / GiB);
    return std::format("{:.0f} MiB", bytes / MiB);
}

std::string fmtPercent(uint64_t used, uint64_t total) {
    if (total == 0) return "0%";
    return std::format("{:.0f}%", 100.0 * used / total);
}

} // namespace

// ─── SystemCommand ───────────────────────────────────────────────────────────

SystemCommand::SystemCommand(std::unique_ptr<output::IRenderer> renderer)
    : m_renderer(std::move(renderer)) {}

std::string_view SystemCommand::name()        const { return "system"; }
std::string_view SystemCommand::description() const { return "Display system information"; }

std::vector<std::string_view> SystemCommand::subcommands() const {
    return {"info"};
}

int SystemCommand::execute(std::span<const std::string_view> args) {
    std::string_view sub = args.empty() ? "info" : args[0];

    if (sub == "info") return runInfo();

    m_renderer->error(std::format("Unknown subcommand '{}'. Run 'nicx help system'.", sub));
    return 1;
}

void SystemCommand::printHelp() const {
    std::cout
        << "\nUsage:\n"
        << "  nicx system [subcommand]\n\n"
        << "Subcommands:\n"
        << "  info    Show full system overview (default)\n\n"
        << "Examples:\n"
        << "  nicx system\n"
        << "  nicx system info\n\n";
}

int SystemCommand::runInfo() {
    auto info = m_probe.collect();
    auto& r = *m_renderer;

    // ── OS ──────────────────────────────────────────────────────────────────
    r.header("System Information");

    r.section("Operating System");
    r.row("Distribution",  info.os.name);
    r.row("OS ID",         info.os.id);
    r.row("Version",       info.os.version.empty() ? "—" : info.os.version);
    r.row("Kernel",        info.os.kernel);
    r.row("Hostname",      info.os.hostname);
    r.row("Init system",   info.os.init,
          info.os.init == "systemd" ? "active" : "active");

    // ── CPU ─────────────────────────────────────────────────────────────────
    r.section("CPU");
    r.row("Model",        info.cpu.model);
    r.row("Architecture", info.cpu.architecture);
    r.row("Cores",        std::to_string(info.cpu.cores));
    r.row("Threads",      std::to_string(info.cpu.threads));

    // ── Memory ──────────────────────────────────────────────────────────────
    r.section("Memory");
    r.row("Total",     fmtBytes(info.memory.totalBytes));
    r.row("Used",      fmtBytes(info.memory.usedBytes),
          fmtPercent(info.memory.usedBytes, info.memory.totalBytes));
    r.row("Available", fmtBytes(info.memory.availableBytes));

    if (info.memory.swapTotalBytes) {
        r.row("Swap total", fmtBytes(*info.memory.swapTotalBytes));
        r.row("Swap used",  fmtBytes(*info.memory.swapUsedBytes),
              fmtPercent(*info.memory.swapUsedBytes, *info.memory.swapTotalBytes));
    } else {
        r.row("Swap", "none", "warn");
    }

    // ── Boot ────────────────────────────────────────────────────────────────
    r.section("Boot");
    r.row("Bootloader", info.boot.bootloader);
    r.row("Boot mode",  info.boot.bootMode, info.boot.efi ? "ok" : "warn");
    r.row("EFI",        info.boot.efi ? "yes" : "no", info.boot.efi ? "ok" : "warn");

    // ── Disks ───────────────────────────────────────────────────────────────
    r.section("Disks & Partitions");

    for (const auto& p : info.partitions) {
        double usedPct = p.totalBytes > 0
            ? 100.0 * p.usedBytes / p.totalBytes : 0.0;
        std::string status = usedPct > 90 ? "warn" : "ok";
        std::string label = std::format("{} ({})", p.mountPoint, p.filesystem);
        std::string usage = std::format("{} / {} ({})",
            fmtBytes(p.usedBytes), fmtBytes(p.totalBytes),
            fmtPercent(p.usedBytes, p.totalBytes));

        r.row(label, usage, status);

        if (p.encrypted)
            r.row("  └ encryption", "dm-crypt/LUKS", "ok");
    }

    r.blank();
    return 0;
}

} // namespace nicx::commands
