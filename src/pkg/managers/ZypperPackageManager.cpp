#include "pkg/managers/ZypperPackageManager.hpp"
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

int ZypperPackageManager::install(const std::vector<std::string>& packages) {
    std::string cmd = "sudo zypper install -y";
    for (const auto& p : packages) cmd += " " + p;
    return std::system(cmd.c_str());
}

int ZypperPackageManager::remove(const std::vector<std::string>& packages) {
    std::string cmd = "sudo zypper remove -y";
    for (const auto& p : packages) cmd += " " + p;
    return std::system(cmd.c_str());
}

int ZypperPackageManager::update() {
    return std::system("sudo zypper update -y");
}

std::vector<SearchResult> ZypperPackageManager::search(std::string_view query) {
    std::vector<SearchResult> results;
    // zypper search -s outputs a table; skip header lines (start with "S |")
    std::string cmd = "zypper --no-color search " + std::string(query) + " 2>/dev/null";
    std::string raw = capture(cmd);

    std::istringstream ss(raw);
    std::string line;
    bool pastHeader = false;
    while (std::getline(ss, line)) {
        if (line.find("--+--") != std::string::npos) { pastHeader = true; continue; }
        if (!pastHeader) continue;

        // "i | neovim | editor | 0.9.0 | x86_64"
        auto split = [](const std::string& s, char delim) {
            std::vector<std::string> parts;
            std::istringstream ss(s);
            std::string tok;
            while (std::getline(ss, tok, delim)) {
                tok.erase(0, tok.find_first_not_of(' '));
                tok.erase(tok.find_last_not_of(' ') + 1);
                parts.push_back(tok);
            }
            return parts;
        };

        auto parts = split(line, '|');
        if (parts.size() < 4) continue;

        bool installed = parts[0] == "i" || parts[0] == "i+";
        std::string name = parts[1];
        std::string desc = parts.size() > 2 ? parts[2] : "";
        std::string ver  = parts.size() > 3 ? parts[3] : "";

        if (!name.empty())
            results.push_back({name, ver, desc, installed});
    }
    return results;
}

bool ZypperPackageManager::isInstalled(std::string_view package) {
    std::string cmd = "rpm -q " + std::string(package) + " >/dev/null 2>&1";
    return std::system(cmd.c_str()) == 0;
}

} // namespace nicx::pkg
