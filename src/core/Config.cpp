#include "core/Config.hpp"
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace nicx::core {

Config::Config() {
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    const char* home = std::getenv("HOME");

    if (xdg && *xdg) {
        m_configDir = std::filesystem::path(xdg) / "nicx";
    } else if (home && *home) {
        m_configDir = std::filesystem::path(home) / ".config" / "nicx";
    } else {
        m_configDir = "/tmp/nicx";
    }

    m_configFile = m_configDir / "config.toml";
}

Config& Config::instance() {
    static Config inst;
    return inst;
}

std::filesystem::path Config::configDir() const { return m_configDir; }
std::filesystem::path Config::configFile() const { return m_configFile; }

void Config::ensureConfigDir() const {
    std::filesystem::create_directories(m_configDir);
}

void Config::writeDefaults() const {
    std::ofstream f(m_configFile);
    f << "# nicx configuration\n"
      << "# https://github.com/microservice-tech-nicolas/nicx\n\n"
      << "[output]\n"
      << "color = true\n"
      << "# format = terminal  # terminal | plain | json\n\n"
      << "[system]\n"
      << "# show_swap = true\n";
}

void Config::load() {
    ensureConfigDir();

    if (!std::filesystem::exists(m_configFile)) {
        writeDefaults();
        return;
    }

    std::ifstream f(m_configFile);
    if (!f) return;

    std::string section;
    std::string line;
    while (std::getline(f, line)) {
        // Strip comments and trim
        if (auto pos = line.find('#'); pos != std::string::npos)
            line = line.substr(0, pos);
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t\r") + 1);
        if (line.empty()) continue;

        // Section header
        if (line.front() == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
            continue;
        }

        // key = value
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        auto key = line.substr(0, eq);
        auto val = line.substr(eq + 1);
        key.erase(key.find_last_not_of(" \t") + 1);
        val.erase(0, val.find_first_not_of(" \t"));

        std::string full_key = section.empty() ? key : section + "." + key;
        m_values[full_key] = val;
    }
}

void Config::save() const {
    ensureConfigDir();
    writeDefaults();
}

std::optional<std::string> Config::get(std::string_view key) const {
    auto it = m_values.find(std::string(key));
    if (it == m_values.end()) return std::nullopt;
    return it->second;
}

void Config::set(std::string_view key, std::string value) {
    m_values[std::string(key)] = std::move(value);
}

} // namespace nicx::core
