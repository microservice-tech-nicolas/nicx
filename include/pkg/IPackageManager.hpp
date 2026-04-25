#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace nicx::pkg {

struct SearchResult {
    std::string name;
    std::string version;
    std::string description;
    bool installed;
};

// Abstract package manager — DIP: PkgCommand depends on this interface,
// not on any concrete distro implementation.
class IPackageManager {
public:
    virtual ~IPackageManager() = default;

    virtual std::string_view id() const = 0;         // "dnf", "pacman", "apt", "yay"
    virtual std::string_view displayName() const = 0; // "DNF", "Pacman", "APT", "yay (AUR)"

    virtual int install(const std::vector<std::string>& packages) = 0;
    virtual int remove(const std::vector<std::string>& packages) = 0;
    virtual int update() = 0;
    virtual std::vector<SearchResult> search(std::string_view query) = 0;
    virtual bool isInstalled(std::string_view package) = 0;
};

} // namespace nicx::pkg
