#include "git/RepoStore.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace nicx::git {

static std::string trim(std::string s) {
    s.erase(0, s.find_first_not_of(" \t\r\n"));
    auto end = s.find_last_not_of(" \t\r\n");
    if (end != std::string::npos) s.erase(end + 1);
    return s;
}

static std::string unquote(std::string s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
        return s.substr(1, s.size() - 2);
    return s;
}

RepoStore::RepoStore(std::filesystem::path file) : m_file(std::move(file)) {}

const std::vector<TrackedRepo>& RepoStore::all() const { return m_repos; }

void RepoStore::load() {
    m_repos.clear();
    if (!std::filesystem::exists(m_file)) return;

    std::ifstream f(m_file);
    if (!f) return;

    std::string line;
    TrackedRepo* current = nullptr;

    while (std::getline(f, line)) {
        if (auto p = line.find('#'); p != std::string::npos) line = line.substr(0, p);
        line = trim(line);
        if (line.empty()) continue;

        if (line == "[[repo]]") {
            m_repos.emplace_back();
            current = &m_repos.back();
            continue;
        }
        if (!current) continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        auto key = trim(line.substr(0, eq));
        auto val = unquote(trim(line.substr(eq + 1)));

        if      (key == "name")      current->name      = val;
        else if (key == "org")       current->org       = val;
        else if (key == "url")       current->url       = val;
        else if (key == "localPath") current->localPath = val;
        else if (key == "notes")     current->notes     = val;
    }

    m_repos.erase(
        std::remove_if(m_repos.begin(), m_repos.end(),
            [](const TrackedRepo& r) { return r.name.empty() || r.url.empty(); }),
        m_repos.end());
}

void RepoStore::save() const {
    std::filesystem::create_directories(m_file.parent_path());
    std::ofstream f(m_file);
    f << "# nicx tracked repositories\n"
      << "# Managed by 'nicx git' — do not edit while nicx is running\n\n";

    for (const auto& r : m_repos) {
        f << "[[repo]]\n";
        f << "name      = \"" << r.name      << "\"\n";
        f << "org       = \"" << r.org       << "\"\n";
        f << "url       = \"" << r.url       << "\"\n";
        if (!r.localPath.empty())
            f << "localPath = \"" << r.localPath << "\"\n";
        if (!r.notes.empty())
            f << "notes     = \"" << r.notes     << "\"\n";
        f << "\n";
    }
}

void RepoStore::add(TrackedRepo repo) {
    for (auto& r : m_repos) {
        if (r.name == repo.name) { r = std::move(repo); return; }
    }
    m_repos.push_back(std::move(repo));
}

void RepoStore::remove(std::string_view name) {
    m_repos.erase(
        std::remove_if(m_repos.begin(), m_repos.end(),
            [&](const TrackedRepo& r) { return r.name == name; }),
        m_repos.end());
}

std::optional<TrackedRepo> RepoStore::find(std::string_view name) const {
    for (const auto& r : m_repos)
        if (r.name == name) return r;
    return std::nullopt;
}

} // namespace nicx::git
