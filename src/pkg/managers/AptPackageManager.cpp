#include "pkg/managers/AptPackageManager.hpp"
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

int AptPackageManager::install(const std::vector<std::string>& packages) {
    std::string cmd = "sudo apt-get install -y";
    for (const auto& p : packages) cmd += " " + p;
    return std::system(cmd.c_str());
}

int AptPackageManager::remove(const std::vector<std::string>& packages) {
    std::string cmd = "sudo apt-get remove -y";
    for (const auto& p : packages) cmd += " " + p;
    return std::system(cmd.c_str());
}

int AptPackageManager::update() {
    return std::system("sudo apt-get update && sudo apt-get upgrade -y");
}

std::vector<SearchResult> AptPackageManager::search(std::string_view query) {
    std::vector<SearchResult> results;
    // apt-cache search returns: "name - description"
    std::string cmd = "apt-cache search " + std::string(query) + " 2>/dev/null";
    std::string raw = capture(cmd);

    // Get installed packages for cross-reference
    std::string instRaw = capture("dpkg --get-selections 2>/dev/null");

    std::istringstream ss(raw);
    std::string line;
    while (std::getline(ss, line)) {
        auto dash = line.find(" - ");
        if (dash == std::string::npos) continue;
        std::string name = line.substr(0, dash);
        std::string desc = line.substr(dash + 3);
        bool installed   = instRaw.find(name + "\t") != std::string::npos;
        results.push_back({name, "", desc, installed});
    }
    return results;
}

bool AptPackageManager::isInstalled(std::string_view package) {
    std::string cmd = "dpkg -s " + std::string(package) + " >/dev/null 2>&1";
    return std::system(cmd.c_str()) == 0;
}

} // namespace nicx::pkg
