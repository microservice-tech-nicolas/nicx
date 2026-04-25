#pragma once
#include "pkg/IPackageManager.hpp"

namespace nicx::pkg {

// Pacman implementation — also used as base by YayPackageManager.
class PacmanPackageManager : public IPackageManager {
public:
    std::string_view id()          const override { return "pacman"; }
    std::string_view displayName() const override { return "Pacman (Arch)"; }

    int install(const std::vector<std::string>& packages) override;
    int remove(const std::vector<std::string>& packages) override;
    int update() override;
    std::vector<SearchResult> search(std::string_view query) override;
    bool isInstalled(std::string_view package) override;

protected:
    virtual std::string binary() const { return "pacman"; }
    virtual std::string installFlags() const { return "-S --noconfirm"; }
};

// yay extends pacman — same interface, different binary, includes AUR.
class YayPackageManager final : public PacmanPackageManager {
public:
    std::string_view id()          const override { return "yay"; }
    std::string_view displayName() const override { return "yay (Arch AUR)"; }

protected:
    std::string binary()       const override { return "yay"; }
    std::string installFlags() const override { return "-S --noconfirm"; }
};

} // namespace nicx::pkg
