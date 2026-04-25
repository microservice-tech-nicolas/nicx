#include "output/TerminalRenderer.hpp"
#include <iostream>
#include <iomanip>
#include <format>

namespace nicx::output {

namespace ansi {
    constexpr auto RESET   = "\033[0m";
    constexpr auto BOLD    = "\033[1m";
    constexpr auto DIM     = "\033[2m";
    constexpr auto CYAN    = "\033[36m";
    constexpr auto GREEN   = "\033[32m";
    constexpr auto YELLOW  = "\033[33m";
    constexpr auto RED     = "\033[31m";
    constexpr auto MAGENTA = "\033[35m";
    constexpr auto WHITE   = "\033[97m";
    constexpr auto GRAY    = "\033[90m";
}

TerminalRenderer::TerminalRenderer(bool color) : m_color(color) {}

std::string TerminalRenderer::colorize(std::string_view text, std::string_view code) const {
    if (!m_color) return std::string(text);
    return std::string(code) + std::string(text) + ansi::RESET;
}

std::string TerminalRenderer::statusColor(std::string_view status) const {
    if (status == "ok" || status == "yes" || status == "enabled" || status == "active")
        return colorize(status, ansi::GREEN);
    if (status == "warn" || status == "warning")
        return colorize(status, ansi::YELLOW);
    if (status == "no" || status == "disabled" || status == "error" || status == "missing")
        return colorize(status, ansi::RED);
    return colorize(status, ansi::CYAN);
}

void TerminalRenderer::header(std::string_view text) {
    std::cout << "\n"
              << colorize(std::string("  ") + std::string(text), std::string(ansi::BOLD) + ansi::CYAN)
              << "\n";
    rule();
}

void TerminalRenderer::section(std::string_view title) {
    std::cout << "\n"
              << colorize(std::string("  ") + std::string(title), std::string(ansi::BOLD) + ansi::WHITE)
              << "\n";
}

void TerminalRenderer::row(std::string_view key, std::string_view value) {
    std::cout << "  "
              << colorize(std::format("{:<{}}", key, KEY_WIDTH), ansi::GRAY)
              << "  " << value << "\n";
}

void TerminalRenderer::row(std::string_view key, std::string_view value, std::string_view status) {
    std::cout << "  "
              << colorize(std::format("{:<{}}", key, KEY_WIDTH), ansi::GRAY)
              << "  " << std::left << std::setw(20) << value
              << "  " << statusColor(status) << "\n";
}

void TerminalRenderer::blank() {
    std::cout << "\n";
}

void TerminalRenderer::rule() {
    std::cout << colorize("  " + std::string(54, '-'), ansi::DIM) << "\n";
}

void TerminalRenderer::info(std::string_view text) {
    std::cout << colorize("  ℹ  ", ansi::CYAN) << text << "\n";
}

void TerminalRenderer::warn(std::string_view text) {
    std::cout << colorize("  ⚠  ", ansi::YELLOW) << text << "\n";
}

void TerminalRenderer::error(std::string_view text) {
    std::cerr << colorize("  ✗  ", ansi::RED) << text << "\n";
}

void TerminalRenderer::success(std::string_view text) {
    std::cout << colorize("  ✓  ", ansi::GREEN) << text << "\n";
}

} // namespace nicx::output
