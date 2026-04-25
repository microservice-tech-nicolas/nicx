#include "pkg/managers/DnfPackageManager.hpp"
#include <cstdlib>
#include <sstream>
#include <array>
#include <cstdio>
#include <map>

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

int DnfPackageManager::install(const std::vector<std::string>& packages) {
    std::string cmd = "sudo dnf install -y";
    for (const auto& p : packages) cmd += " " + p;
    return std::system(cmd.c_str());
}

int DnfPackageManager::remove(const std::vector<std::string>& packages) {
    std::string cmd = "sudo dnf remove -y";
    for (const auto& p : packages) cmd += " " + p;
    return std::system(cmd.c_str());
}

int DnfPackageManager::update() {
    return std::system("sudo dnf upgrade -y");
}

std::vector<SearchResult> DnfPackageManager::search(std::string_view query) {
    std::vector<SearchResult> results;
    // Use repoquery for reliable parseable output: name|version|summary
    // Wildcard-wrap the query to get broader matches
    std::string pattern = "*" + std::string(query) + "*";
    std::string cmd = "dnf5 repoquery --queryformat=\"%{name}|%{version}|%{summary}\\n\" "
                      "\"" + pattern + "\" 2>/dev/null";
    std::string raw = capture(cmd);

    // Deduplicate: keep only latest version per name
    std::map<std::string, SearchResult> seen;

    std::istringstream ss(raw);
    std::string line;
    while (std::getline(ss, line)) {
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
            line.pop_back();
        if (line.empty()) continue;

        // Split on '|'
        auto p1 = line.find('|');
        if (p1 == std::string::npos) continue;
        auto p2 = line.find('|', p1 + 1);
        if (p2 == std::string::npos) continue;

        std::string name    = line.substr(0, p1);
        std::string version = line.substr(p1 + 1, p2 - p1 - 1);
        std::string desc    = line.substr(p2 + 1);

        if (name.empty()) continue;
        // Keep the entry (last version wins — dnf5 returns oldest first)
        seen[name] = {name, version, desc, isInstalled(name)};
    }

    results.reserve(seen.size());
    for (auto& [_, r] : seen) results.push_back(std::move(r));
    return results;
}

bool DnfPackageManager::isInstalled(std::string_view package) {
    std::string cmd = "rpm -q " + std::string(package) + " >/dev/null 2>&1";
    return std::system(cmd.c_str()) == 0;
}

} // namespace nicx::pkg
