#pragma once

#include <string>
#include <filesystem>
#include <unordered_map>
#include <optional>

namespace nicx::core {

// Config owns all user settings loaded from ~/.config/nicx/config.toml.
// Single Responsibility: only config I/O and key lookup.
class Config {
public:
    static Config& instance();

    void load();
    void save() const;

    std::optional<std::string> get(std::string_view key) const;
    void set(std::string_view key, std::string value);

    std::filesystem::path configDir() const;
    std::filesystem::path configFile() const;

private:
    Config();
    void ensureConfigDir() const;
    void writeDefaults() const;

    std::filesystem::path m_configDir;
    std::filesystem::path m_configFile;
    std::unordered_map<std::string, std::string> m_values;
};

} // namespace nicx::core
