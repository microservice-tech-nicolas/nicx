#include "core/CommandRegistry.hpp"
#include <stdexcept>
#include <algorithm>

namespace nicx::core {

CommandRegistry& CommandRegistry::instance() {
    static CommandRegistry inst;
    return inst;
}

void CommandRegistry::registerCommand(std::string name, Factory factory) {
    m_factories.emplace(std::move(name), std::move(factory));
}

std::unique_ptr<ICommand> CommandRegistry::create(std::string_view name) const {
    auto it = m_factories.find(std::string(name));
    if (it == m_factories.end()) return nullptr;
    return it->second();
}

std::vector<std::string> CommandRegistry::names() const {
    std::vector<std::string> result;
    result.reserve(m_factories.size());
    for (const auto& [k, _] : m_factories) result.push_back(k);
    std::sort(result.begin(), result.end());
    return result;
}

} // namespace nicx::core
