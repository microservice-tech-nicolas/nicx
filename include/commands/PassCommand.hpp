#pragma once

#include "core/ICommand.hpp"
#include "output/IRenderer.hpp"
#include <memory>

namespace nicx::commands {

// "nicx pass" — pass (password-store) setup and status.
//
// Subcommands:
//   setup          Install pass + gpg, init store with configured key, optionally pull git remote
//   status         Show pass store status, git remote, gpg key
//   pull           Pull latest from git remote (if configured)
//   push           Push to git remote
class PassCommand final : public core::ICommand {
public:
    explicit PassCommand(std::unique_ptr<output::IRenderer> renderer);

    std::string_view name()        const override;
    std::string_view description() const override;
    std::vector<std::string_view> subcommands() const override;
    int execute(std::span<const std::string_view> args) override;
    void printHelp() const override;

private:
    int runSetup();
    int runStatus();
    int runPull();
    int runPush();

    bool ensureInstalled(std::string_view pkg);
    std::string gpgKeyId() const;
    std::string storeDir() const;

    std::unique_ptr<output::IRenderer> m_renderer;
};

} // namespace nicx::commands
