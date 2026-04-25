#pragma once

#include "core/ICommand.hpp"
#include "output/IRenderer.hpp"
#include <memory>

namespace nicx::commands {

// "nicx gpg" — GPG key management.
//
// Subcommands:
//   setup          Ensure gpg is installed, guide key generation
//   list           List available secret keys
//   status         Show which key is configured for git signing / pass
//   export <id>    Export a public key (to stdout or file)
//   import <file>  Import a key from file
class GpgCommand final : public core::ICommand {
public:
    explicit GpgCommand(std::unique_ptr<output::IRenderer> renderer);

    std::string_view name()        const override;
    std::string_view description() const override;
    std::vector<std::string_view> subcommands() const override;
    int execute(std::span<const std::string_view> args) override;
    void printHelp() const override;

private:
    int runSetup();
    int runList();
    int runStatus();
    int runExport(std::string_view keyId);
    int runImport(std::string_view file);

    std::unique_ptr<output::IRenderer> m_renderer;
};

} // namespace nicx::commands
