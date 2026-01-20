#pragma once

#include <git2.h>
#include <string>
#include <vector>
#include <utility>

class GitRepository {
public:
    GitRepository();
    ~GitRepository();
    git_repository* get_repo() const { return repo_; }
    std::string get_repo_root() const { return repo_root_; }
    std::string get_commit_dir() const { return commit_dir_; }
private:
    git_repository* repo_;
    std::string repo_root_;
    std::string commit_dir_;
};

class GitUtils {
public:
    GitUtils(GitRepository& repo);
    static bool is_git_repo();
    static std::string get_repo_root();
    static std::string get_cached_git_dir();
    std::string get_diff(bool cached = true);
    std::string get_full_diff();
    std::vector<std::string> get_unstaged_files();
    std::vector<std::string> get_tracked_modified_files();
    std::vector<std::string> get_untracked_files();
    void add_files();
    void add_files(const std::vector<std::string>& files);
    void commit(const std::string& message);
    std::pair<std::string, std::string> commit_with_output(const std::string& message);
    void push();
private:
    static std::string cached_repo_root_;
    static std::string cached_git_dir_;
    GitRepository& repo_;
};