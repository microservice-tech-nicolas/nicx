#include "pkg/managers/PacmanPackageManager.hpp"
#include <cstdlib>
#include <sstream>
#include <array>
#include <cstdio>

namespace nicx::pkg {

static std::string capture(const std::string& cmd) {
    std::array<char, 512> buf;
    std::string out;
    FILE* p = popen(cmd.c_str(), "r");
    if (!p) return {};
    while (fgets(buf.data(), buf.size(), p)) out += buf.data();
    pclose(p);
    return out;
}

int PacmanPackageManager::install(const std::vector<std::string>& packages) {
    std::string cmd = "sudo " + binary() + " " + installFlags();
    for (const auto& p : packages) cmd += " " + p;
    return std::system(cmd.c_str());
}

int PacmanPackageManager::remove(const std::vector<std::string>& packages) {
    std::string cmd = "sudo " + binary() + " -Rns --noconfirm";
    for (const auto& p : packages) cmd += " " + p;
    return std::system(cmd.c_str());
}

int PacmanPackageManager::update() {
    std::string cmd = (binary() == "yay")
        ? "yay -Syu --noconfirm"
        : "sudo pacman -Syu --noconfirm";
    return std::system(cmd.c_str());
}

std::vector<SearchResult> PacmanPackageManager::search(std::string_view query) {
    std::vector<SearchResult> results;
    // pacman/yay -Ss outputs pairs of lines:
    // repo/name version [installed]
    //     description
    std::string cmd = binary() + " -Ss " + std::string(query) + " 2>/dev/null";
    std::string raw = capture(cmd);

    std::istringstream ss(raw);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.empty() || line.front() == ' ') continue; // skip desc lines

        // "core/bash 5.2.015-1 [installed]"
        auto slash = line.find('/');
        auto space = line.find(' ', slash != std::string::npos ? slash : 0);
        if (slash == std::string::npos || space == std::string::npos) continue;

        std::string name    = line.substr(slash + 1, space - slash - 1);
        std::string rest    = line.substr(space + 1);
        auto sp2            = rest.find(' ');
        std::string version = (sp2 != std::string::npos) ? rest.substr(0, sp2) : rest;
        bool installed      = line.find("[installed]") != std::string::npos;

        // grab description from next line
        std::string desc;
        std::string descLine;
        if (std::getline(ss, descLine)) {
            desc = descLine;
            desc.erase(0, desc.find_first_not_of(" \t"));
        }

        results.push_back({name, version, desc, installed});
    }
    return results;
}

bool PacmanPackageManager::isInstalled(std::string_view package) {
    std::string cmd = binary() + " -Q " + std::string(package) + " >/dev/null 2>&1";
    return std::system(cmd.c_str()) == 0;
}

} // namespace nicx::pkg
