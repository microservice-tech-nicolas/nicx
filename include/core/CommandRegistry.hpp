#pragma once

#include "core/ICommand.hpp"
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <functional>

namespace nicx::core {

// Singleton registry — commands self-register via static initializers.
// OCP: adding a new command never touches this class.
class CommandRegistry {
public:
    using Factory = std::function<std::unique_ptr<ICommand>()>;

    static CommandRegistry& instance();

    void registerCommand(std::string name, Factory factory);
    std::unique_ptr<ICommand> create(std::string_view name) const;
    std::vector<std::string> names() const;

private:
    CommandRegistry() = default;
    std::unordered_map<std::string, Factory> m_factories;
};

// RAII registrar — place one static instance per command translation unit.
struct CommandRegistrar {
    CommandRegistrar(std::string name, CommandRegistry::Factory factory) {
        CommandRegistry::instance().registerCommand(std::move(name), std::move(factory));
    }
};

} // namespace nicx::core
