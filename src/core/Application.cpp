#include "core/Application.hpp"
#include "core/CommandRegistry.hpp"
#include "core/Config.hpp"
#include "nicx/version.hpp"
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>

namespace nicx::core {

Application::Application(std::span<char* const> argv) : m_argv(argv) {}

void Application::printBanner() const {
    std::cout <<
        "\n"
        "  ███╗   ██╗██╗ ██████╗██╗  ██╗\n"
        "  ████╗  ██║██║██╔════╝╚██╗██╔╝\n"
        "  ██╔██╗ ██║██║██║      ╚███╔╝ \n"
        "  ██║╚██╗██║██║██║      ██╔██╗ \n"
        "  ██║ ╚████║██║╚██████╗██╔╝ ██╗\n"
        "  ╚═╝  ╚═══╝╚═╝ ╚═════╝╚═╝  ╚═╝\n"
        "\n"
        "  Linux system toolkit by Nicolas\n"
        "  v" << VERSION_STRING << "\n\n";
}

void Application::printVersion() const {
    std::cout << "nicx " << VERSION_STRING << "\n";
}

void Application::printHelp() const {
    printBanner();
    std::cout
        << "Usage:\n"
        << "  nicx <command> [subcommand] [options]\n\n"
        << "Commands:\n";

    auto& reg = CommandRegistry::instance();
    for (const auto& name : reg.names()) {
        auto cmd = reg.create(name);
        if (cmd) {
            std::cout << "  " << std::left << std::setw(16) << name
                      << "  " << cmd->description() << "\n";
        }
    }

    std::cout
        << "\nOptions:\n"
        << "  -h, --help       Show this help\n"
        << "  -v, --version    Show version\n"
        << "  --no-color       Disable colored output\n"
        << "\nRun 'nicx help <command>' for command-specific help.\n\n";
}

int Application::run() {
    Config::instance().load();

    // Collect args as string_views
    std::vector<std::string_view> args;
    for (std::size_t i = 1; i < m_argv.size(); ++i)
        args.emplace_back(m_argv[i]);

    if (args.empty()) {
        printHelp();
        return 0;
    }

    std::string_view first = args[0];

    if (first == "-h" || first == "--help" || first == "help") {
        if (args.size() > 1) {
            // "nicx help <command>" — banner + command help
            auto cmd = CommandRegistry::instance().create(args[1]);
            if (cmd) {
                printBanner();
                cmd->printHelp();
                return 0;
            }
            std::cerr << "Unknown command: " << args[1] << "\n";
            return 1;
        }
        printHelp(); // includes banner
        return 0;
    }

    if (first == "-v" || first == "--version" || first == "version") {
        printVersion();
        return 0;
    }

    // All commands: print compact banner first
    printBanner();

    auto cmd = CommandRegistry::instance().create(first);
    if (!cmd) {
        std::cerr << "nicx: unknown command '" << first << "'\n"
                  << "Run 'nicx --help' for available commands.\n";
        return 1;
    }

    std::span<const std::string_view> rest(args.data() + 1, args.size() - 1);
    const int rc = cmd->execute(rest);
    std::cout << "\n";
    return rc;
}

} // namespace nicx::core
