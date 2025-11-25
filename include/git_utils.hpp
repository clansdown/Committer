#pragma once

#include <string>
#include <vector>

class GitUtils {
public:
    static bool is_git_repo();
    static std::string get_repo_root();
    static std::string get_diff(bool cached = true);
    static std::string get_full_diff();
    static std::vector<std::string> get_unstaged_files();
    static void add_files();
    static void commit(const std::string& message);
};