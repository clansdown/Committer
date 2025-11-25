#include "git_utils.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cstdio>
#include <memory>
#include <array>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>

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

std::string GitUtils::get_diff(bool cached) {
    std::string cmd = "git diff";
    if (cached) cmd += " --cached";
    std::array<char, 128> buffer;
    std::string result;
    auto deleter = [](FILE* f) { pclose(f); };
    std::unique_ptr<FILE, decltype(deleter)> pipe(popen(cmd.c_str(), "r"), deleter);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

std::string GitUtils::get_full_diff() {
    std::array<char, 128> buffer;
    std::string result;
    auto deleter = [](FILE* f) { pclose(f); };
    std::unique_ptr<FILE, decltype(deleter)> pipe(popen("git diff HEAD", "r"), deleter);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

std::vector<std::string> GitUtils::get_unstaged_files() {
    std::array<char, 128> buffer;
    auto deleter = [](FILE* f) { pclose(f); };
    std::unique_ptr<FILE, decltype(deleter)> pipe(popen("git status --porcelain", "r"), deleter);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    std::vector<std::string> files;
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        std::string line = buffer.data();
        if (line.size() > 3 && ((line[0] == 'M' && line[1] == ' ') || (line[0] == '?' && line[1] == '?'))) {
            size_t space = line.find(' ', 2);
            if (space != std::string::npos) {
                std::string file = line.substr(space + 1);
                if (!file.empty() && file.back() == '\n') file.pop_back();
                files.push_back(file);
            }
        }
    }
    return files;
}

void GitUtils::add_files() {
    system("git add .");
}

void GitUtils::commit(const std::string& message) {
    pid_t pid = fork();
    if (pid == -1) {
        throw std::runtime_error("Fork failed");
    } else if (pid == 0) {
        // child
        char* args[] = {"git", "commit", "-m", const_cast<char*>(message.c_str()), nullptr};
        execvp("git", args);
        _exit(1); // if exec fails
    } else {
        // parent
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            throw std::runtime_error("Git commit failed");
        }
    }
}