#pragma once

#include <span>
#include <string_view>

namespace nicx::core {

// Application orchestrates startup, routing, and help.
// Single Responsibility: argument dispatch only.
class Application {
public:
    explicit Application(std::span<char* const> argv);
    int run();

private:
    void printBanner() const;
    void printHelp() const;
    void printVersion() const;

    std::span<char* const> m_argv;
};

} // namespace nicx::core
