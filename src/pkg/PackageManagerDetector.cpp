#include "pkg/PackageManagerDetector.hpp"
#include "pkg/managers/DnfPackageManager.hpp"
#include "pkg/managers/PacmanPackageManager.hpp"
#include "pkg/managers/AptPackageManager.hpp"
#include "pkg/managers/ZypperPackageManager.hpp"
#include <cstdlib>

namespace nicx::pkg {

static bool inPath(const char* bin) {
    std::string cmd = std::string("command -v ") + bin + " >/dev/null 2>&1";
    return std::system(cmd.c_str()) == 0;
}

std::unique_ptr<IPackageManager> PackageManagerDetector::detect() {
    if (inPath("yay"))    return std::make_unique<YayPackageManager>();
    if (inPath("pacman")) return std::make_unique<PacmanPackageManager>();
    if (inPath("dnf"))    return std::make_unique<DnfPackageManager>();
    if (inPath("apt"))    return std::make_unique<AptPackageManager>();
    if (inPath("zypper")) return std::make_unique<ZypperPackageManager>();
    return nullptr;
}

std::string PackageManagerDetector::detectId() {
    auto pm = detect();
    if (!pm) return "unknown";
    return std::string(pm->id());
}

} // namespace nicx::pkg
