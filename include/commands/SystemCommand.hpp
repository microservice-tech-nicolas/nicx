#pragma once

#include "core/ICommand.hpp"
#include "output/IRenderer.hpp"
#include "system/SystemProbe.hpp"
#include <memory>

namespace nicx::commands {

// "nicx system" — subcommands: info
class SystemCommand final : public core::ICommand {
public:
    explicit SystemCommand(std::unique_ptr<output::IRenderer> renderer);

    std::string_view name() const override;
    std::string_view description() const override;
    std::vector<std::string_view> subcommands() const override;
    int execute(std::span<const std::string_view> args) override;
    void printHelp() const override;

private:
    int runInfo();

    std::unique_ptr<output::IRenderer> m_renderer;
    system::SystemProbe m_probe;
};

} // namespace nicx::commands
