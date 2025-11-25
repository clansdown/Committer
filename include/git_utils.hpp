#pragma once

#include <string>

class GitUtils {
public:
    static bool is_git_repo();
    static std::string get_repo_root();
    static std::string get_diff();
    static void add_files();
    static void commit(const std::string& message);
};