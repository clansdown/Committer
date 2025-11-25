#include "git_utils.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cstdio>
#include <memory>
#include <array>

bool GitUtils::is_git_repo() {
    return system("git rev-parse --git-dir > /dev/null 2>&1") == 0;
}

std::string GitUtils::get_repo_root() {
    std::array<char, 128> buffer;
    std::string result;
    auto deleter = [](FILE* f) { pclose(f); };
    std::unique_ptr<FILE, decltype(deleter)> pipe(popen("git rev-parse --show-toplevel 2>/dev/null", "r"), deleter);
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    if (!result.empty() && result.back() == '\n') result.pop_back();
    return result;
}

std::string GitUtils::get_diff() {
    std::array<char, 128> buffer;
    std::string result;
    auto deleter = [](FILE* f) { pclose(f); };
    std::unique_ptr<FILE, decltype(deleter)> pipe(popen("git diff --cached", "r"), deleter);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

void GitUtils::add_files() {
    system("git add .");
}

void GitUtils::commit(const std::string& message) {
    std::string cmd = "git commit -m \"" + message + "\"";
    system(cmd.c_str());
}