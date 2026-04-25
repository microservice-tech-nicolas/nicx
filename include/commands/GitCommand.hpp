#pragma once

#include "core/ICommand.hpp"
#include "output/IRenderer.hpp"
#include "git/RepoStore.hpp"
#include <memory>
#include <filesystem>

namespace nicx::commands {

// "nicx git" — git tooling, gh auth, repo tracking and cloning.
//
// Subcommands:
//   setup          Ensure git + gh are installed, run gh auth if needed
//   auth           Run gh auth login interactively
//   status         Show git/gh auth/gpg signing status
//   add  <url>     Track a repo (optionally clone it)
//   remove <name>  Untrack a repo
//   list           List tracked repos
//   clone <name>   Clone a specific tracked repo
//   clone-all      Clone all tracked repos not yet present locally
class GitCommand final : public core::ICommand {
public:
    GitCommand(
        std::unique_ptr<output::IRenderer> renderer,
        std::unique_ptr<git::RepoStore>    store
    );

    std::string_view name()        const override;
    std::string_view description() const override;
    std::vector<std::string_view> subcommands() const override;
    int execute(std::span<const std::string_view> args) override;
    void printHelp() const override;

private:
    int runSetup();
    int runAuth();
    int runStatus();
    int runAdd(std::span<const std::string_view> args);
    int runRemove(std::string_view name);
    int runList();
    int runClone(std::string_view name);
    int runCloneAll();

    bool checkTool(std::string_view bin, std::string_view installHint);
    std::filesystem::path resolveClonePath(const git::TrackedRepo& repo) const;
    std::string devDir() const;

    std::unique_ptr<output::IRenderer> m_renderer;
    std::unique_ptr<git::RepoStore>    m_store;
};

} // namespace nicx::commands
