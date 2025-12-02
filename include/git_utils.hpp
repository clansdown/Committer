#pragma once

#include <git2.h>
#include <string>
#include <vector>
#include <utility>

class GitUtils {
public:
    static bool is_git_repo();
    static std::string get_repo_root();
    static std::string get_diff(bool cached = true);
    static std::string get_full_diff();
    static std::vector<std::string> get_unstaged_files();
    static std::vector<std::string> get_tracked_modified_files();
    static std::vector<std::string> get_untracked_files();
    static void add_files();
    static void add_files(const std::vector<std::string>& files);
    static void commit(const std::string& message);
    static std::pair<std::string, std::string> commit_with_output(const std::string& message);
    static void push();
};