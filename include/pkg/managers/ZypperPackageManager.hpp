#pragma once
#include "pkg/IPackageManager.hpp"

namespace nicx::pkg {

class ZypperPackageManager final : public IPackageManager {
public:
    std::string_view id()          const override { return "zypper"; }
    std::string_view displayName() const override { return "Zypper (openSUSE)"; }

    int install(const std::vector<std::string>& packages) override;
    int remove(const std::vector<std::string>& packages) override;
    int update() override;
    std::vector<SearchResult> search(std::string_view query) override;
    bool isInstalled(std::string_view package) override;
};

} // namespace nicx::pkg
