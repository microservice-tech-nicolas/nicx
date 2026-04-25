#pragma once

#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <map>

namespace nicx::pkg {

// Per-package manager name override.
// Allows a package to be named "neovim" under dnf but "neovim-git" under yay.
struct PmOverride {
    std::string pmId;       // "dnf", "pacman", "apt", ...
    std::string name;       // package name on that PM
};

// How the package is managed.
enum class InstallMethod {
    PackageManager, // via detected system PM
    Manual,         // installed from source, script, etc — tracked only
};

struct TrackedPackage {
    std::string name;               // canonical name (user-chosen)
    InstallMethod method;
    std::string pmId;               // which PM installed it
    std::string notes;              // optional free-form notes
    std::vector<PmOverride> overrides; // per-PM name remapping
};

// SRP: owns the packages.toml file. Knows nothing about package managers.
class PackageStore {
public:
    explicit PackageStore(std::filesystem::path file);

    void load();
    void save() const;

    void add(TrackedPackage pkg);
    void remove(std::string_view name);
    std::optional<TrackedPackage> find(std::string_view name) const;
    const std::vector<TrackedPackage>& all() const;

    // Resolve the actual PM name for a tracked package given a PM id.
    std::string resolveNameFor(const TrackedPackage& pkg, std::string_view pmId) const;

private:
    std::filesystem::path m_file;
    std::vector<TrackedPackage> m_packages;
};

} // namespace nicx::pkg
