#pragma once

#include "core/ICommand.hpp"
#include "output/IRenderer.hpp"
#include "pkg/IPackageManager.hpp"
#include "pkg/PackageStore.hpp"
#include <memory>

namespace nicx::commands {

// "nicx pkg" — package management with tracking.
//
// Subcommands:
//   install <name...>   Install + track packages
//   remove  <name...>   Uninstall + untrack packages
//   update              Update all system packages
//   search  <term>      Search available packages
//   list                List tracked packages
//   sync                Re-install all tracked packages (bootstrap new system)
class PkgCommand final : public core::ICommand {
public:
    PkgCommand(
        std::unique_ptr<output::IRenderer> renderer,
        std::unique_ptr<pkg::IPackageManager> pm,
        std::unique_ptr<pkg::PackageStore> store
    );

    std::string_view name()        const override;
    std::string_view description() const override;
    std::vector<std::string_view> subcommands() const override;
    int execute(std::span<const std::string_view> args) override;
    void printHelp() const override;

private:
    int runInstall(std::span<const std::string_view> names);
    int runRemove(std::span<const std::string_view> names);
    int runUpdate();
    int runSearch(std::string_view query);
    int runList();
    int runSync();

    std::unique_ptr<output::IRenderer>   m_renderer;
    std::unique_ptr<pkg::IPackageManager> m_pm;
    std::unique_ptr<pkg::PackageStore>   m_store;
};

} // namespace nicx::commands
