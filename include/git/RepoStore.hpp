#pragma once

#include <string>
#include <vector>
#include <optional>
#include <filesystem>

namespace nicx::git {

struct TrackedRepo {
    std::string name;         // short name / identifier
    std::string org;          // GitHub org or username
    std::string url;          // full clone URL (ssh or https)
    std::string localPath;    // override clone path (empty = auto from dev_dir/org/name)
    std::string notes;
};

// SRP: owns repos.toml — no git operations.
class RepoStore {
public:
    explicit RepoStore(std::filesystem::path file);

    void load();
    void save() const;

    void add(TrackedRepo repo);
    void remove(std::string_view name);
    std::optional<TrackedRepo> find(std::string_view name) const;
    const std::vector<TrackedRepo>& all() const;

private:
    std::filesystem::path m_file;
    std::vector<TrackedRepo> m_repos;
};

} // namespace nicx::git
