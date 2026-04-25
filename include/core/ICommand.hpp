#pragma once

#include <string>
#include <string_view>
#include <span>
#include <vector>

namespace nicx::core {

// Interface for all commands — Open/Closed: extend by adding new commands,
// never modify existing infrastructure.
class ICommand {
public:
    virtual ~ICommand() = default;

    // Metadata
    virtual std::string_view name() const = 0;
    virtual std::string_view description() const = 0;

    // Subcommands this command accepts (e.g. "info", "usage")
    virtual std::vector<std::string_view> subcommands() const = 0;

    // Entry point
    virtual int execute(std::span<const std::string_view> args) = 0;

    // Help text printed for "nicx help <command>"
    virtual void printHelp() const = 0;
};

} // namespace nicx::core
