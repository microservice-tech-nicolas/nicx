#include "pkg/PackageStore.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

namespace nicx::pkg {

// ── Simple TOML-subset writer/reader for packages.toml ─────────────────────
// Format:
//
// [[package]]
// name = "neovim"
// method = "pm"
// pm = "dnf"
// notes = "primary editor"
//
// [[package]]
// name = "yay"
// method = "manual"
// pm = ""
// notes = "AUR helper, built from source"
// override.pacman = "yay"

PackageStore::PackageStore(std::filesystem::path file) : m_file(std::move(file)) {}

const std::vector<TrackedPackage>& PackageStore::all() const { return m_packages; }

static std::string trim(std::string s) {
    s.erase(0, s.find_first_not_of(" \t\r\n"));
    s.erase(s.find_last_not_of(" \t\r\n") + 1);
    return s;
}

static std::string unquote(std::string s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
        return s.substr(1, s.size() - 2);
    return s;
}

void PackageStore::load() {
    m_packages.clear();
    if (!std::filesystem::exists(m_file)) return;

    std::ifstream f(m_file);
    if (!f) return;

    std::string line;
    TrackedPackage* current = nullptr;

    while (std::getline(f, line)) {
        // strip comments
        if (auto p = line.find('#'); p != std::string::npos) line = line.substr(0, p);
        line = trim(line);
        if (line.empty()) continue;

        if (line == "[[package]]") {
            m_packages.emplace_back();
            current = &m_packages.back();
            continue;
        }

        if (!current) continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        auto key = trim(line.substr(0, eq));
        auto val = unquote(trim(line.substr(eq + 1)));

        if (key == "name")   { current->name   = val; }
        else if (key == "method") {
            current->method = (val == "manual") ? InstallMethod::Manual
                                                : InstallMethod::PackageManager;
        }
        else if (key == "pm")    { current->pmId   = val; }
        else if (key == "notes") { current->notes  = val; }
        else if (key.rfind("override.", 0) == 0) {
            std::string pmId = key.substr(9);
            current->overrides.push_back({pmId, val});
        }
    }

    // Remove any empty entries that crept in
    m_packages.erase(
        std::remove_if(m_packages.begin(), m_packages.end(),
            [](const TrackedPackage& p) { return p.name.empty(); }),
        m_packages.end());
}

void PackageStore::save() const {
    std::filesystem::create_directories(m_file.parent_path());
    std::ofstream f(m_file);
    f << "# nicx tracked packages\n"
      << "# Managed by 'nicx pkg' — do not edit while nicx is running\n\n";

    for (const auto& pkg : m_packages) {
        f << "[[package]]\n";
        f << "name   = \"" << pkg.name << "\"\n";
        f << "method = \"" << (pkg.method == InstallMethod::Manual ? "manual" : "pm") << "\"\n";
        f << "pm     = \"" << pkg.pmId << "\"\n";
        if (!pkg.notes.empty())
            f << "notes  = \"" << pkg.notes << "\"\n";
        for (const auto& ov : pkg.overrides)
            f << "override." << ov.pmId << " = \"" << ov.name << "\"\n";
        f << "\n";
    }
}

void PackageStore::add(TrackedPackage pkg) {
    // Replace if already tracked
    for (auto& p : m_packages) {
        if (p.name == pkg.name) { p = std::move(pkg); return; }
    }
    m_packages.push_back(std::move(pkg));
}

void PackageStore::remove(std::string_view name) {
    m_packages.erase(
        std::remove_if(m_packages.begin(), m_packages.end(),
            [&](const TrackedPackage& p) { return p.name == name; }),
        m_packages.end());
}

std::optional<TrackedPackage> PackageStore::find(std::string_view name) const {
    for (const auto& p : m_packages)
        if (p.name == name) return p;
    return std::nullopt;
}

std::string PackageStore::resolveNameFor(const TrackedPackage& pkg,
                                         std::string_view pmId) const {
    for (const auto& ov : pkg.overrides)
        if (ov.pmId == pmId) return ov.name;
    return pkg.name; // default: canonical name
}

} // namespace nicx::pkg
