#pragma once

#include "pkg/IPackageManager.hpp"
#include <memory>

namespace nicx::pkg {

// SRP: only responsible for detecting and constructing the right PM.
class PackageManagerDetector {
public:
    // Returns the best available package manager, or nullptr if none found.
    // Priority: yay > pacman > dnf > apt > zypper
    static std::unique_ptr<IPackageManager> detect();

    // Returns the id of what would be detected, without constructing it.
    static std::string detectId();
};

} // namespace nicx::pkg
