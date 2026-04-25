#pragma once

#include "core/ICommand.hpp"
#include "output/IRenderer.hpp"
#include <memory>
#include <filesystem>

namespace nicx::commands {

// "nicx cron" — managed cron scripts and crontab installation.
//
// Creates stub scripts in ~/.config/nicx/cron/:
//   minutely.sh  hourly.sh  daily.sh  weekly.sh  monthly.sh  yearly.sh
//
// Subcommands:
//   setup           Create stub scripts and install crontab entries
//   status          Show installed cron entries and script paths
//   edit <interval> Open a script in $EDITOR (minutely/hourly/daily/weekly/monthly/yearly)
//   uninstall       Remove nicx crontab entries (scripts are kept)
class CronCommand final : public core::ICommand {
public:
    explicit CronCommand(std::unique_ptr<output::IRenderer> renderer);

    std::string_view name()        const override;
    std::string_view description() const override;
    std::vector<std::string_view> subcommands() const override;
    int execute(std::span<const std::string_view> args) override;
    void printHelp() const override;

private:
    int runSetup();
    int runStatus();
    int runEdit(std::string_view interval);
    int runUninstall();

    std::filesystem::path cronDir() const;
    std::filesystem::path scriptPath(std::string_view interval) const;

    // Returns true if the user crontab already contains a nicx-managed entry
    bool userCrontabHasEntry(std::string_view interval) const;
    // Install/replace all nicx entries in the user crontab
    int installUserCrontab();
    // Remove all nicx entries from the user crontab
    int uninstallUserCrontab();

    std::unique_ptr<output::IRenderer> m_renderer;
};

} // namespace nicx::commands
